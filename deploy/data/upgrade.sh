#!/usr/bin/env bash

script_dir=$(readlink -e "$(dirname "$0")")
source "$script_dir/common.sh"

set -e
unpack_files
common_tasks