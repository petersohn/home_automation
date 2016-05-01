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

    if [[ $has_device == yes ]]; then
        set_permissions_to_directory "$device_dir"
    fi
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

restart_service() {
    if [[ $system_type == systemd ]]; then
        systemctl restart "${1}.service"
    else
        service "$1" restart
    fi
}

check_service() {
    if [[ $system_type == systemd ]]; then
        systemctl status "${1}.service"
    else
        service "$1" status
    fi
}

restart_services() {
    if [[ $system_type == systemd ]]; then
        systemctl daemon-reload
    fi
    restart_service lighttpd
    restart_service home_automation
}

check_services() {
    check_service lighttpd
    check_service home_automation
}

verify_installation() {
    check_services lighttpd
    check_services home_automation
}

common_tasks() {
    set_permissons
    create_symlinks
    migrate
    restart_services
    verify_installation
}
