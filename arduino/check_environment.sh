#!/usr/bin/env bash

shopt -s globstar

result=0
for variable in $(sed -rn 's/^export (.*)/\1/p' **/Tupfile **/*.tup); do
    if [[ -z ${!variable+unset} ]]; then
        echo "$variable is missing."
        result=2
    fi
done

exit $result
