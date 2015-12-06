#!/usr/bin/env bash

source "$(dirname "$(which $0)")/environment.sh"

write_variable() {
    variable="$1"
    if [[ -n ${!variable+unset} ]]; then
        echo "export ${variable}=\"${!variable}\"" >>"$filename"
    fi
}

if [ $# -eq 0 ]; then
    filename=load_environment.sh
else
    filename="$1"
fi

rm -f "$filename"
foreach_environment write_variable
