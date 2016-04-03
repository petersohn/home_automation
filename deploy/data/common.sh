#!/usr/bin/env bash

set_permissons() {
    chown --recursive root:home_automation /home/home_automation/server
    chmod --recursive 640 /home/home_automation/server
}

unpack_files() {
    tar -C / -xf "$script_dir/package.tar"
}

migrate() {
    python3 /home/home_automation/server/home_automation/manage.py makemigrations home
    python3 /home/home_automation/server/home_automation/manage.py migrate
}

restart_services() {
    systemctl restart lighttpd.service
    systemctl restart home_automation.service
}

common_tasks() {
    unpack_files
    set_permissons
    migrate
    restart_services
}
