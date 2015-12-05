#!/usr/bin/env bash

if [ $# -eq 0 ]; then
    filename=load_environment.sh
else
    filename="$1"
fi

variables=(ESP_DIR ESP_COMPILER_DIR ESPTOOL ESP_PORT)

rm -f "$filename"
for variable in "${variables[@]}"; do
    echo "${variable}=\"${!variable}\"" >>"$filename"
done
