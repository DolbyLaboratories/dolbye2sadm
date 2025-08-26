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


#ifndef		_DOLBYE_PARSER_H_
#define		_DOLBYE_PARSER_H_

#include <string>
#include <map>

#include "ddeinfo.h"
#include "dolbye_file.h"

#include <xercesc/dom/DOM.hpp>

using namespace XERCES_CPP_NAMESPACE;


#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>




class DolbyEParser
{
private:
	FILE *filePtr;
	unsigned int frameCount;
	FrameInfoStruct frameInfo;
	DolbyEFile dolbyEFile;
	DOMDocument* doc;

	char description_text_buf[MAX_NPGRMS][MAX_DESCTEXTLEN];    /* description text buffer */
	int desc_text_ptr[MAX_NPGRMS] = {0};
	int null_char_warning[MAX_NPGRMS] = {0};
	int desc_text_length_error[MAX_NPGRMS] = {0};
	bool desc_text_received[MAX_NPGRMS] = {false};

	// Old Stuff

	int compare_frameinfo(FrameInfoStruct *info1, FrameInfoStruct *info2);
	int findPreambleSync(FrameInfoStruct *fip);
	int Dolby_E_frame(FrameInfoStruct *fip);
	int sync_segment(FrameInfoStruct *fip);
	int display_sync_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag);
	int metadata_segment(FrameInfoStruct *fip);
	int display_metadata_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag);
	int ac3_metadata_subsegment(FrameInfoStruct *fip, int subseg_id);
	int display_ac3_metadata_subsegment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag, int subseg_id);
	int audio_segment(FrameInfoStruct *fip);
	int channel_subsegment(ChannelSubsegInfoStruct *cip);
	int metadata_extension_segment(FrameInfoStruct *fip);
	int display_metadata_extension_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag);
	int ac3_metadata_extension_subsegment(FrameInfoStruct *fip);
	int display_ac3_metadata_extension_subsegment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag);
	int audio_extension_segment(FrameInfoStruct *fip);
	int meter_segment(FrameInfoStruct *fip);
	int display_meter_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag);

	// New Stuff

	DOMElement* AddDomNode(DOMElement *parent, std::string label);
	DOMElement* AddDomNodeValue(DOMElement *parent, std::string label, std::string value);
	DOMElement* AddDomNodeValue(DOMElement *parent, std::string label, unsigned int value);
	DOMElement* AddDomNodeAttribute(DOMElement *parent, std::string label, std::string attribute, unsigned int value);
	DOMElement* AddDomNodeAttribute(DOMElement *parent, std::string label, std::string attribute, std::string value);
	DOMElement* AddDomNodeValueAttribute(DOMElement *parent, std::string label, unsigned int value, std::string attribute, unsigned int attribValue);
	DOMElement* AddDomNodeValueAttribute(DOMElement *parent, std::string label, unsigned int value, std::string attribute, std::string attribValue);
	DOMElement* AddDomNodeValueAttributes(DOMElement *parent, std::string label, std::string value, const std::map<std::string, std::string> &attributes);
	DOMElement* AddDomNodeAttributes(DOMElement *parent, std::string label, const std::map<std::string, std::string> &attributes);
	
	void AddProfileElem(DOMElement *parent);
	void AddDolbyESegment(DOMElement *parent);
	void AddAC3Segment(DOMElement *parent);
	void AddAC3Program(DOMElement *parent, unsigned int progNo);
	void AddAC3EncoderParametersSegment(DOMElement *parent);	
	void AddAC3EncoderParameters(DOMElement *parent, unsigned int progNo);
	void AddTransportTrackFormatElem(DOMElement *parent);
	void AddAudioFormatExtendedElem(DOMElement *parent);
	
	unsigned int AddADMProgramme(DOMElement *parent, unsigned int progNo, unsigned int atuCount);
	unsigned int GetTotalNumberOfTracksRequired();
	void GetProgrammeDescriptionText(void);
	void GetNumberFrames(void);


