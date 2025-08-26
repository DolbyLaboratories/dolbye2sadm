# dolbye2sadm

The purpose of this tool is to convert a packed Dolby E file (.dde) to Serialized ADM as per the new Dolby E S-ADM profile specified by Tim McNamara. To obtain a packed Dolby E file (.dde) from a WAV file, first use the frame337 tool 
https://github.com/DolbyLaboratories/frame337.

For more information about the Dolby E S-ADM profile see https://confluence.dolby.net/kb/x/c5xcHw

## Prerequisites

Install Cmake and Conan for your platform.

This project the package manager Conan to acquire the Boost library for the generation of uuids

See https://docs.conan.io/2/installation.html for information about installing conan

The first time conan is used it needs to be configured:
```
conan profile detect --force
```

## Building

On Mac and Linux you can use Conan and Cmake to generate a makefile like below from the root folder.

To create a Debug build:
```
conan install . --output-folder=build_debug --build=missing -s build_type=Debug          
cd build_debug
cmake -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```
To create a Release build
```
conan install . --output-folder=build_release --build=missing -s build_type=Release        
cd build_release
cmake -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
make
```


On Windows, it is recommended to use Microsoft build tools to generate a nmake Makefile. Make sure that the environment is correctly configured using vcvars64.bat or similar command file. Ensure that a resource compiler (rc.exe) is also in the path. This is available in Windows SDKs.

To create a Debug build on Windows (similar for Release)
```
conan install . -c tools.cmake.cmaketoolchain:generator="NMake Makefiles" --output-folder=build_debug --build=missing -s build_type=Debug
cmake -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091="NEW" -DCMAKE_BUILD_TYPE=Debug ..
nmake
```

## Running

The executable is in the build directory under samples.

Usage: dolbye2sadm infile.dde [outfile.xml]

The output file is optional. If it is not specified then the XML output will go to the console.

## Testing

Directory /test contains several Dolby E test files and a set of reference output S-ADM XML files that can be used to check for conformance.

Where more than one version exists for each supported Dolby E programme configuration (5.1 and 5.1+2), the second version
excercises a different setting for ac3_acmod. See the implementation guide for further details regarding use cases
where the Dolby E programme configuration does not match the AC-3 channel mode