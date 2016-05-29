#!/usr/bin/env python3

import argparse
import git
import os
import os.path
import re
import shlex
import sys
import tarfile
import tempfile
from urllib.request import Request, urlopen
import zipfile


SERVER_DIR = "server"
DATA_DIR = "deploy/data"
DJANGO_PROJECT_DIR = SERVER_DIR + "/home_automation"
STATIC_FILES_DIR = DJANGO_PROJECT_DIR + "/home/static/home"
VISJS_DIR = STATIC_FILES_DIR + "/visjs"
INSTALLATION_DIR = "home/home_automation"


def download_file(url, target=tempfile.TemporaryFile()):
    sys.stderr.write("Downloading " + url + "\n")
    with urlopen(Request(url, headers={"User-Agent": "Foobar"})) as response:
        if response.status != 200:
            raise RuntimeError("Got HTTP error: " + response.msg)
        length_header = response.getheader("Content-Length")
        if length_header is not None:
            length = int()
        else:
            length = None
        amount = 1024 * 4
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
    target_path = os.path.join(target_dir, new_name)
    if not os.path.exists(target_path):
        with download_file(
                "http://jqueryui.com/resources/download/" +
                name + ".zip") as f:
            with zipfile.ZipFile(f) as zip:
                zip.extractall(target_dir)
                if new_name is not None:
                    os.rename(os.path.join(target_dir, name), target_path)


def download_file_to_target(url, target):
    if not os.path.exists(target):
        download_file(url, open(target, mode="wb"))


def download_dependencies():
    global STATIC_FILES_DIR
    process_zip("jquery-ui-1.11.4", STATIC_FILES_DIR, "jquery-ui")
    process_zip(
        "jquery-ui-themes-1.11.4", STATIC_FILES_DIR, "jquery-ui-themes")
    os.makedirs(VISJS_DIR, exist_ok=True)
    download_file_to_target(
        "https://cdnjs.cloudflare.com/ajax/libs/vis/4.16.1/vis.min.js",
        os.path.join(VISJS_DIR, "vis.min.js"))
    download_file_to_target(
        "https://cdnjs.cloudflare.com/ajax/libs/vis/4.16.1/vis.min.css",
        os.path.join(VISJS_DIR, "vis.min.css"))


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


def repo_file_filter(tarinfo):
    global INSTALLATION_DIR
    tarinfo.name = os.path.join(INSTALLATION_DIR, tarinfo.name)
    return tarinfo


def add_server_files(archive, repo):
    def needs_to_be_included(o, d):
        return o.name.endswith(".py") or o.name.endswith(".html")

    for object in repo.tree()["server"].traverse(
            prune=lambda o, d: o.name == "test",
            predicate=needs_to_be_included):
        archive.add(object.path, filter=repo_file_filter)
    archive.add(STATIC_FILES_DIR, filter=repo_file_filter)


def add_all_files_from_repo(archive, repo, path):
    for object in repo.tree()[path].traverse():
        archive.add(object.path, filter=repo_file_filter)


def add_file(archive, source, target):
    if type(source) is str:
        tarinfo = archive.gettarinfo(source, arcname=target)
        archive.addfile(tarinfo, fileobj=open(source, "rb"))
    else:
        tarinfo = archive.gettarinfo(fileobj=source, arcname=target)
        archive.addfile(tarinfo, fileobj=source)


def add_data_file(archive, name, path, fileobj=None):
    global DATA_DIR
    target_path = os.path.join(path, name)
    if fileobj is None:
        filename = os.path.join(DATA_DIR, name)
        add_file(archive, filename, target_path)
    else:
        add_file(archive, fileobj, target_path)


def concatenate_data_files(*names):
    global DATA_DIR
    result = tempfile.TemporaryFile()
    for name in names:
        with open(os.path.join(DATA_DIR, name), "rb") as file:
            result.write(file.read())
    result.seek(0)
    return result


