#!/usr/bin/env python3

import argparse
import git
import os
import re
import sys
import tarfile
import tempfile
from urllib.request import Request, urlopen
import zipfile


SERVER_DIR = "server"
DATA_DIR = "deploy/data"
DJANGO_PROJECT_DIR = SERVER_DIR + "/home_automation"
STATIC_FILES_DIR = DJANGO_PROJECT_DIR + "/home/static/home"
INSTALLATION_DIR = "home/home_automation"


def download_file(url):
    sys.stderr.write("Downloading " + url + "\n")
    with urlopen(Request(url, headers={"User-Agent": "Foobar"})) as response:
        if response.status != 200:
            raise RuntimeError("Got HTTP error: " + response.msg)
        length = int(response.getheader("Content-Length"))
        amount = 1024 * 4
        target = tempfile.TemporaryFile()
        downloaded = 0
        if length is not None:
            sys.stderr.write("0.0%\r")
        try:
            while True:
                data = response.read(amount)
                if len(data) == 0:
                    target.seek(0)
                    return target
                target.write(data)
                downloaded += len(data)
                if length is not None:
                    sys.stderr.write(
                        "{:.1f}%\r".format(downloaded / length * 100))
                else:
                    sys.stderr.write(".")
        finally:
            sys.stderr.write("\n")


def process_zip(name, target_dir, new_name):
    target_path = target_dir + "/" + new_name
    if not os.path.exists(target_path):
        with download_file(
                "http://jqueryui.com/resources/download/" +
                name + ".zip") as f:
            with zipfile.ZipFile(f) as zip:
                zip.extractall(target_dir)
                if new_name is not None:
                    os.rename(target_dir + "/" + name, target_path)


def download_dependencies():
    global STATIC_FILES_DIR
    process_zip("jquery-ui-1.11.4", STATIC_FILES_DIR, "jquery-ui")
    process_zip(
        "jquery-ui-themes-1.11.4", STATIC_FILES_DIR, "jquery-ui-themes")


def calculate_prefix(path):
    basename = os.path.basename(path)
    return re.sub("^(.*)\\.((tar.*)|tgz)", "\\1", basename)


def create_archive(filename):
    compression = ""
    match = re.search("\\.tar\\.(\w+)$", filename)
    if match is not None:
        compression = match.group(1)
    elif filename.endswith(".tgz"):
        compression = "gz"
    return tarfile.open(filename, "w:" + compression)


def add_server_files(archive, repo):
    def needs_to_be_included(o, d):
        return o.name.endswith(".py") or o.name.endswith(".html")

    def filter(tarinfo):
        global INSTALLATION_DIR
        tarinfo.name = INSTALLATION_DIR + "/" + tarinfo.name
        return tarinfo

    for object in repo.tree()["server"].traverse(
            prune=lambda o, d: o.name == "test",
            predicate=needs_to_be_included):
        archive.add(object.path, filter=filter)
    archive.add(STATIC_FILES_DIR, filter=filter)


def add_config_files(archive):
    global DATA_DIR
    archive.addfile(
        archive.gettarinfo(
            DATA_DIR + "/lighttpd.conf",
            arcname="/etc/lighttpd/lighttpd.conf"))
    archive.addfile(
        archive.gettarinfo(
            DATA_DIR + "/home_automation.service",
            arcname="/usr/lib/systemd/system/home_automation.service"))


def add_files(archive, prefix, repo):
    global STATIC_FILES_DIR

    with tempfile.TemporaryFile() as file:
        inner_archive = tarfile.open(mode="w", fileobj=file)
        add_server_files(inner_archive, repo)
        add_config_files(inner_archive)
        inner_archive.close()
        file.seek(0)
        archive.addfile(
            archive.gettarinfo(
                arcname=prefix + "/package.tar", fileobj=file),
            fileobj=file)


def get_data_dir_filter(prefix):
    def filter(tarinfo):
        global DATA_DIR
        tarinfo.name = tarinfo.name.replace(DATA_DIR, prefix)
        return tarinfo

    return filter


def add_upgrade_files(archive, prefix):
    filter = get_data_dir_filter(prefix)
    archive.add(DATA_DIR + "/common.sh", filter=filter)
    archive.add(DATA_DIR + "/upgrade.sh", filter=filter)


def add_install_files(archive, prefix):
    filter = get_data_dir_filter(prefix)
    archive.add(DATA_DIR + "/common.sh", filter=filter)
    archive.add(DATA_DIR + "/install.sh", filter=filter)


def main():
    parser = argparse.ArgumentParser(
            description="Create deployment package.")
    parser.add_argument(
        "--type", nargs='?', choices=["install", "upgrade", "download"],
        default="install",
        help="The type of the package. 'install' is a full installation "
             "package and 'upgrade' is an upgrade package. 'download' only "
             "downloads dependencies into the local repository.")
    parser.add_argument(
        "--source", nargs='?', default=".",
        help="The directory to get the files from. It must be a git "
             "repository for home automation.")
    parser.add_argument(
        "--output", nargs='?', default="home_automation.tar.gz",
        help="The name of the output archive. No archive is generated if "
             "--type=download.")
    arguments = parser.parse_args()
    arguments.output = os.path.abspath(arguments.output)
    repo = git.Repo(search_parent_directories=True)
    os.chdir(repo.working_tree_dir)

    download_dependencies()
    if arguments.type != "download":
        archive = create_archive(arguments.output)
        prefix = calculate_prefix(arguments.output)
        add_files(archive, prefix, repo)
        if arguments.type == "install":
            add_install_files(archive, prefix)
        else:
            add_upgrade_files(archive, prefix)

        archive.close()


if __name__ == "__main__":
    main()
