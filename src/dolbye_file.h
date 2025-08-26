/******************************************************************************
 * This program is protected under international and U.S. copyright laws as
 * an unpublished work. This program is confidential and proprietary to the
 * copyright owners. Reproduction or disclosure, in whole or in part, or the
 * production of derivative works therefrom without the express permission of
 * the copyright owners is prohibited.
 *
 *                Copyright (C) 2024 by Dolby Laboratories,
 *                Copyright (C) 2024 by Dolby International AB.
 *                            All rights reserved.
 ******************************************************************************/

#ifndef		_BITUNP_H_
#define		_BITUNP_H_

typedef int Int32;

#define DATA_BUF_SZ 4096
#define N_DOWN_CNTRS 3

/* error enumerations for BITUNP module */	   
enum 
{
	errAOK,
	errInvalidWordSize = 200,
	errInvalidPackedDataPtr,
	errItemWordSizeExceedsPackedWordSize,
	errNumberBitsLeftLessThanZero
};

class DolbyEFile
{
private:

	FILE *FilePtr;		/* packed data file handle */
	int FileWrdSz;			/* file word size (bytes) */
	int BSWrdSz;			/* bit stream / payload word size (bits) */

	Int32 DataBuf[DATA_BUF_SZ], *BufPtr;
	int BitPtr;
	int BitCnt;				/* number of bits in DataBuf */
	int DnCntr[N_DOWN_CNTRS];

public:

	DolbyEFile(void);

	int InitFile(					/* return error code.  0 = AOK */
		FILE *fPtr,					/* IN: packed data offset */
		int wdSz);					/* IN: file word size (bytes) */

	int InitStream(					/* return error code.  0 = AOK */
		int wdSz);					/* IN: bit stream word word size (bits) */

	int ReadFile(					/* return error code.  0 = AOK */
		int nWords);				/* IN: # items to be unpacked */

	int GetBitsLeft(void);			/* return # of bits left in input buffer */

	int BitUnkey(					/* return error code.  0 = AOK */
		int keyvalue,				/* IN: key value */
		int numitems); 				/* IN: # items to be unkeyed */

	int BitUnp_rj(					/* return error code.  0 = AOK */
		int datalist[],				/* IN/OUT: ptr to data array to be filled */
		int numitems, 				/* IN: # items to be unpacked  */
		int numbits);				/* IN: # bits per item */

	int SkipBits(					/* return error code.  0 = AOK */
		int numSkipBits);			/* IN: # bits to skip */

	int SetDnCntr(					/* return error code.  0 = AOK */
		int counterNum,				/* IN: counter number to set */
		int cnt);					/* IN: counter setting */

	int GetDnCntr(					/* return bit count, -1 if error */
		int counterNum);			/* OUT: counter setting */

	int decBitCntrs(				/* return error code.  0 = AOK */
		int numbits);				/* IN: # bits to decrement from counters */
};

#endif
