#!/usr/bin/env python3

import argparse
import git
import os
import sys
import tarfile
import tempfile
from urllib.request import Request, urlopen
import zipfile


STATIC_FILES_DIR = "server/home_automation/home/static/home"


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


def create_archive(filename):
    pass


def add_upgrade_files(archive):
    pass


def add_upgrade_script(archive):
    pass


def add_install_files(archive):
    pass


def add_install_script(archive):
    pass


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
    repo = git.Repo(search_parent_directories=True)
    os.chdir(repo.working_tree_dir)

    download_dependencies()
    if arguments.type != "download":
        archive = create_archive(arguments.output)
        add_upgrade_files(archive)
        if arguments.type == "install":
            add_install_files(archive)
            add_install_script(archive)
        else:
            add_upgrade_script(archive)

        archive.close()


if __name__ == "__main__":
    main()