def add_data_files(archive, arguments):
    if arguments.system == "systemd":
        add_data_file(
            archive, "home_automation.service", "usr/lib/systemd/system")
    else:
        add_data_file(
            archive, "home_automation.conf", "/etc/init")
    add_data_file(archive, "home_automation_manage", "/usr/local/bin")
    if arguments.device is not None:
        lighttpd_conf_file = concatenate_data_files(
            "lighttpd.conf", "lighttpd.conf.device")
        add_file(
            archive, arguments.device,
            "/home/home_automation/device/pi/config.json")
    else:
        lighttpd_conf_file = None
    add_data_file(archive, "lighttpd.conf", "etc/lighttpd",
                  fileobj=lighttpd_conf_file)


def add_files(archive, prefix, repo, arguments):
    global STATIC_FILES_DIR

    with tempfile.TemporaryFile() as file:
        inner_archive = tarfile.open(mode="w", fileobj=file)
        add_server_files(inner_archive, repo)
        add_all_files_from_repo(inner_archive, repo, "python")
        if arguments.device is not None:
            add_all_files_from_repo(inner_archive, repo, "device/pi")
        add_data_files(inner_archive, arguments)
        inner_archive.close()
        file.seek(0)
        archive.addfile(
            archive.gettarinfo(
                arcname=prefix + "/package.tar", fileobj=file),
            fileobj=file)


def add_install_file(archive, prefix, name):
    def filter(tarinfo):
        global DATA_DIR
        tarinfo.name = tarinfo.name.replace(DATA_DIR, prefix)
        return tarinfo

    archive.add(os.path.join(DATA_DIR, name), filter=filter)


def add_environment_file(archive, prefix, environment):
    with tempfile.TemporaryFile() as file:
        for key, value in environment.items():
            data = key + "=" + shlex.quote(value) + "\n"
            file.write(data.encode("UTF-8"))
        file.seek(0)
        archive.addfile(
            archive.gettarinfo(
                arcname=prefix + "/environment.sh", fileobj=file),
            fileobj=file)


def add_common_files(archive, prefix, environment):
    add_install_file(archive, prefix, "common.sh")
    add_install_file(archive, prefix, "get_django_path.py")
    add_environment_file(archive, prefix, environment)


def add_upgrade_files(archive, prefix, environment):
    add_common_files(archive, prefix, environment)
    add_install_file(archive, prefix, "upgrade.sh")


def add_install_files(archive, prefix, environment):
    add_common_files(archive, prefix, environment)
    add_install_file(archive, prefix, "install.sh")


def prepare_environment(arguments):
    result = {}
    result["has_device"] = "yes" if arguments.device else "no"
    result["system_type"] = arguments.system
    return result


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
        "--system", nargs='?', choices=["systemd", "upstart"],
        default="systemd",
        help="The type of service management on the target OS. Supported "
             "values are 'systemd' (default) or 'upstart'.")
    parser.add_argument(
        "--source", nargs='?', default=".",
        help="The directory to get the files from. It must be a git "
             "repository for home automation.")
    parser.add_argument(
        "--output", nargs='?', default="home_automation.tar.gz",
        help="The name of the output archive. No archive is generated if "
             "--type=download.")
    parser.add_argument(
        "--device", nargs="?", default=None,
        help="Also install Raspberry Pi device. Use the argument as the "
             "config file.")
    arguments = parser.parse_args()
    arguments.output = os.path.abspath(arguments.output)
    if arguments.device is not None:
        arguments.device = os.path.abspath(arguments.device)
    repo = git.Repo(search_parent_directories=True)
    os.chdir(repo.working_tree_dir)

    download_dependencies()
    if arguments.type != "download":
        archive = create_archive(arguments.output)
        prefix = calculate_prefix(arguments.output)
        add_files(archive, prefix, repo, arguments)
        environment = prepare_environment(arguments)
        if arguments.type == "install":
            add_install_files(archive, prefix, environment)
        else:
            add_upgrade_files(archive, prefix, environment)

        archive.close()


if __name__ == "__main__":
    main()
