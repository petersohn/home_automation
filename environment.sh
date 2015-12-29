#!/usr/bin/env bash

shopt -s globstar

foreach_environment() {
    for variable in $(sed -rn 's/^export (.*)/\1/p' **/Tupfile **/*.tup); do
        "$1" "$variable"
    done
}
