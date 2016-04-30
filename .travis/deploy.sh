#!/usr/bin/env bash

set -ex

mkdir -p custom/install
cd custom/install
tar -xf ../../home_automation.tar.gz
sudo bash -x ./home_automation/install.sh
