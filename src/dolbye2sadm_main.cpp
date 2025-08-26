/****************************************************************************
 *
 *
 * Copyright (c) 2024 Dolby International AB.
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 * BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

#include "dolbye_parser.h"

void show_usage(void)
{
    std::cout << std::endl << "Usage: dolbye2sadm infile.dde [outfile.xml]" << std::endl;
    exit(2);
}

int main(int argc, char *argv[])
{
    std::ofstream outputXmlFile;
    char *inputFileName;
    char *outputFileName;

// Print banner
    std::cout << std::endl << "Dolby E to S-ADM Conversion tool " << REV_STR << std::endl;
    std::cout << "(C) Copyright 2025 Dolby Laboratories, Inc.  All rights reserved." << std::endl;

// Parse command line arguments
    if ((argc == 1) || (argc > 3))
    {
        show_usage();
    }

    inputFileName = argv[1];

    if (argc == 3)
    {
        outputFileName = argv[2];
    }
    else
    {
        outputFileName = nullptr;
    }

// Open file to write XML
    if (outputFileName)
    {
        outputXmlFile.open(outputFileName);
        if (!outputXmlFile.is_open())
        {
            throw std::runtime_error("Error: Unable to open file to write xml data");
            exit(1);
        }
    }

    DolbyEParser parser(inputFileName);
    std::string s;

    parser.GetNextFrame();
    parser.GenerateSadmXML(s);
    if (outputXmlFile.is_open())
    {
        outputXmlFile << s;
        outputXmlFile.close();
    }
    else
    {
        std::cout << s;
    }
    return 0;
}
