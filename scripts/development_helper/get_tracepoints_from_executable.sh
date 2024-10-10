#!/usr/bin/env bash
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

if [ $# -eq 0 ]
then
    echo "$0"
    echo ""
    echo "extract all clltk meta data sections from given files"
    echo ""
    echo "example:"
    echo " $0 some_file.so some_other_file.so"
    echo ""
    echo " with tracebuffer_xyz in some_file.so"
    echo " and with tracebuffer_xyz and tracebuffer_abc in some_other_file.so"
    echo ""
    echo " will generate:"
    echo "  - some_file.so.tracebuffer_xyz.clltk_meta_data_raw"
    echo "  - some_other_file.so.tracebuffer_xyz.clltk_meta_data_raw"
    echo "  - some_other_file.so.tracebuffer_abc.clltk_meta_data_raw"
else
    for file in "$@"
    do
        elf_section_names=$(readelf -SW $file)
        clltk_sections_name=$(grep -o '_clltk_[[:alnum:]_]*_meta' <<< $elf_section_names)

        if [ -z "$clltk_sections_name" ]
        then
            echo "{\"file\": \"$file\", \"error\": \"no clltk tracebuffer\"}"
            continue
        else
            for section in $clltk_sections_name
            do
                tracebuffer=$(grep -Po "(?<=_clltk_).*(?=_meta)" <<< $section)
                outputfile="${file}.${tracebuffer}.clltk_meta_data_raw"
                echo "{\"file\": \"$file\", \"tracebuffer\": \"$tracebuffer\", \"section\": \"$section\", \"outputfile\": \"${outputfile}\"}"
                objcopy -O binary --only-section=${section} ${file} ${outputfile}
            done
        fi

    done
fi