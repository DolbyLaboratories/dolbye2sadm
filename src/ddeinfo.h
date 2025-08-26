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

#ifndef		_DDEINFO_H_
#define		_DDEINFO_H_


/***** Defines and enumerations *****/

#define REV_STR			"1.0"
#define FILE_WORD_SZ	4			/* # bytes per word in bs file */
#define PREAMBLE_SZ		4

#define DISP_SYNC		0x0001
#define DISP_META		0x0002
#define DISP_METR		0x0004

#define NUMFRAMERATES	5

#define DEC2BCD(in)		(((in/10) << 4) + (in % 10))
#define BCD2DEC(in)		(((in >> 4) * 10) + (in & 0xF))

enum		/* group types */
{
	LONG,
	SHORT,
	BRIDGE
};

/***** Constant tables *****/

#define SYNC16			0x078e
#define SYNC20			0x0788e
#define SYNC24			0x07888e

#define NPGMCFG			24
#define NFRMRATE		8
#define MAX_BITDEPTH	24
#define	MAX_NCHANS		8
#define	MAX_NBLKS		9
#define MAX_NPGRMS		8
#define MAX_ADDBSIBYTES 64
#define MAX_NUMSEGS		3
#define MAX_DESCTEXTLEN	34

#define maskStrmNum		0x0e00000
#define maskErr			0x0008000
#define maskMode		0x0006000
#define maskType		0x0001f00

#define preambleStrm0	0x0000000
#define preambleDolbyE	0x0001c00
#define preambleNoErr	0x0000000

enum		/* bit depth values */
{
	bitDepth16 = 0,
	bitDepth20,
	bitDepth24,
	nBitDepths
};


/***** Structure declarations *****/

typedef struct {
	int sync_word;
	int key_present;
} SyncSegmentStruct;

typedef struct {	
	int metadata_revision_id;
	int metadata_segment_size;
	int program_config;
	int frame_rate_code;
	int original_frame_rate_code;	
	int metadata_reserved_bits;
	int metadata_extension_segment_size;
	int meter_segment_size;	
	int bandwidth_id[MAX_NPGRMS];
	int revision_id[MAX_NCHANS];
	int bitpool_type[MAX_NCHANS];
	int begin_gain[MAX_NCHANS];
	int end_gain[MAX_NCHANS];
	int metadata_subsegment_id[MAX_NUMSEGS];
	int metadata_subsegment_length[MAX_NUMSEGS];
	int unused_metadata_subsegment_bits[MAX_NUMSEGS];
	int unused_metadata_bits;	
} MetadataSegmentStruct;

typedef struct {
	int ac3_datarate[MAX_NPGRMS];
	int ac3_bsmod[MAX_NPGRMS];
	int ac3_acmod[MAX_NPGRMS];
	int ac3_cmixlev[MAX_NPGRMS];
	int ac3_surmixlev[MAX_NPGRMS];
	int ac3_dsurmod[MAX_NPGRMS];
	int ac3_lfeon[MAX_NPGRMS];
	int ac3_dialnorm[MAX_NPGRMS];
	int ac3_langcode[MAX_NPGRMS];
	int ac3_langcod[MAX_NPGRMS];
	int ac3_audprodie[MAX_NPGRMS];
	int ac3_mixlevel[MAX_NPGRMS];
	int ac3_roomtyp[MAX_NPGRMS];
	int ac3_copyrightb[MAX_NPGRMS];
	int ac3_origbs[MAX_NPGRMS];
	int ac3_xbsi1e[MAX_NPGRMS];
	int ac3_dmixmod[MAX_NPGRMS];
	int ac3_ltrtcmixlev[MAX_NPGRMS];
	int ac3_ltrtsurmixlev[MAX_NPGRMS];
	int ac3_lorocmixlev[MAX_NPGRMS];
	int ac3_lorosurmixlev[MAX_NPGRMS];
	int ac3_xbsi2e[MAX_NPGRMS];
	int ac3_dsurexmod[MAX_NPGRMS];
	int ac3_dheadphonmod[MAX_NPGRMS];
	int ac3_adconvtyp[MAX_NPGRMS];
	int ac3_xbsi2[MAX_NPGRMS];
	int ac3_encinfo[MAX_NPGRMS];
	int ac3_hpfon[MAX_NPGRMS];
	int ac3_bwlpfon[MAX_NPGRMS];
	int ac3_lfelpfon[MAX_NPGRMS];
	int ac3_sur90on[MAX_NPGRMS];
	int ac3_suratton[MAX_NPGRMS];
	int ac3_rfpremphon[MAX_NPGRMS];
	int ac3_compre[MAX_NPGRMS];
	int ac3_compr1[MAX_NPGRMS];
	int ac3_dynrnge[MAX_NPGRMS];
	int ac3_dynrng1[MAX_NPGRMS];
	int ac3_dynrng2[MAX_NPGRMS];
	int ac3_dynrng3[MAX_NPGRMS];
	int ac3_dynrng4[MAX_NPGRMS];
	int ac3_addbsie[MAX_NPGRMS];
	int ac3_addbsil[MAX_NPGRMS];
	int ac3_addbsi[MAX_NPGRMS][MAX_ADDBSIBYTES];
	int ac3_timecod1e[MAX_NPGRMS];
	int ac3_timecod1[MAX_NPGRMS];
	int ac3_timecod2e[MAX_NPGRMS];
	int ac3_timecod2[MAX_NPGRMS];
} AC3MetadataSegmentStruct;

