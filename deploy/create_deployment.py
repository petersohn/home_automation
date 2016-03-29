#!/usr/bin/env python3

import argparse
import git
import os
import tarfile


def download_dependencies():
    pass


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