public:
	DolbyEParser(std::string dolbyeInputFileName);

	int GetNextFrame(void);
	int SkipNextFrame(void);
	int GetFrame(unsigned int frameNo);

	void GenerateSadmXML(std::string &s);

	std::string GenerateUUID(void)
	{
		const std::string uuid_str = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
    	return uuid_str;
	}

	// ADM ID names
	const std::string audioTrackUID = "ATU_0000000";
	const std::string audioPackFormatID = "AP_0001000";
	const std::string audioChannelFormatID = "AC_0001000";
	const std::string audioObjectID = "AO_100";
	const std::string audioContentID = "ACO_100";
	const std::string audioProgrammeID = "APR_100";

	const float frame_rates[NUMFRAMERATES] = {(float)23.98, 24, 25, (float)29.97, 30};
	const int samples_per_frame[NUMFRAMERATES] = {2002, 2000, 1920, 1602, 1600};

	/* table giving the number of frames before turning over to zero in the SMPTE time code for each frame rate */
	static constexpr short last_frame_tab[NUMFRAMERATES] = {24, 24, 25, 30, 30};

	/* table giving the drop frame flag for each frame rate */
	static constexpr short drop_frame_tab[NUMFRAMERATES] = {1, 0, 0, 1, 0};

	const char *frame_rts[NUMFRAMERATES] = {"23.98 fps", "24 fps", "25 fps", "29.97 fps", "30 fps"};

	static constexpr int maskSync[nBitDepths] =
	{   0x0ffff00, 0x0fffff0, 0x0ffffff };

	static constexpr int preambleSyncA[nBitDepths] =
	{   0x0f87200, 0x06f8720, 0x096f872 };

	static constexpr int preambleSyncB[nBitDepths] =
	{   0x04e1f00, 0x054e1f0, 0x0a54e1f };

	static constexpr int preambleMode[nBitDepths] =
	{   0x0000000, 0x0002000, 0x0004000 };

	static constexpr int bitDepthTab[nBitDepths] =
	{   16, 20, 24 };

	static constexpr int nProgsTab[NPGMCFG] =
	{   2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 8, 1, 2, 3, 3, 4, 5, 6, 1, 2, 3, 4, 1, 1 };

	static constexpr int nChansTab[NPGMCFG] =
	{   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 6, 6, 6, 6, 6, 6, 6, 4, 4, 4, 4, 8, 8 };

	static constexpr int lfeChanTab[NPGMCFG] =
	{   5, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 5, 5 };

	const char *yesNoText[2] =
	{   "no", "yes" };

	const char *progConfigText[NPGMCFG] =
	{   "5.1+2", "5.1+1+1", "4+4", "4+2+2", "4+2+1+1", "4+1+1+1+1", "2+2+2+2",
	    "2+2+2+1+1", "2+2+1+1+1+1", "2+1+1+1+1+1+1", "1+1+1+1+1+1+1+1",
	    "5.1", "4+2", "4+1+1", "2+2+2", "2+2+1+1", "2+1+1+1+1", "1+1+1+1+1+1",
	    "4", "2+2", "2+1+1", "1+1+1+1", "7.1", "7.1 Screen" };

	const char *chanIDText[NPGMCFG][MAX_NCHANS] =
	{   { "0L", "0C", "0Ls", "1L", "0R", "0LFE", "0Rs", "1R" },
	    { "0L", "0C", "0Ls", "1C", "0R", "0LFE", "0Rs", "2C" },
	    { "0L", "0C", "1L", "1C", "0R", "0S", "1R", "1S" },
	    { "0L", "0C", "1L", "2L", "0R", "0S", "1R", "2R" },
	    { "0L", "0C", "1L", "2C", "0R", "0S", "1R", "3C" },
	    { "0L", "0C", "1C", "3C", "0R", "0S", "2C", "4C" },
	    { "0L", "1L", "2L", "3L", "0R", "1R", "2R", "3R" },
	    { "0L", "1L", "2L", "3C", "0R", "1R", "2R", "4C" },
	    { "0L", "1L", "2C", "4C", "0R", "1R", "3C", "5C" },
	    { "0L", "1C", "3C", "5C", "0R", "2C", "4C", "6C" },
	    { "0C", "2C", "4C", "6C", "1C", "3C", "5C", "7C" },
	    { "0L", "0C", "0Ls", "0R", "0LFE", "0Rs", "", "" },
	    { "0L", "0C", "1L", "0R", "0S", "1R", "", "" },
	    { "0L", "0C", "1C", "0R", "0S", "2C", "", "" },
	    { "0L", "1L", "2L", "0R", "1R", "2R", "", "" },
	    { "0L", "1L", "2C", "0R", "1R", "3C", "", "" },
	    { "0L", "1C", "3C", "0R", "2C", "4C", "", "" },
	    { "0C", "2C", "4C", "1C", "3C", "5C", "", "" },
	    { "0L", "0C", "0R", "0S", "", "", "", "" },
	    { "0L", "1L", "0R", "1R", "", "", "", "" },
	    { "0L", "1C", "0R", "2C", "", "", "", "" },
	    { "0C", "2C", "1C", "3C", "", "", "", "" }, 
	    { "0L", "0C", "0Ls", "0BLs", "0R", "0LFE", "0Rs", "0BRs" },
	    { "0L", "0C", "0Ls", "0Le", "0R", "0LFE", "0Rs", "0Re" }};

	const char *frameRateText[NFRMRATE] =
	{   "23.98 fps", "24 fps", "25 fps", "29.97 fps", "30 fps",
	    "50 fps", "59.94 fps", "60 fps" };

	const char *timeCodeText[2] =
	{   "nondrop", "drop" };

	const char *bandwidthIDText[4] =
	{   "full bandwidth", "half bandwidth", "voice grade", "reserved" };

	const char *bitpoolTypeText[2] =
	{   "independent", "common" };

	const char *metaSubSegText[16] =
	{   "none", "AC-3 metadata subsegment xbsi support", 
	    "AC-3 metadata subsegment no xbsi support", "reserved",
	    "reserved", "reserved", "reserved", "reserved",
	    "reserved", "reserved", "reserved", "reserved",
	    "reserved", "reserved", "reserved", "reserved" };

	const char *groupTypeCodeText[4] =
	{   "long", "short", "bridge", "reserved" };

	const char *newReuseText[4] =
	{   "reuse", "new", "stop", "reserved" };

	const char *onOffText[2] =
	{   "off", "on" };

	const char *AC3datarateText[32] =
	{   "32 kbps", "40 kbps", "48 kbps", "56 kbps", "64 kbps", "80 kbps",
	    "96 kbps", "112 kbps", "128 kbps", "160 kbps", "192 kbps", "224 kbps",
	    "256 kbps", "320 kbps", "384 kbps", "448 kbps", "512 kbps", "576 kbps",
	    "640 kbps", "reserved", "reserved", "reserved", "reserved", "reserved",
	    "reserved", "reserved", "reserved", "reserved", "reserved", "reserved",
	    "reserved", "not specified" };

	const char *AC3bsmodText[9] =
	{   "complete main", "music and effects", "visually impaired",
	    "hearing impaired", "dialogue", "commentary", "emergency",
	    "voice over", "karaoke" };

	const char *AC3acmodText[8] =
	{   "1+1", "1/0", "2/0", "3/0", "2/1", "3/1", "2/2", "3/2" };

	const char *AC3cmixlevText[4] =
	{   "-3 dB", "-4.5 dB", "-6 dB", "reserved" };

	const char *AC3surmixlevText[4] =
	{   "-3 dB", "-6 dB", "-inf dB", "reserved" };

	const char *AC3dsurmodText[4] =
	{   "not indicated", "NOT Dolby Surround encoded", "Dolby Surround encoded",
	    "reserved" };

	const char *AC3roomtypText[4] =
	{   "not indicated", "large room, X curve monitor", "small room, flat monitor",
	    "reserved" };

	#define NCOMPPRESETS 6

	const char *AC3compPresetText[NCOMPPRESETS] =
	{       "none", "Film Standard", "Film Light", "Music Standard", "Music Light",
	    "Speech" };

	const char *AC3dmixmodText[4] = 
	{      "not indicated", "Lt/Rt downmix preferred", "Lo/Ro downmix preferred",
	       "reserved" };

	const char *AC3newmixlevText[8] =
	{      "1.414 (+3.0 dB)", "1.189 (+1.5 dB)", "1.000 ( 0.0 dB)", "0.841 (-1.5 dB)",
	       "0.707 (-3.0 dB)", "0.595 (-4.5 dB)", "0.500 (-6.0 dB)", "0.000 (-inf dB)"};

	const char *AC3dsurexmodText[4] =
	{      "not indicated", "NOT Dolby Surround EX encoded", 
	       "Dolby Surround EX encoded", "reserved"};

	const char *AC3dheadphonmodText[4] =
	{     "not indicated", "NOT Dolby Headphone encoded",
	      "Dolby Headphone encoded", "reserved"};

	const char *AC3adconvtyp[2] = 
	{     "Standard", "HDCD"};
};

#endif //		_DOLBYE_PARSER_H_

