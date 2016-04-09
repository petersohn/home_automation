#!/usr/bin/env bash

create_user() {
    addgroup --system home_automation
    adduser --system --ingroup home_automation home_automation
}

setup_psql() {
    sudo -u postgres createuser home_automation
    sudo -u postgres psql -c 'create database home_automation owner home_automation'
}

setup_services() {
    systemctl enable home_automation.service
}

script_dir=$(readlink -e "$(dirname "$0")")
source "$script_dir/common.sh"

set -e
create_user
setup_psql
unpack_files
setup_services
common_tasks