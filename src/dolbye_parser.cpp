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

#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <string>
#include <stdio.h>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/framework/MemBufFormatTarget.hpp>

#include "dolbye_parser.h"
#include "dolbye_file.h"

constexpr short DolbyEParser::last_frame_tab[NUMFRAMERATES];
constexpr short DolbyEParser::drop_frame_tab[NUMFRAMERATES];
constexpr int   DolbyEParser::maskSync[nBitDepths];
constexpr int   DolbyEParser::preambleSyncA[nBitDepths];
constexpr int   DolbyEParser::preambleSyncB[nBitDepths];
constexpr int   DolbyEParser::preambleMode[nBitDepths];
constexpr int   DolbyEParser::bitDepthTab[nBitDepths];
constexpr int   DolbyEParser::nProgsTab[NPGMCFG];
constexpr int   DolbyEParser::nChansTab[NPGMCFG];
constexpr int   DolbyEParser::lfeChanTab[NPGMCFG];

/**************************************************************************************************************************************************************/
// Helpers
class XStr
{
    public :
    XStr(const char* const toTranscode)
    {
        // Call the private transcoding method
        fUnicodeForm = XMLString::transcode(toTranscode);
    }
    XStr(int i)
    {
        const std::string s = std::to_string(i);
        fUnicodeForm = XMLString::transcode(s.c_str());
    }

    ~XStr()
    {
        XMLString::release(&fUnicodeForm);
    }

    const XMLCh* unicodeForm() const
    {
        return fUnicodeForm;
    }

    private :
    XMLCh*   fUnicodeForm;
};

#define X(str) XStr(str).unicodeForm()

