#!/usr/bin/env bash

source_dir="$1"

while read -r file; do
    filename=$(basename $file)
    case "$file" in
    *.c)
        echo ": |> ^ CC ${file}^ \$(PRINT_JSON) ${file} \$(CC) \$(C_FLAGS) \$(INCLUDE_DIRS) -o %o ${file} |> ${filename}.o | ${filename}.compile.json"
        ;;
    *.cpp)
        echo ": |> ^ CXX ${file}^ \$(PRINT_JSON) ${file} \$(CXX) \$(CXX_FLAGS) \$(INCLUDE_DIRS) -o %o ${file} |> ${filename}.o | ${filename}.compile.json"
        ;;
    *.S)
        echo ": |> ^ ASS ${file}^ \$(PRINT_JSON) ${file} \$(CC) \$(ASS_FLAGS) \$(INCLUDE_DIRS) -o %o ${file} |> ${filename}.o | ${filename}.compile.json"
        ;;
    esac

done < <(find "$source_dir" -maxdepth 1 -name '*.c' -o -name '*.cpp' -o -name '*.S')
