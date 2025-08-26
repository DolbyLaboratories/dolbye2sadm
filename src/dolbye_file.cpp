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

#include <stdio.h>
#include "dolbye_file.h"

#define X -1

#define BIT_ERR_NONE 0
#define BIT_ERR_EOF 0xe0f

enum
{
	BIT_ERR_NOINIT = 1000,
	BIT_ERR_OVERWRITE,
	BIT_ERR_FILEREAD,
	BIT_ERR_UNDERFLOW
};

DolbyEFile::DolbyEFile():
FilePtr(NULL),
FileWrdSz(X),			/* file word size (bytes) */
BSWrdSz(X),			/* bit stream / payload word size (bits) */
BufPtr(nullptr),
BitPtr(0),
BitCnt(0)			/* number of bits in DataBuf */
{}


/*******************************************************************************
;
; InitFile
;	initialize bitstream file parameters
;
*******************************************************************************/

int DolbyEFile::InitFile(					/* return error code.  0 = AOK */
	FILE *fPtr,					/* IN: packed data offset */
	int wdSz)					/* IN: file word size (bytes) */
{

//	printf("Initializing file, word size = %d\n", wdSz);

	FilePtr = fPtr;				/* set file pointer */
	FileWrdSz = wdSz;			/* set file word size in bytes */
	BSWrdSz = X;				/* discard previous bitstream word size */
	BitCnt = X;					/* discard previous bit count */

	if (FilePtr == NULL) return(BIT_ERR_NOINIT);
	if (FileWrdSz == X) return(BIT_ERR_NOINIT);

	return(BIT_ERR_NONE);
}	/* InitFile() */

/*******************************************************************************
;
; InitStream
;	initialize bitstream formatting parameters
;
*******************************************************************************/

int DolbyEFile::InitStream(					/* return error code.  0 = AOK */
	int wdSz)					/* IN: bit stream word word size (bits) */
{
	if (FileWrdSz == X) return(BIT_ERR_NOINIT);

//	printf("Initializing stream, word size = %d\n", wdSz);

	BSWrdSz = wdSz;				/* set bitstream word size in bits */
	BitCnt = 0;					/* reset bit count */

	if (BSWrdSz == X) return(BIT_ERR_NOINIT);

	return(BIT_ERR_NONE);
}	/* InitStream() */

/*******************************************************************************
;
; ReadFile
;	read multiple words from file into buffer
;
*******************************************************************************/

int DolbyEFile::ReadFile(
	int nWords)					/* IN: # items to be unpacked */
{
	if (BSWrdSz == X) return(BIT_ERR_NOINIT);

//	printf("Reading %d words\n", nWords);

	if (BitCnt != 0)
	{
//		printf("Bit count = %d\n", BitCnt);
		return(BIT_ERR_OVERWRITE);		/* attempt to overwrite buffer */
	}

	if (fread((void *)DataBuf, FileWrdSz, nWords, FilePtr) != (size_t)nWords)
	{
		if (feof(FilePtr))
		{
			return(BIT_ERR_EOF);		/* end of file */
		}
		else
		{
			return(BIT_ERR_FILEREAD);	/* file read error */
		}
	}

	BitCnt = nWords * BSWrdSz;		/* set valid bit count */
	BufPtr = DataBuf;				/* reset data buffer pointer */
	BitPtr = 0;						/* reset current bit pointer */

//	printf("Bit count = %d\n", BitCnt);

	return(BIT_ERR_NONE);
}	/* ReadFile() */

/*******************************************************************************
;
; GetBitsLeft
;	get # bits left in bitstream
;
*******************************************************************************/

int DolbyEFile::GetBitsLeft(void)
{
	return(BitCnt);
}

/*******************************************************************************
;
;	Function Name:	BitUnkey
;	Contents:		Undo bitstream key
;
*******************************************************************************/

int DolbyEFile::BitUnkey(					/* return error code.  0 = AOK */
	int keyvalue,				/* IN: key value */
	int numitems) 				/* IN: # items to be unpacked */
{
	int *payload;
	int i;

//	printf("Unshifted key = %08x\n", keyvalue);
	keyvalue <<= (FileWrdSz*8 - BSWrdSz);
//	printf("Shifted key = %08x\n", keyvalue);

	if (BitCnt < (numitems * BSWrdSz))
	{
		return(BIT_ERR_UNDERFLOW);		/* underflow error */
	}

	if (BitPtr != 0)
	{
		payload = BufPtr + 1;
	}
	else
	{
		payload = BufPtr;
	}

	for (i = 0; i < numitems; i++)
	{
//		printf("Original buffer value = %08lx\n", payload[i]);
		payload[i] ^= keyvalue;
//		printf("Unkeyed buffer value = %08lx\n", payload[i]);
	}

	return(0);
}

