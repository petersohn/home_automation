#!/usr/bin/env bash

set -e
source_dir="$ESP_DIR/cores/esp8266"
core_dir="$(dirname "$(which "$0")")/core"
cp -t "$core_dir" "$source_dir"/*.S "$source_dir"/*.c "$source_dir"/*.cpp
tup "${core_dir}/core.a"