typedef struct {
	int metadata_extension_key;
	int metadata_extension_subsegment_id[MAX_NUMSEGS];
	int metadata_extension_subsegment_length[MAX_NUMSEGS];
	int unused_metadata_extension_subsegment_bits[MAX_NUMSEGS];
	int unused_metadata_extension_bits;	
} MetadataExtSegmentStruct;

typedef struct {
	int ac3_compr2[MAX_NPGRMS];
	int ac3_dynrng5[MAX_NPGRMS];
	int ac3_dynrng6[MAX_NPGRMS];
	int ac3_dynrng7[MAX_NPGRMS];
	int ac3_dynrng8[MAX_NPGRMS];
} AC3MetadataExtSegmentStruct;

typedef struct {
	int peak_meter[MAX_NCHANS];
	int rms_meter[MAX_NCHANS];
} MeterSegmentStruct;

typedef struct {

	int frameLength;				/* # words in frame */
	int wordSz;						/* # valid bits per word (lj'd) */
	int keyPresent;					/* key present flag */					
	int progConfig;					/* program configuration */
	int frameRate;					/* frame rate code */
	int lowFrameRate;				/* low frame rate flag */
	int metaExtSz;					/* metadata ext subsegment size */
	int chanSubsegSz[MAX_NCHANS];	/* channel subsegment sizes */
	int meterSz;					/* meter subsegment size */
	int nProgs;						/* number of programs */
	int nChans;						/* number of channels */
	int lfeChan;					/* LFE channel index */

	SyncSegmentStruct				Sync;			/* sync frame segment */
	MetadataSegmentStruct			Metadata;		/* metadata frame segment */
	AC3MetadataSegmentStruct		AC3Metadata;	/* ac3 metadata frame segment */
	MetadataExtSegmentStruct		MetadataExt;	/* metadata extension frame segment */
	AC3MetadataExtSegmentStruct		AC3MetadataExt;	/* ac3 metadata extension frame segment */
	MeterSegmentStruct				Meter;			/* meter frame segment */

	int prevGroupTypeCode[MAX_NCHANS];	/* primary group type code */
	int metadata_key;				/* metadata key value */
	int frame_count;				/* frame count */
	int description_text[MAX_NPGRMS];  /* description text character */	
	int metadata_crc;				/* metadata CRC value */
	int metadata_extension_crc;		/* metadata extension CRC value */
	int timecode[8];				/* time code */

} FrameInfoStruct;

typedef struct {
	int lowFrameRate;				/* low frame rate flag */
	int priExtFlag;					/* primary/extension flag */
	int lfeFlag;					/* LFE channel flag */
	int groupTypeCode;				/* group type code */
	int bandwidthCode;				/* bandwidth code */
	int blockCount;					/* number of blocks */
	int regionCount[MAX_NBLKS];		/* number of regions per block */
	int bandCount[MAX_NBLKS];		/* number of bands per block */
	int prevGroupTypeCode;			/* previous group type code */
} ChannelSubsegInfoStruct;

#endif	//	_DDEINFO_H_


