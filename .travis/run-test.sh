#!/usr/bin/env bash

set -ex

cd ./server/home_automation
python3 ./manage.py makemigrations
python3 ./manage.py test
