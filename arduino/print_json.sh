#!/usr/bin/env bash

file="$1"
shift
cat >"${file}.json" <<_EOF_
[{
    "directory": "$PWD",
    "command": "$*",
    "file": "$file"
}]
_EOF_

"$@"
