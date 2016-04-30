#!/usr/bin/env bash

create_link() {
    file="$1"
    source="$VIRTUAL_ENV/$file"
    target="/usr/local/$file"
    rm -rf "$target"
    cp -r "$source" "$target"
}

create_links() {
    for f in $(eval "echo $*"); do
        create_link "${f#$VIRTUAL_ENV}"
    done
}

set -ex

echo $VIRTUAL_ENV

create_links '$VIRTUAL_ENV/lib/python*'
create_links '$VIRTUAL_ENV/bin/python*'
create_links '$VIRTUAL_ENV/bin/pip*'

for file in /usr/local/bin/pip*; do
    version="${file##*pip}"
    sed -i "s|^#!.*$|#!/usr/local/bin/python${version}|" "$file"
done
