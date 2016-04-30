#!/usr/bin/env bash

set -ex

python3 ./server/home_automation/manage.py makemigrations
./deploy/create_deployment.py --type=install --system=upstart
