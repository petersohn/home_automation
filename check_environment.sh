#!/usr/bin/env bash

source "$(dirname "$(which $0)")/environment.sh"

check_variable() {
    variable="$1"
    if [[ -z ${!variable+unset} ]]; then
        echo "$variable is missing."
        result=2
    fi
}

result=0
foreach_environment check_variable

exit $result
