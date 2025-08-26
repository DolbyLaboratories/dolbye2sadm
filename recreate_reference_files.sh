#!/bin/bash
export exe="./build_debug/dolbye2sadm"
export dde_dir="./test/dde"
export ref_dir="./test/reference"
echo $PWD
ls $exe
$exe $dde_dir/2+2-1.dde $ref_dir/2+2-1.xml
$exe $dde_dir/4x2-1.dde $ref_dir/4x2-1.xml
$exe $dde_dir/5.1+2-1.dde $ref_dir/5.1+2-1.xml
$exe $dde_dir/5.1+2-2.dde $ref_dir/5.1+2-2.xml
$exe $dde_dir/5.1-1.dde $ref_dir/5.1-1.xml
$exe $dde_dir/5.1-2.dde $ref_dir/5.1-2.xml

