#!/usr/bin/env bash

set -ex

rm -rf /usr/local/lib
ln -s $VIRTUAL_ENV/lib /usr/local/lib
rm -rf /usr/local/bin
ln -s $VIRTUAL_ENV/bin /usr/local/bin


