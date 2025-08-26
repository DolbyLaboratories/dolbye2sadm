/******************************************************************************
 * This program is protected under international and U.S. copyright laws as   *
 * an unpublished work. This program is confidential and proprietary to the   *
 * copyright owners. Reproduction or disclosure, in whole or in part, or the  *
 * production of derivative works therefrom without the express permission of *
 * the copyright owners is prohibited.                                        *
 *                                                                            *
 *                Copyright (C) 2024 by Dolby Laboratories,                   *
 *                Copyright (C) 2024 by Dolby International AB.               *
 *                            All rights reserved.                            *
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
