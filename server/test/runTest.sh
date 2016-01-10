#!/usr/bin/env bash

script_dir=$(dirname "$(which "$0")")

set -e
database_name="test_db_$(date '+%Y%m%d_%H%M%S%N')_$RANDOM"
psql --quiet --set 'ON_ERROR_STOP=true' postgres <<_EOF_
drop database if exists $database_name;
create database $database_name;
_EOF_
set +e

"${script_dir}/main.py" --connectString "dbname=$database_name" --testServerAddress "${TEST_SERVER_ADDRESS:-127.0.0.1:8080}" -- "$@"
result=$?

psql --quiet postgres <<_EOF_
drop database $database_name;
_EOF_

exit $result


