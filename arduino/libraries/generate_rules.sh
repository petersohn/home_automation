#!/usr/bin/env bash

source_dir="$1"

while read -r file; do
    case "$file" in
    *.c)
        echo ": |> ^ CC ${file}^ \$(CC) \$(C_FLAGS) \$(INCLUDE_DIRS) -o %o ${file} |> $(basename $file).o"
        ;;
    *.cpp)
        echo ": |> ^ CXX ${file}^ \$(CXX) \$(CXX_FLAGS) \$(INCLUDE_DIRS) -o %o ${file} |> $(basename $file).o"
        ;;
    *.S)
        echo ": |> ^ ASS ${file}^ \$(CC) \$(ASS_FLAGS) \$(INCLUDE_DIRS) -o %o ${file} |> $(basename $file).o"
        ;;
    esac

done < <(find "$source_dir" -maxdepth 1 -name '*.c' -o -name '*.cpp' -o -name '*.S')