/*******************************************************************************
;
;	Function Name:	BitUnp_rj
;	Contents:		unpack right justified data from bitstream 
;
*******************************************************************************/

int DolbyEFile::BitUnp_rj(					/* return error code.  0 = AOK */
	int dataPtr[],				/* IN/OUT: ptr to data array to be filled */
	int numitems, 				/* IN: # items to be unpacked  */
	int numbits)				/* IN: # bits per item */
{
	const unsigned int ljMask[32] =
	{	0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 
		0xf0000000, 0xf8000000, 0xfc000000, 0xfe000000, 
		0xff000000, 0xff800000, 0xffc00000, 0xffe00000, 
		0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 
		0xffff0000, 0xffff8000, 0xffffc000, 0xffffe000, 
		0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00, 
		0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 
		0xfffffff0, 0xfffffff8, 0xfffffffc, 0xfffffffe };
	int data, i;
	unsigned int ulsbdata;

//	printf("Unpacking %d items of %d bits each\n", numitems, numbits);

	if (BSWrdSz == X) return(BIT_ERR_NOINIT);

	if (BitCnt < (numitems * numbits))
	{
		return(BIT_ERR_UNDERFLOW);		/* underflow error */
	}

	for (i = 0; i < numitems; i++)
	{

/*	Unpack data as a left-justified element */

		data = (int)(((*BufPtr & ljMask[BSWrdSz]) << BitPtr) & ljMask[numbits]);
		BitPtr += numbits;
		while (BitPtr >= BSWrdSz)
		{
			BitPtr -= BSWrdSz;
			ulsbdata = (unsigned int)*++BufPtr;
			data |= ((ulsbdata >> (numbits - BitPtr)) & ljMask[numbits]);
		}

/*	Right-justify the element and store to output array */

		*dataPtr++ = (int)((unsigned int)(data) >> (FileWrdSz*8 - numbits));
//		printf("Item = 0x%06x\n", *(dataPtr - 1));
	}

	BitCnt -= numbits * numitems;
	decBitCntrs(numbits * numitems);
//	printf("Bit count = %d\n", BitCnt);

	return(BIT_ERR_NONE);
}	/* BitUnp_rj() */

/*******************************************************************************
;
;	Function Name:	skipbits
;	Contents:		skip past bits in bitstream
;
*******************************************************************************/

int DolbyEFile::SkipBits(				/* return error code.  0 = AOK */
	int numbits)			/* IN: # bits to skip */
{
	if (BSWrdSz == X) return(BIT_ERR_NOINIT);

	if (BitCnt < numbits)
	{
		return(BIT_ERR_UNDERFLOW);		/* underflow error */
	}

	BitPtr += numbits;
	while (BitPtr >= BSWrdSz)
	{
		BufPtr++;
		BitPtr -= BSWrdSz;
	}

	BitCnt -= numbits;
	decBitCntrs(numbits);

	return(BIT_ERR_NONE);
}	/* skipbits() */

/*******************************************************************************
;
;	Function Name:	SetDnCntr
;	Contents:		set local bit counter
;
*******************************************************************************/

int DolbyEFile::SetDnCntr(				/* return error code.  0 = AOK */
	int counterNum,			/* IN: counter number to set */
	int cnt)				/* IN: counter setting */
{

	if (counterNum>N_DOWN_CNTRS)
	{
		return 25;			/* illegal counter number */
	}
//	if (DnCntr[counterNum] != 0)
//	{
//		return 26;			/* counter in use */
//	}
	DnCntr[counterNum] = cnt;

	return (errAOK);
}	/* skipbits() */

/*******************************************************************************
;
;	Function Name:	GetDnCntr
;	Contents:		get local bit counter
;
*******************************************************************************/

int DolbyEFile::GetDnCntr(				/* return bit count, -1 if error */
	int counterNum)			/* OUT: counter setting */
{

	if (counterNum>N_DOWN_CNTRS)
	{
		return -1;			/* illegal counter number */
	}
	return DnCntr[counterNum];
}	/* skipbits() */


/*******************************************************************************
;
;	Function Name:	decBitCntrs
;	Contents:		decrement local bit counters
;
*******************************************************************************/

int DolbyEFile::decBitCntrs(			/* return error code.  0 = AOK */
	int numbits)			/* IN: # bits to decrement from counters */
{
	int i;

	for (i=0; i<N_DOWN_CNTRS; i++)
	{
		if (DnCntr[i] != 0)
			DnCntr[i] -= numbits;
		if (DnCntr[i] < 0)
			return 530987;
	}
	return errAOK;
}
