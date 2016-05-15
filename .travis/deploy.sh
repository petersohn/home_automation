#!/usr/bin/env bash

set -ex

./deploy/create_deployment.py --type=install --system=upstart
mkdir -p custom/install
cd custom/install
tar -xf ../../home_automation.tar.gz
sudo bash -x ./home_automation/install.sh
