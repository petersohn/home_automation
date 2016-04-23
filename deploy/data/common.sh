#!/usr/bin/env bash

source "$script_dir/environment.sh"

home_dir=/home/home_automation
server_dir="$home_dir/server"
device_dir="$home_dir/device"

set_permissions_to_directory() {
    chown --recursive root:home_automation "$1"
    chmod --recursive g-w,o-rwx "$1"
}

set_permissons() {
    set_permissions_to_directory "$server_dir"
    set_permissions_to_directory "$device_dir"
}

create_symlinks() {
    ln -fs "$("$script_dir/get_django_path.py")/contrib/admin/static/admin" "$server_dir/home_automation/home/static/"
}

unpack_files() {
    tar -C / -xf "$script_dir/package.tar"
}

migrate() {
    python3 "$server_dir/home_automation/manage.py" makemigrations home
    sudo -u home_automation python3 "$server_dir/home_automation/manage.py" migrate
}

restart_services() {
    systemctl daemon-reload
    systemctl restart lighttpd.service
    systemctl restart home_automation.service
}

verify_installation() {
    systemctl status lighttpd.service
    systemctl status home_automation.service
}

common_tasks() {
    set_permissons
    create_symlinks
    migrate
    restart_services
    verify_installation
}