static void timecode_to_string(char *s, int timecode[])
{
    if ((timecode[1] & 0x3f) == 0x3f)
    {
        snprintf(s, 8, "invalid");
    }
    else
    {
        snprintf(s, 12, "%02d:%02d:%02d:%02d",
                ((((timecode[1] >> 4) & 0x03) * 10) + (timecode[1] & 0x0f)),
                ((((timecode[3] >> 4) & 0x07) * 10) + (timecode[3] & 0x0f)),
                ((((timecode[5] >> 4) & 0x07) * 10) + (timecode[5] & 0x0f)),
                ((((timecode[7] >> 4) & 0x03) * 10) + (timecode[7] & 0x0f)));
    }
}
// End of Helpers
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
DolbyEParser::DolbyEParser(std::string dolbyeInputFileName)
{
	if ((filePtr = fopen(dolbyeInputFileName.c_str(), "rb")) == NULL)
    {
        throw std::runtime_error("Error: File not found\n");
    }

	if (dolbyEFile.InitFile(filePtr, FILE_WORD_SZ) != 0)
    {
        throw std::runtime_error("Error opening input file");
    }
    // Determine number of frames in file
    GetNumberFrames();
    // Get Programme Descriptions
    GetProgrammeDescriptionText();
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/

void DolbyEParser::GetNumberFrames(void)
{
    // save position
    long pos = ftell(filePtr);
    // rewind
    fseek(filePtr, 0, SEEK_SET);
    frameCount = 0;
    while (0 == findPreambleSync(&frameInfo))
    {
        frameCount++;
    }
    // Return to original position
    fseek(filePtr, pos, SEEK_SET);
}


void DolbyEParser::GetProgrammeDescriptionText(void)
{
    bool finished = false;
    // save position
    long pos = ftell(filePtr);
    // rewind
    fseek(filePtr, 0, SEEK_SET);

    // Parse 70 frames
    // This is guaranteed to find all messages irrespective of the start point in the sequence
    unsigned int framesToCheck = std::min(frameCount, (unsigned int)70);
    for (unsigned int frame = 0 ; frame < framesToCheck ; frame ++)
    {
        GetNextFrame();
        // Parse a Dolby E frame
        if (Dolby_E_frame(&frameInfo))
        {
            throw std::runtime_error("Error Parsing Dolby E frame");
        }

        for (unsigned int pgm = 0 ; pgm < frameInfo.nProgs ; pgm++)
        {
            if (desc_text_received[pgm])
            {
                continue;
            }
            int c = frameInfo.description_text[pgm];
            switch (c)
            {
                case 0x00:    
                    null_char_warning[pgm] = 1;
                    break;
                case 0x02:
                    desc_text_ptr[pgm] = 0;
                    break;
                case 0x03:
                    description_text_buf[pgm][desc_text_ptr[pgm]] = '\0';
                    // Check to see if we have a valid string and signal
                    if (strlen(description_text_buf[pgm]) > 0)
                    {
                        desc_text_received[pgm] = true; 
                    }
                    break;
                default:
                    if ((c < 0x20) || (c > 0x7e))
                    {
                        std::cerr << "Warning: Invalid Character in program description text" << std::endl;
                        break;
                    }    

                    description_text_buf[pgm][desc_text_ptr[pgm]] = c;
                    desc_text_ptr[pgm]++;

                    if(desc_text_ptr[pgm] >= MAX_DESCTEXTLEN)
                    {
                        std::cerr << "Warning: Program description text too long - Truncating" << std::endl;
                        desc_text_length_error[pgm] = 1;
                        desc_text_ptr[pgm] = 0;
                        description_text_buf[pgm][MAX_DESCTEXTLEN - 1] = '\0';
                    }
            }
        }
    }
    // return to original position
    fseek(filePtr, pos, SEEK_SET);
}

int DolbyEParser::GetNextFrame(void)
{
	// Initialize all frame info elements to zero
	memset(&frameInfo, 0, sizeof(FrameInfoStruct));

	int err = findPreambleSync(&frameInfo);
	if (err)
    {
        throw std::runtime_error("Couldn't find sync in input file");
    }
    return err;
}

// This is added for speed
// Don't bother to init struct
// Only used for seeking
int DolbyEParser::SkipNextFrame(void)
{
	int err = findPreambleSync(&frameInfo);
	if (err)
    {
        throw std::runtime_error("Couldn't find sync in input file");
    }
    return err;
}

int DolbyEParser::GetFrame(unsigned int frameNo)
{
	unsigned int offset = 0;
	if (frameNo < frameCount)
	{
		fseek(filePtr, 0, SEEK_SET);
		offset = frameNo;
	}
	if (frameNo > frameCount)
	{
		offset = frameNo - frameCount;
	}
	for (unsigned int i = 0 ; i < offset ; offset++)
	{
		int err = SkipNextFrame();
		if (err)
		{
			return err;
		}
	}
	GetNextFrame();
	return 0;
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
DOMElement* DolbyEParser::AddDomNode(DOMElement *parent, std::string label)
{
    DOMElement* elem = doc->createElement(X(label.c_str()));
    parent->appendChild(elem);
    return(elem);
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
DOMElement* DolbyEParser::AddDomNodeValue(DOMElement *parent, std::string label, std::string value)
{
    DOMElement* elem = doc->createElement(X(label.c_str()));
    parent->appendChild(elem);
    elem->appendChild(doc->createTextNode(X(value.c_str())));
    return(elem);
}

DOMElement* DolbyEParser::AddDomNodeValue(DOMElement *parent, std::string label, unsigned int value)
{
    DOMElement* elem = doc->createElement(X(label.c_str()));
    parent->appendChild(elem);
    elem->appendChild(doc->createTextNode(X(value)));
    return(elem);
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
DOMElement* DolbyEParser::AddDomNodeAttribute(DOMElement *parent, std::string label, std::string attribute, unsigned int value)
{
    return AddDomNodeAttribute(parent, label, attribute, std::to_string(value));
}

DOMElement* DolbyEParser::AddDomNodeAttribute(DOMElement *parent, std::string label, std::string attribute, std::string value)
{
    DOMElement* elem = doc->createElement(X(label.c_str()));
    parent->appendChild(elem);
    elem->setAttribute(X(attribute.c_str()), X(value.c_str()));
    return(elem);
}

DOMElement* DolbyEParser::AddDomNodeAttributes(DOMElement *parent, std::string label, const std::map<std::string, std::string> &attributes)
{
    DOMElement* elem = doc->createElement(X(label.c_str()));
    parent->appendChild(elem);
    for (auto i : attributes)
    {
        elem->setAttribute(X(i.first.c_str()), X(i.second.c_str()));
    }
    return(elem);
}


/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
DOMElement* DolbyEParser::AddDomNodeValueAttribute(DOMElement *parent, std::string label, unsigned int value, std::string attribute, unsigned int attribValue)
{
    return AddDomNodeValueAttribute(parent, label, value, attribute, std::to_string(attribValue));
}

DOMElement* DolbyEParser::AddDomNodeValueAttribute(DOMElement *parent, std::string label, unsigned int value, std::string attribute, std::string attribValue)
{
    DOMElement* elem = doc->createElement(X(label.c_str()));
    parent->appendChild(elem);
    elem->appendChild(doc->createTextNode(X(value)));
    elem->setAttribute(X(attribute.c_str()), X(attribValue.c_str()));
    return(elem);
}

DOMElement* DolbyEParser::AddDomNodeValueAttributes(DOMElement *parent, std::string label, std::string value, const std::map<std::string, std::string> &attributes)
{
    DOMElement* elem = doc->createElement(X(label.c_str()));
    parent->appendChild(elem);
    elem->appendChild(doc->createTextNode(X(value.c_str())));
    for (auto i : attributes)
    {
        elem->setAttribute(X(i.first.c_str()), X(i.second.c_str()));
    }
    return(elem);
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddDolbyESegment(DOMElement *parent)
{
    DOMElement* deMdSegElem = AddDomNodeAttribute(parent, "metadataSegment", "ID", "1");
    DOMElement* dolbyEElem = AddDomNodeAttribute(deMdSegElem, "dolbyE", "ID", "0");
    AddDomNodeValue(dolbyEElem, "programConfig", frameInfo.progConfig);
    AddDomNodeValue(dolbyEElem, "frameRateCode", frameInfo.frameRate);

    char tc[20];
    timecode_to_string(tc,frameInfo.timecode);
    AddDomNodeValue(dolbyEElem, "smpteTimeCode", tc);

    // Supported Dolby E programme configurations in the spec are 5.1+2 (0), 4x2 (6), 5.1 (11), 2+2 (19)
    if (frameInfo.progConfig == 0 || frameInfo.progConfig == 6 || frameInfo.progConfig == 11 || frameInfo.progConfig == 19)
    {
        std::cout << "Valid Dolby E programme configuration detected" << std::endl;
    }
    else
    {
        std::cout << "*** Warning Unsupported Dolby E programme configuration detected ***" << std::endl;
    }
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddAC3Segment(DOMElement *parent)
{
    DOMElement*  ac3MdSegElem = doc->createElement(X("metadataSegment"));
    parent->appendChild(ac3MdSegElem);
    ac3MdSegElem->setAttribute(X("ID"), X("3"));
    for (int progNo = 0 ; progNo < frameInfo.nProgs ; progNo++)
    {
        AddAC3Program(ac3MdSegElem, progNo);
    }
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddAC3EncoderParametersSegment(DOMElement *parent)
{
    DOMElement*  ac3EncParamSegElem = doc->createElement(X("metadataSegment"));
    parent->appendChild(ac3EncParamSegElem);
    ac3EncParamSegElem->setAttribute(X("ID"), X("11"));
    for (int progNo = 0 ; progNo < frameInfo.nProgs ; progNo++)
    {
        AddAC3EncoderParameters(ac3EncParamSegElem, progNo);
    }
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddTransportTrackFormatElem(DOMElement *parent)
{
    std::string atu_id = "";
    unsigned int trackCount = GetTotalNumberOfTracksRequired();
    std::map<std::string,std::string> attributes;

    attributes["transportID"] = "TP_0001";
    attributes["transportName"] = "X";
    attributes["numIDs"] = std::to_string(trackCount);
    attributes["numTracks"] = std::to_string(trackCount);
    DOMElement*  transportTrackFormatElem = AddDomNodeAttributes(parent, "transportTrackFormat", attributes);

    attributes.clear();
    attributes["formatLabel"] = "0001";
    attributes["formatDefinition"] = "PCM";

    for (unsigned int atu_counter = 0 ; atu_counter < trackCount ; atu_counter++)
    {
        attributes["trackID"] = std::to_string(atu_counter + 1);
        DOMElement* audioTrackElem = AddDomNodeAttributes(transportTrackFormatElem, "audioTrack", attributes);
        atu_id = audioTrackUID + std::to_string(atu_counter + 1);
        // A cheap way to keep the number of hex digits correct for test files that contain more tracks (as counted by each programme acmod Vs. the max in Dolby E (8))
        if (atu_id.length() > 12)
        {
            atu_id.erase(4,1);
        }
        AddDomNodeValue(audioTrackElem,"audioTrackUIDRef", atu_id);
    }
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
unsigned int DolbyEParser::GetTotalNumberOfTracksRequired()
{
    unsigned int totalTracks = 0;
    unsigned int ac3AcmodTracks = 0;

    for (unsigned int progNo = 0 ; progNo < frameInfo.nProgs ; progNo++)
    {
        switch(frameInfo.AC3Metadata.ac3_acmod[progNo])
        {
            case 1:
                ac3AcmodTracks = 1;
                break;
            case 2:
                ac3AcmodTracks = 2;
                break;
            case 3:
                ac3AcmodTracks = 3;
                break;
            case 4:
                ac3AcmodTracks = 3;
                break;
            case 5:
                ac3AcmodTracks = 4;
                break;
            case 6:
                ac3AcmodTracks = 4;
                break;
            case 7:
                ac3AcmodTracks = 6;
                break;
            default:
                std::cout << "*** Error Invalid AC-3 channel configuration detected ***" << std::endl;
        }
        totalTracks = totalTracks + ac3AcmodTracks;
    }
    return(totalTracks);
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddAudioFormatExtendedElem(DOMElement *parent)
{
    // Top level of ADM
    DOMElement*  audioFormatExtendedElem = AddDomNodeAttribute(parent, "audioFormatExtended", "version", "ITU-R_BS.2076-3");
    AddProfileElem(audioFormatExtendedElem);

    // Add audio programme(s) and references for each present AC-3 audio programme
    int atuCount = 0;
    for (int progNo = 0 ; progNo < frameInfo.nProgs ; progNo++)
    {
        atuCount = AddADMProgramme(audioFormatExtendedElem, progNo, atuCount);
    }
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddProfileElem(DOMElement *parent)
{
    DOMElement*  profileListElem = doc->createElement(X("profileList"));
    parent->appendChild(profileListElem);

    // Include details about the AdvSS profile (Dolby E profile is a subset of it)
    std::map<std::string,std::string> attributes1;
    attributes1["profileName"] =  "Advanced sound system: ADM and S-ADM profile for emission";
    attributes1["profileVersion"] = "1";
    attributes1["profileLevel"] = "1";
    AddDomNodeValueAttributes(profileListElem, "profile", "ITU-R BS.2168", attributes1);

    // Include details about the Dolby E profile
    std::map<std::string,std::string> attributes;
    attributes["profileName"] =  "Dolby E ADM and S-ADM Profile for emission";
    attributes["profileVersion"] = "1";
    attributes["profileLevel"] = "1";
    AddDomNodeValueAttributes(profileListElem, "profile", "Dolby E ADM and S-ADM Profile for emission", attributes);
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
unsigned int DolbyEParser::AddADMProgramme(DOMElement *parent, unsigned int progNo, unsigned int atuCount)
{
    std::string audioPackId;
    std::string audioProgrammeName = "Programme " + std::to_string(progNo + 1);
    std::string audioContentName = "Content " + std::to_string(progNo + 1);
    std::string audioObjectName = "Object " + std::to_string(progNo + 1);

    unsigned int countOfTracks = 0;
    unsigned int atuStartOffset = atuCount;
    unsigned int trackCount = 0;
    int loudnessValue = frameInfo.AC3Metadata.ac3_dialnorm[progNo] * -1;

    // Add optional program description text
    if (desc_text_received[progNo])
    {
        audioProgrammeName += std::string(" (") + std::string(description_text_buf[progNo]) + std::string(")");
    }
    // audioProgramme structure is very simple, each audioProgramme references just one audioContent
    std::map<std::string, std::string> attributes;
    attributes["audioProgrammeID"] = audioProgrammeID + std::to_string(progNo + 1);
    attributes["audioProgrammeName"] = audioProgrammeName;
    attributes["audioProgrammeLanguage"] = "und";

    DOMElement* audioProgrammeElement = AddDomNodeAttributes(parent, "audioProgramme", attributes);
    AddDomNodeValue(audioProgrammeElement, "audioContentIDRef", audioContentID + std::to_string(progNo + 1));

    DOMElement* audioProgrammeloudnessElem = AddDomNode(audioProgrammeElement, "loudnessMetadata");
    AddDomNodeValue(audioProgrammeloudnessElem, "dialogueLoudness", loudnessValue);

    // audioContent structure is very simple, each audioContent references one audio object
    attributes.clear();
    attributes["audioContentID"] = audioContentID + std::to_string(progNo + 1);
    attributes["audioContentName"] = audioContentName;
    attributes["audioContentLanguage"] = "und";

    DOMElement* audioContentElement = AddDomNodeAttributes(parent, "audioContent", attributes);
    AddDomNodeValue(audioContentElement, "audioObjectIDRef", audioObjectID + std::to_string(progNo + 1));

    DOMElement* audioContentloudnessElem = AddDomNode(audioContentElement, "loudnessMetadata");
    AddDomNodeValue(audioContentloudnessElem, "dialogueLoudness", loudnessValue);
    switch(frameInfo.AC3Metadata.ac3_bsmod[progNo])
    {
    // Complete Main
    case 0:
        AddDomNodeValueAttribute(audioContentElement, "dialogue", 2, "mixedContentKind", "1");
        break;
    // Music and Effects
    case 1:
        AddDomNodeValueAttribute(audioContentElement, "dialogue", 0, "nonDialogueContentKind", "3");
        break;
    // Audio Description / Visually Impaired
    case 2:
        AddDomNodeValueAttribute(audioContentElement, "dialogue", 2, "mixedContentKind", "4");
        break;
    // Commentary
    case 4:
    case 5:
        AddDomNodeValueAttribute(audioContentElement, "dialogue", 1, "dialogueContentKind", "5");
        break;
    // Emergency
    case 6:
        AddDomNodeValueAttribute(audioContentElement, "dialogue", 1, "dialogueContentKind", "6");
        break;
    case 3:
    case 7:
    default:
        AddDomNodeValueAttribute(audioContentElement, "dialogue", 2, "mixedContentKind", "0");
    }

    attributes.clear();
    attributes["audioObjectID"] = audioObjectID + std::to_string(progNo + 1);
    attributes["audioObjectName"] = audioObjectName;
    attributes["interact"] = "0";
    DOMElement* audioObjectElement = AddDomNodeAttributes(parent, "audioObject", attributes);

    // The only supported channel modes in spec are 2.0 and 5.1, support for other acmod values is left in here for test purposes
    // Channel modes that are not in common defs (2/1 and 2/2) will have the audioPackFormatID in the composition set to the nearest equivalent (3.0 and 3.1)
    switch(frameInfo.AC3Metadata.ac3_acmod[progNo])
    {
        case 1:
            countOfTracks = 1;
            audioPackId = "1";
            break;
        case 2:
            countOfTracks = 2;
            audioPackId = "2";
            break;
        case 3:
            countOfTracks = 3;
            audioPackId = "a";
            break;
        case 4:
            countOfTracks = 3;
            audioPackId = "a";
            break;
        case 5:
            countOfTracks = 4;
            audioPackId = "b";
            break;
        case 6:
            countOfTracks = 4;
            audioPackId = "b";
            break;
        case 7:
            countOfTracks = 6;
            audioPackId = "3";
            break;
        default:
            throw std::runtime_error("*** Error Invalid AC-3 channel configuration detected ***");
    }

    AddDomNodeValue(audioObjectElement, "audioPackFormatIDRef", audioPackFormatID + audioPackId);
    for (trackCount = 0 ; trackCount < countOfTracks ; trackCount++)
    {
        std::string atuId = audioTrackUID + std::to_string(atuStartOffset + (trackCount + 1));
        // A cheap way to keep the number of hex digits correct for test files that contain more tracks (as counted by each programme acmod Vs. the max in Dolby E (8))
        if (atuId.length() > 12)
        {
            atuId.erase(4,1);
        }
        AddDomNodeValue(audioObjectElement, "audioTrackUIDRef", atuId);

        DOMElement* audioTrackUIDElem = AddDomNodeAttribute(parent, "audioTrackUID", "UID", atuId);
        AddDomNodeValue(audioTrackUIDElem, "audioChannelFormatIDRef", audioChannelFormatID + std::to_string(trackCount + 1));
        AddDomNodeValue(audioTrackUIDElem, "audioPackFormatIDRef", audioPackFormatID + audioPackId);
    }
    return(atuStartOffset + trackCount);
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddAC3EncoderParameters(DOMElement *parent, unsigned int progNo)
{
    DOMElement* ac3ProgEncodeParameters = AddDomNodeAttribute(parent, "encodeParameters", "ID", progNo);
    AddDomNodeValue(ac3ProgEncodeParameters, "hpFOn", frameInfo.AC3Metadata.ac3_hpfon[progNo]);
    AddDomNodeValue(ac3ProgEncodeParameters, "bwLpFOn", frameInfo.AC3Metadata.ac3_bwlpfon[progNo]);
    AddDomNodeValue(ac3ProgEncodeParameters, "lfeLpFOn", frameInfo.AC3Metadata.ac3_lfelpfon[progNo]);
    AddDomNodeValue(ac3ProgEncodeParameters, "sur90On", frameInfo.AC3Metadata.ac3_sur90on[progNo]);
    AddDomNodeValue(ac3ProgEncodeParameters, "surAttOn", frameInfo.AC3Metadata.ac3_suratton[progNo]);
    AddDomNodeValue(ac3ProgEncodeParameters, "rfPremphOn", frameInfo.AC3Metadata.ac3_rfpremphon[progNo]);
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::AddAC3Program(DOMElement *parent, unsigned int progNo)
{
    DOMElement* ac3ProgElem = AddDomNodeAttribute(parent, "ac3Program", "ID", progNo);

    DOMElement* ac3ProgInfoElem = AddDomNode(ac3ProgElem, "programInfo");
    AddDomNodeValue(ac3ProgInfoElem, "acMod", frameInfo.AC3Metadata.ac3_acmod[progNo]);
    AddDomNodeValue(ac3ProgInfoElem, "bsMod", frameInfo.AC3Metadata.ac3_bsmod[progNo]);
    AddDomNodeValue(ac3ProgInfoElem, "lfeOn", frameInfo.AC3Metadata.ac3_lfeon[progNo]);
    AddDomNodeValue(ac3ProgElem, "cMixLev", frameInfo.AC3Metadata.ac3_cmixlev[progNo]);
    AddDomNodeValue(ac3ProgElem, "surMixLev", frameInfo.AC3Metadata.ac3_surmixlev[progNo]);
    AddDomNodeValue(ac3ProgElem, "dSurMod", frameInfo.AC3Metadata.ac3_dsurmod[progNo]);
    AddDomNodeValue(ac3ProgElem,"dialNorm", frameInfo.AC3Metadata.ac3_dialnorm[progNo]);
    AddDomNodeValue(ac3ProgElem,"copyRightB", frameInfo.AC3Metadata.ac3_copyrightb[progNo]);
    AddDomNodeValue(ac3ProgElem, "origBs", frameInfo.AC3Metadata.ac3_origbs[progNo]);

    DOMElement* langCodeElem = AddDomNodeAttribute(ac3ProgElem, "langCode", "exists", frameInfo.AC3Metadata.ac3_langcode[progNo]);
    AddDomNodeValue(langCodeElem, "langCod", frameInfo.AC3Metadata.ac3_langcod[progNo]);

    DOMElement* audioProdInfoElem = AddDomNodeAttribute(ac3ProgElem, "audioProdInfo", "exists", frameInfo.AC3Metadata.ac3_audprodie[progNo]);
    AddDomNodeValue(audioProdInfoElem, "mixLevel", frameInfo.AC3Metadata.ac3_mixlevel[progNo]);
    AddDomNodeValue(audioProdInfoElem,"roomTyp", frameInfo.AC3Metadata.ac3_roomtyp[progNo]);

    DOMElement* extBsi1eElem = AddDomNodeAttribute(ac3ProgElem, "extBsi1e", "exists", frameInfo.AC3Metadata.ac3_xbsi1e[progNo]);
    AddDomNodeValue(extBsi1eElem, "loRoCMixLev", frameInfo.AC3Metadata.ac3_lorocmixlev[progNo]);
    AddDomNodeValue(extBsi1eElem, "loRoSurMixLev", frameInfo.AC3Metadata.ac3_lorosurmixlev[progNo]);
    AddDomNodeValue(extBsi1eElem, "ltRtCMixLev", frameInfo.AC3Metadata.ac3_ltrtcmixlev[progNo]);
    AddDomNodeValue(extBsi1eElem, "ltRtSurMixLev", frameInfo.AC3Metadata.ac3_ltrtsurmixlev[progNo]);
    AddDomNodeValue(extBsi1eElem, "dMixMod", frameInfo.AC3Metadata.ac3_dmixmod[progNo]);

    DOMElement* extBsi2eElem = AddDomNodeAttribute(ac3ProgElem, "extBsi2e", "exists", frameInfo.AC3Metadata.ac3_xbsi2e[progNo]);
    AddDomNodeValue(extBsi2eElem, "dSurExMod", frameInfo.AC3Metadata.ac3_dsurexmod[progNo]);
    AddDomNodeValue(extBsi2eElem, "dHeadPhonMod", frameInfo.AC3Metadata.ac3_dheadphonmod[progNo]);
    AddDomNodeValue(extBsi2eElem, "adConvTyp", frameInfo.AC3Metadata.ac3_adconvtyp[progNo]);

    AddDomNodeValueAttribute(ac3ProgElem,"compr1", frameInfo.AC3Metadata.ac3_compr1[progNo], "exists", frameInfo.AC3Metadata.ac3_compre[progNo]);
    AddDomNodeValueAttribute(ac3ProgElem,"dynRng1", frameInfo.AC3Metadata.ac3_dynrng1[progNo], "exists", frameInfo.AC3Metadata.ac3_dynrnge[progNo]);
    if (desc_text_received[progNo])
    {
            AddDomNodeValue(ac3ProgElem, "programDescriptionText", std::string(description_text_buf[progNo]));
    }

    // Supported ac3_acmod configurations are 2 and 7, others might not have an equivalent common def pack
    if (frameInfo.AC3Metadata.ac3_acmod[progNo] == 2 || frameInfo.AC3Metadata.ac3_acmod[progNo] == 7)
    {
        std::cout << "Valid AC-3 channel configuration detected" << std::endl;
    }
    else
    {
        std::cerr << "*** Warning Unsupported AC-3 channel configuration detected ***" << std::endl;
    }
}
/**************************************************************************************************************************************************************/


/**************************************************************************************************************************************************************/
void DolbyEParser::GenerateSadmXML(std::string &s)
{
	// Parse a Dolby E frame
	if (Dolby_E_frame(&frameInfo))
	{
		throw std::runtime_error("Error Parsing Dolby E frame");
	}

	// Initialize the XML4C2 system
    try
    {
        XMLPlatformUtils::Initialize();
    }
    catch(const XMLException& toCatch)
    {
        char *pMsg = XMLString::transcode(toCatch.getMessage());
        std::cerr << "Error during Xerces-c Initialization.\n"
        << "  Exception message:"
        << pMsg;
        XMLString::release(&pMsg);
        return;
    }

    DOMImplementation* impl =  DOMImplementationRegistry::getDOMImplementation(X("Core"));

    if (impl == NULL)
    {
        throw std::runtime_error("Failed to create Implementation");
    }

    // Create top-level doc with root frame element of S-ADM
    doc = impl->createDocument(0, X("frame"), 0);

    DOMElement* rootElem = doc->getDocumentElement();
    rootElem->setAttribute(X("version"), X("ITU-R_BS.2125-1"));

    DOMElement* frameHeaderElem = AddDomNode(rootElem, "frameHeader");

    // Set S-ADM frame duration based upon Dolby E frame rate, if a fractional number then duration is set to first value of five frame sequence [1602, 1601, 1602, 1601, 1602]
    // Here we create a unique flowID for each composition and include it (attribute is optional in AdvSS profile)
    std::string duration = "00:00:00.0" + std::to_string(samples_per_frame[frameInfo.frameRate - 1]) + "S48000";
    std::map<std::string,std::string> attributes;
    attributes["frameFormatID"]="FF_00000001";
    attributes["type"] = "full";
    attributes["start"] = "00:00:00.00000S48000";
    attributes["duration"] = duration;
    attributes["timeReference"] = "local";
    attributes["flowID"] = GenerateUUID();

    AddDomNodeAttributes(frameHeaderElem, "frameFormat", attributes);

    // Add the transportTrackFormat element
    AddTransportTrackFormatElem(frameHeaderElem);

    // Add profileList element
    AddProfileElem(frameHeaderElem);
   
    // Create ADM template based upon acmod for each program
    AddAudioFormatExtendedElem(rootElem);

    // Add DBMD custom metadata element
    DOMElement* customElem = doc->createElement(X("audioFormatCustom"));
    rootElem->appendChild(customElem);
    attributes.clear();
    attributes["audioFormatCustomSetID"] = "AFC_1001";
    attributes["audioFormatCustomSetName"] = "DolbyE DBMD Chunk";
    attributes["audioFormatCustomSetType"] = "CUSTOM_SET_TYPE_DOLBYE_DBMD_CHUNK";
    attributes["audioFormatCustomSetVersion"] = "1";
    DOMElement* customSetElem = AddDomNodeAttributes(customElem, "audioFormatCustomSet", attributes);
    DOMElement* dbmdElem = doc->createElement(X("dbmd"));
    customSetElem->appendChild(dbmdElem);

    // Add Dolby E segment to DBMD
    AddDolbyESegment(dbmdElem);

    // Add AC3 S=segment(s) to DBMD
    AddAC3Segment(dbmdElem);

    // Add AC3 Encode parameter(s) segment to DBMD
    AddAC3EncoderParametersSegment(dbmdElem);

    DOMLSSerializer   *theSerializer = ((DOMImplementationLS*)impl)->createLSSerializer();
    DOMLSOutput       *theOutputDesc = ((DOMImplementationLS*)impl)->createLSOutput();
    MemBufFormatTarget *myFormTarget;

    theSerializer->getDomConfig()->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);
    theSerializer->getDomConfig()->setParameter(XMLUni::fgDOMWRTXercesPrettyPrint, false);

    myFormTarget = new MemBufFormatTarget();
    theOutputDesc->setByteStream(myFormTarget);
    theSerializer->write(doc, theOutputDesc);

    unsigned int xmlLen = myFormTarget->getLen();
    const unsigned char *xmlBuffer = myFormTarget->getRawBuffer();

    s = std::string(xmlBuffer, xmlBuffer + xmlLen);
    theOutputDesc->release();
    theSerializer->release();
    delete myFormTarget;

    doc->release();

    XMLPlatformUtils::Terminate();
}
/**************************************************************************************************************************************************************/