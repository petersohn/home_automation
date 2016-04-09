#!/usr/bin/env bash

server_dir=/home/home_automation/server

set_permissons() {
    chown --recursive root:home_automation "$server_dir"
    chmod --recursive g-w,o-rwx "$server_dir"
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
    systemctl restart lighttpd.service
    systemctl restart home_automation.service
}

common_tasks() {
    set_permissons
    create_symlinks
    migrate
    restart_services
}
