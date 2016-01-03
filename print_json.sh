#!/usr/bin/env bash

eliminateTupVariant() {
    topDir="$PWD"
    while [ "$PWD" != / -a ! -e ../.tup -a ! -e .tup ]; do
        cd ..
    done

    if [ -e tup.config ]; then
        configDir=$PWD
        cd ..
        echo "$PWD${topDir#$configDir}"
    else
        echo "$topDir"
    fi
}

findRealDirectory() {
    pushd . >/dev/null
    if pwd | grep --quiet /.tup/mnt/@tupjob-; then
        # get out from the tup virtual filesystem
        cd "/${PWD#*/.tup/mnt/@tupjob-*/}"

        # get out from the tup variant directory (if any)
        cd "$(eliminateTupVariant)"
    fi
}

file="$1"
shift

cat >"$(basename $file).compile.json" <<_EOF_
[{
    "directory": "$(findRealDirectory; pwd)",
    "command": "$*",
    "file": "$file"
}]
_EOF_

"$@"
