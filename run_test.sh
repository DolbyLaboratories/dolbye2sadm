#!/bin/bash

export exe_dir="./build_release"
export exe="$exe_dir/dolbye2sadm"

if [ ! -f $exe ]; then
        echo "Executable does not exist, rebuilding"
        if [ ! -d $exe_dir ]; then
  			conan install . --output-folder=$exe_dir --build=missing -s build_type=Release
  		fi
		cd $exe_dir
		if [ ! -f "Makefile" ]; then
			cmake -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
		fi
		make
		cd ..
fi

export test_dir="./test"
export dde_dir=$test_dir/dde
export reference_dir="./test/reference"

pass_num=0
fail_num=0

for xmlFile in $test_dir/*.xml ; do
	xmlFile_stem=`basename $xmlFile .xml`
	xmlFile_nodir=`basename $xmlFile`
	refFile=$reference_dir/$xmlFile_nodir
	ddeFile=$dde_dir/$xmlFile_stem.dde
	cmd="$exe $ddeFile $xmlFile"
	echo Executing... $cmd
	$cmd
	echo --------------------------------------------
	echo Validating $xmlFile...
	xmllint --xpath "//dbmd" $xmlFile | xmllint --schema "XML Schemas/dbmd_schema.xsd" --noout -
	echo --------------------------------------------

	if [ $? -eq "0" ]; then
		((pass_num++))
	else
		((fail_num++))
	fi

	echo Comparing $xmlFile with $refFile
	# remove flowID which will always be different and can be ignored
	sed 's/flowID=\"[^"]*\"/flowID=\"\"/g' $xmlFile > diff_file1.tmp
	sed 's/flowID=\"[^"]*\"/flowID=\"\"/g' $refFile > diff_file2.tmp
	diff diff_file1.tmp diff_file2.tmp > diffs.tmp

	if [ $? -eq "0" ]; then
		((pass_num++))
	else
		((fail_num++))
	fi
	rm -f diff_file1.tmp diff_file2.tmp diffs.tmp
done

echo "Number of passes: " $pass_num
echo "Number of failures: " $fail_num
if [ $fail_num -eq "0" ]; then
	echo "Overall Result: Pass"
	exit 0
else
	echo "Overall Result: Fail"
	exit 1
fi
