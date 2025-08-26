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
