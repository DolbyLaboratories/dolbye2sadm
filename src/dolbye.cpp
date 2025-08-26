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

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ddeinfo.h"
#include "dolbye_file.h"
#include "dolbye_parser.h"

/* Helpers */

static int init_channel_subseg_info(ChannelSubsegInfoStruct *cip);
static double GetComprDB(int value);
static int check_time_code(int *current_tc, int *last_tc, int frame_rate);

int DolbyEParser::compare_frameinfo(FrameInfoStruct *info1, FrameInfoStruct *info2)
{
    int *intptr1, *intptr2;
    unsigned long int i;

    intptr1 = (int *)info1;
    intptr2 = (int *)info2;

    /* Test for non-sequential frame_count between frames */
    if(info1->frame_count != (unsigned short)(info2->frame_count + 1))
    {
        printf("\n\n** Non-sequential frame count occured after frame %i **",info2->frame_count);
        printf("\n** Transition from %i to %i **\n\n",info2->frame_count, info1->frame_count);
    }

    if( check_time_code(info1->timecode, info2->timecode, info1->frameRate) )
    {
        //non-sequential time code
        printf("** Non-sequential time code occured after frame %i **",info2->frame_count);
        printf("\n** Transition from");
        printf(" %02d:%02d:%02d:%02d (%s) to",
                ((((info2->timecode[1] >> 4) & 0x03) * 10) + (info2->timecode[1] & 0x0f)),
                ((((info2->timecode[3] >> 4) & 0x07) * 10) + (info2->timecode[3] & 0x0f)),
                ((((info2->timecode[5] >> 4) & 0x07) * 10) + (info2->timecode[5] & 0x0f)),
                ((((info2->timecode[7] >> 4) & 0x03) * 10) + (info2->timecode[7] & 0x0f)),
                timeCodeText[(info2->timecode[7] >> 6) & 0x01]);
        printf(" %02d:%02d:%02d:%02d (%s) **\n\n",
                ((((info1->timecode[1] >> 4) & 0x03) * 10) + (info1->timecode[1] & 0x0f)),
                ((((info1->timecode[3] >> 4) & 0x07) * 10) + (info1->timecode[3] & 0x0f)),
                ((((info1->timecode[5] >> 4) & 0x07) * 10) + (info1->timecode[5] & 0x0f)),
                ((((info1->timecode[7] >> 4) & 0x03) * 10) + (info1->timecode[7] & 0x0f)),
                timeCodeText[(info1->timecode[7] >> 6) & 0x01]);
    }

    for (i = 0; i < ((sizeof(FrameInfoStruct) / sizeof(int)) - MAX_NCHANS - 12 - MAX_NPGRMS - 16); i++)
    {   /*compare everying but prevGroupTypeCode, frame_count, metadata_crc, 
            metadata_extension_crc, metadata key, timecode and meter segment*/
        if ((*intptr1++) != (*intptr2++)) return(1);
    }    

    return(0);
}

/*****************************************************************************
*    findPreambleSync: find frame preamble sync
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*        fip->frameLength    set based on preamble (only valid if no error)
*        fip->wordSz            set based on preamble (only valid if no error)
*****************************************************************************/

int DolbyEParser::findPreambleSync(FrameInfoStruct *fip)
{

    int payloadSz;                /* in words */
    int bitdepth, err;
    int preamble[PREAMBLE_SZ], i;

/*    Search for preamble sync */
/*    Preamble format is as follows:
        sync a            0xf872 (16) or 0x6f872 (20) or 0x96f872 (24)
        sync b            0x4e1f (16) or 0x54e1f (20) or 0xa54e1f (24)
        burst info        strmnum:3, typedata:5, err:1, mode:2, type:5
        length            payload length in bits
*/

    if ((err = dolbyEFile.InitStream(MAX_BITDEPTH))) return(err);
    if ((err = dolbyEFile.ReadFile(PREAMBLE_SZ))) return(err);
    if ((err = dolbyEFile.BitUnp_rj(preamble, PREAMBLE_SZ, MAX_BITDEPTH))) return(err);

    while (1)
    {

/*    Test for sync for each possible bit depth */

        for (i = 0; i < MAX_BITDEPTH; i++)
        {
            if (((preamble[0] & maskSync[i]) == preambleSyncA[i])
                && ((preamble[1] & maskSync[i]) == preambleSyncB[i]))
            {
                if ((preamble[2] & maskType) != preambleDolbyE)
                {
                    printf("Warning: Not Dolby E bitstream\n");
                }
                else if ((preamble[2] & maskMode) != preambleMode[i])
                {
                    printf("Warning: Inconsistent preamble data mode\n");
                }
                else if ((preamble[2] & maskErr) != preambleNoErr)
                {
                    printf("Warning: Error flag set\n");
                }
                else if ((preamble[2] & maskStrmNum) != preambleStrm0)
                {
                    printf("Warning: Only stream #0 supported\n");
                }
                else
                {
                    bitdepth = bitDepthTab[i];
                    payloadSz = preamble[3] >> (MAX_BITDEPTH - bitdepth);
                    if (((payloadSz / bitdepth) * bitdepth) != payloadSz)
                    {
                        printf("Error: Inconsistent preamble payload size\n");
                        return(-1);
                    }
                    else
                    {
                        fip->wordSz = bitdepth;
                        fip->frameLength = payloadSz / bitdepth;
                        if ((err = dolbyEFile.InitStream(bitdepth))) return(err);
                        if ((err = dolbyEFile.ReadFile(payloadSz / bitdepth))) return(err);
                        return(0);
                    }
                }
            }
        }

        for (i = 0; i < (PREAMBLE_SZ - 1); i++)
        {
            preamble[i] = preamble[i + 1];
        }
        if ((err = dolbyEFile.ReadFile(1))) return(err);
        if ((err = dolbyEFile.BitUnp_rj(&preamble[PREAMBLE_SZ - 1], 1, MAX_BITDEPTH))) return(err);
    }

    return(-1);

}    /* findPreambleSync() */

/*****************************************************************************
*    Dolby_E_Frame: parse the overall Dolby E frame
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*        *fip                updated in sync and metadata segment parser
*****************************************************************************/

int DolbyEParser::Dolby_E_frame(FrameInfoStruct *fip)
{
    int err;

    if ((err = sync_segment(fip))) return(err);    
    if ((err = metadata_segment(fip))) return(err);
    if ((err = audio_segment(fip))) return(err);

    if (fip->lowFrameRate)
    {
        if ((err = metadata_extension_segment(fip))) return(err);
        if ((err = audio_extension_segment(fip))) return(err);
    }

    if ((err = meter_segment(fip))) return(err);

    return(0);
}

/*****************************************************************************
*    sync_segment: parse the sync segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*        fip->keyPresent        set based on sync word (only valid if no error)
*****************************************************************************/

int DolbyEParser::sync_segment(FrameInfoStruct *fip)
{
    int    err;

    /* sync word */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Sync.sync_word, 1, (fip->wordSz) - 1))) return(err);
    fip->Sync.sync_word <<= 1;

    switch (fip->wordSz)
    {
        case 16:            
            if (fip->Sync.sync_word != SYNC16) return(-1);
            break;
        case 20:            
            if (fip->Sync.sync_word != SYNC20) return(-1);
            break;
        case 24:            
            if (fip->Sync.sync_word != SYNC24) return(-1);
            break;
        default:
            return(-1);
    }

    /* key present */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Sync.key_present, 1, 1))) return(err);    
    fip->keyPresent = fip->Sync.key_present;
    
    fip->Sync.sync_word += fip->keyPresent;

    return(0);
}    /* sync_segment() */

/*****************************************************************************
*    display_sync_segment: display the sync segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*        fip->keyPresent        set based on sync word (only valid if no error)
*****************************************************************************/

int DolbyEParser::display_sync_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag)
{
    if (display_flag) printf("  Sync Segment\n");
        
    fprintf(xmlfp, "<Sync_Segment>\n");

    /* sync word */
    if (display_flag) printf("    Sync: ");
        
    fprintf(xmlfp, "<Sync>");
    

    switch (fip->wordSz)
    {
        case 16:
            if (display_flag) printf("0x%04x\n", fip->Sync.sync_word);            
            
            fprintf(xmlfp, "0x%04x", fip->Sync.sync_word);
            break;
        case 20:
            if (display_flag) printf("0x%05x\n", fip->Sync.sync_word);
                
            fprintf(xmlfp, "0x%05x", fip->Sync.sync_word);
            break;
        case 24:
            if (display_flag) printf("0x%06x\n", fip->Sync.sync_word);            
                
            fprintf(xmlfp, "0x%06x", fip->Sync.sync_word);
            break;
        default:
            return(-1);
    }
    fprintf(xmlfp, "</Sync>\n");

    /* key present */    
    if (display_flag) printf("    Key present: %s (%d)\n", yesNoText[fip->Sync.key_present], fip->Sync.key_present);
        
    fprintf(xmlfp, "<Key_present>%s</Key_present>\n", yesNoText[fip->Sync.key_present]);
    

    if (display_flag)
    {
        printf("    Bit depth: %d bits\n", fip->wordSz);
        printf("    Total frame length: %d words\n", fip->frameLength);
    }
    fprintf(xmlfp, "<Bit_depth>%d bits</Bit_depth>\n", fip->wordSz);
    fprintf(xmlfp, "<Total_frame_length>%d words</Total_frame_length>\n", fip->frameLength);
    fprintf(xmlfp, "</Sync_Segment>\n");

    return(0);
}    /* sync_segment() */

/*****************************************************************************
*    metadata_segment: parse the metadata segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*        *fip                updates remaining elements (only valid if no error)
*****************************************************************************/

int DolbyEParser::metadata_segment(FrameInfoStruct *fip)
{
    int    err, seg = 0;
    int pgm, ch;

    /* metadata key */
    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnp_rj(&fip->metadata_key, 1, fip->wordSz))) return(err);        
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, 1))) return(err);
    }

    /* metadata_revision_id */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.metadata_revision_id, 1, 4))) return(err);

    /* metadata_segment_size */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.metadata_segment_size, 1, 10))) return(err);
    if ((err = dolbyEFile.SetDnCntr(0, fip->wordSz * fip->Metadata.metadata_segment_size - 14))) return(err);

    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, fip->Metadata.metadata_segment_size))) return(err);
    }

    /* program_config */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.program_config, 1, 6))) return(err);
    if (fip->Metadata.program_config >= NPGMCFG)
    {
        printf("Error: invalid program config %d\n", fip->Metadata.program_config);
        return(-1);
    }

    fip->progConfig = fip->Metadata.program_config;
    fip->nProgs = nProgsTab[fip->Metadata.program_config];
    fip->nChans = nChansTab[fip->Metadata.program_config];
    fip->lfeChan = lfeChanTab[fip->Metadata.program_config];

    /* frame_rate_code */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.frame_rate_code, 1, 4))) return(err);
    if ((fip->Metadata.frame_rate_code == 0) || (fip->Metadata.frame_rate_code >= 9))
    {
        printf("Error: invalid frame rate %d\n", fip->Metadata.frame_rate_code);
        return(-1);
    }

    fip->frameRate = fip->Metadata.frame_rate_code;
    if (fip->frameRate <= 5)
    {
        fip->lowFrameRate = 1;
    }
    else
    {
        fip->lowFrameRate = 0;
    }

    /* original_frame_rate_code */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.original_frame_rate_code, 1, 4))) return(err);
    if ((fip->Metadata.original_frame_rate_code == 0) 
        || (fip->Metadata.original_frame_rate_code >= 9))
    {
        printf("Error: invalid original frame rate %d\n", fip->Metadata.original_frame_rate_code);
        return(-1);
    }

    /* frame_count */
    if ((err = dolbyEFile.BitUnp_rj(&fip->frame_count, 1, 16))) return(err);

    /* SMPTE_time_code */
    if ((err = dolbyEFile.BitUnp_rj(fip->timecode, 8, 8))) return(err);

    /* metadata_reserved_bits */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.metadata_reserved_bits, 1, 8))) return(err);

    /* channel_subsegment_size[ch] */
    if ((err = dolbyEFile.BitUnp_rj(fip->chanSubsegSz, fip->nChans, 10))) return(err);

    if (fip->lowFrameRate)
    {
        /* metadata_extension_segment_size */
        if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.metadata_extension_segment_size, 1, 8))) return(err);        
        fip->metaExtSz = fip->Metadata.metadata_extension_segment_size;
    }

    /* meter_segment_size */
    if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.meter_segment_size, 1, 8))) return(err);
    fip->meterSz = fip->Metadata.meter_segment_size;

    for (pgm = 0; pgm < fip->nProgs; pgm++)
    {
        /* description_text[pgm] */
        if ((err = dolbyEFile.BitUnp_rj(&fip->description_text[pgm], 1, 8))) return(err);
        switch (fip->description_text[pgm])
        {
            case 0x00:    
                null_char_warning[pgm] = 1;
                break;
            case 0x02:
                desc_text_ptr[pgm] = 0;
                break;
            case 0x03:
                description_text_buf[pgm][desc_text_ptr[pgm]] = '\0';
                break;
            default:
                if ((fip->description_text[pgm] < 0x20) || 
                    (fip->description_text[pgm] > 0x7e)) return(65535);        

                description_text_buf[pgm][desc_text_ptr[pgm]] = fip->description_text[pgm];
                desc_text_ptr[pgm]++;

                if(desc_text_ptr[pgm] >= MAX_DESCTEXTLEN)
                {
                    desc_text_length_error[pgm] = 1;
                    desc_text_ptr[pgm] = 0;
                    description_text_buf[pgm][MAX_DESCTEXTLEN - 1] = '\0';
                }
                break;
        }

        /* bandwidth_id[pgm] */
        if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.bandwidth_id[pgm], 1, 2))) return(err);
    }

    for (ch = 0; ch < fip->nChans; ch++)
    {
        /* revision_id[ch] */
        if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.revision_id[ch], 1, 4))) return(err);        

        /* bitpool_type[ch] */
        if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.bitpool_type[ch], 1, 1))) return(err);

        /* begin_gain[ch] */
        if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.begin_gain[ch], 1, 10))) return(err);

        /* end_gain[ch] */
        if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.end_gain[ch], 1, 10))) return(err);        
    }

    while (1)
    {
        /* metadata_subsegment_id */
        if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.metadata_subsegment_id[seg], 1, 4))) return(err);

        if (fip->Metadata.metadata_subsegment_id[seg] == 0) break;        
        
        if(fip->Metadata.metadata_subsegment_id[seg] < 3) 
        {
            /* metadata_subsegment_length */
            if ((err = dolbyEFile.BitUnp_rj(&fip->Metadata.metadata_subsegment_length[seg], 1, 12))) return(err);            

            if ((err = dolbyEFile.SetDnCntr(1, fip->Metadata.metadata_subsegment_length[seg]))) return(err);
            ac3_metadata_subsegment(fip, fip->Metadata.metadata_subsegment_id[seg]);
            fip->Metadata.unused_metadata_subsegment_bits[seg] = dolbyEFile.GetDnCntr(1);
            if ((err = dolbyEFile.SkipBits(fip->Metadata.unused_metadata_subsegment_bits[seg]))) return(err);
        }
        else
        {
            return(-1);
        }

        seg++;
    }

    /* unused_metadata_bits */
    fip->Metadata.unused_metadata_bits = dolbyEFile.GetDnCntr(0);    
    if ((err = dolbyEFile.SkipBits(fip->Metadata.unused_metadata_bits))) return(err);                        
    
        /* metadata_crc */
        dolbyEFile.BitUnp_rj(&fip->metadata_crc, 1, fip->wordSz);
    
        return(0);
    }    /* metadata_segment() */
    
    /*****************************************************************************
    *    display_metadata_segment: display the metadata segment
    *
    *    inputs:
    *        fip                    pointer to frame info structure
    *
    *    outputs:
    *        return value        0 if no error, nonzero if error
    *        *fip                updates remaining elements (only valid if no error)
    *****************************************************************************/
    
    int DolbyEParser::display_metadata_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag)
    {
        int pgm, ch, seg = 0;
    
        if (display_flag) printf("  Metadata Segment\n");
            
        fprintf(xmlfp, "<Metadata_Segment>\n");
    
        /* metadata key */
        if (fip->keyPresent)
        {
            if (display_flag) printf("    Metadata key: ");
            
            fprintf(xmlfp, "<Metadata_key>");
            switch (fip->wordSz)
            {
                case 16:
                    if (display_flag) printf("0x%04x\n", fip->metadata_key);
                    
                    fprintf(xmlfp, "0x%04x", fip->metadata_key);
                    break;
                case 20:
                    if (display_flag) printf("0x%05x\n", fip->metadata_key);
                    
                    fprintf(xmlfp, "0x%05x", fip->metadata_key);
                    break;
                case 24:
                    if (display_flag) printf("0x%06x\n", fip->metadata_key);
                    
                    fprintf(xmlfp, "0x%06x", fip->metadata_key);
                    break;
                default:
                    return(-1);
            }
            fprintf(xmlfp, "</Metadata_key>\n");
        }
    
        /* metadata_revision_id */
        if (display_flag) printf("    Metadata revision id: %d\n", fip->Metadata.metadata_revision_id);
        
        fprintf(xmlfp, "<Metadata_revision_id>%d</Metadata_revision_id>\n", fip->Metadata.metadata_revision_id);
    
        /* metadata_segment_size */
        if (display_flag) printf("    Metadata segment size: %d words\n", fip->Metadata.metadata_segment_size);
            
        fprintf(xmlfp, "<Metadata_segment_size>%d words</Metadata_segment_size>\n", fip->Metadata.metadata_segment_size);
    
    
        /* program_config */
        if (display_flag) printf("    Program config: %s (%d)\n", progConfigText[fip->Metadata.program_config], fip->Metadata.program_config);
        
        fprintf(xmlfp, "<Program_config>%s (%d)</Program_config>\n", 
                progConfigText[fip->Metadata.program_config], fip->Metadata.program_config);
    
    
        /* frame_rate_code */
        if (display_flag) printf("    Frame rate: %s (%d)\n", frameRateText[fip->Metadata.frame_rate_code - 1], fip->Metadata.frame_rate_code);
    
        fprintf(xmlfp, "<Frame_rate>%s (%d)</Frame_rate>\n", 
                frameRateText[fip->Metadata.frame_rate_code - 1], fip->Metadata.frame_rate_code);
    
        /* original_frame_rate_code */
        if (display_flag) printf("    Original frame rate: %s (%d)\n", frameRateText[fip->Metadata.original_frame_rate_code - 1], fip->Metadata.original_frame_rate_code);
        
        fprintf(xmlfp, "<Original_frame_rate>%s (%d)</Original_frame_rate>\n", 
                frameRateText[fip->Metadata.original_frame_rate_code - 1], fip->Metadata.original_frame_rate_code);
    
    
        /* frame_count */
        if (display_flag) printf("    Frame count: 0x%04x\n", fip->frame_count);
            
        fprintf(xmlfp, "<Frame_count>0x%04x</Frame_count>\n", fip->frame_count);
    
    
        /* SMPTE_time_code */
        fprintf(xmlfp, "<SMPTE_time_code>");
        if ((fip->timecode[1] & 0x3f) == 0x3f)         
             fprintf(xmlfp, "invalid");
        else
            fprintf(xmlfp, "%02d:%02d:%02d:%02d (%s)",
                    ((((fip->timecode[1] >> 4) & 0x03) * 10) + (fip->timecode[1] & 0x0f)),
                    ((((fip->timecode[3] >> 4) & 0x07) * 10) + (fip->timecode[3] & 0x0f)),
                    ((((fip->timecode[5] >> 4) & 0x07) * 10) + (fip->timecode[5] & 0x0f)),
                    ((((fip->timecode[7] >> 4) & 0x03) * 10) + (fip->timecode[7] & 0x0f)),
                    timeCodeText[(fip->timecode[7] >> 6) & 0x01]);
        fprintf(xmlfp, "</SMPTE_time_code>\n");
        
        if (display_flag)
        {
            printf("    SMPTE time code: ");
            
    
            if ((fip->timecode[1] & 0x3f) == 0x3f)
            {
                printf("invalid\n");
            }
            else
            {
                printf("%02d:%02d:%02d:%02d (%s)\n",
                    ((((fip->timecode[1] >> 4) & 0x03) * 10) + (fip->timecode[1] & 0x0f)),
                    ((((fip->timecode[3] >> 4) & 0x07) * 10) + (fip->timecode[3] & 0x0f)),
                    ((((fip->timecode[5] >> 4) & 0x07) * 10) + (fip->timecode[5] & 0x0f)),
                    ((((fip->timecode[7] >> 4) & 0x03) * 10) + (fip->timecode[7] & 0x0f)),
                    timeCodeText[(fip->timecode[7] >> 6) & 0x01]);
    
            }
        }
    
        /* metadata_reserved_bits */
        if (display_flag) printf("    Metadata reserved bits: 0x%02x\n", fip->Metadata.metadata_reserved_bits);
                
        fprintf(xmlfp, "<Metadata_reserved_bits>0x%02x</Metadata_reserved_bits>\n", fip->Metadata.metadata_reserved_bits);
    
    
        /* channel_subsegment_size[ch] */
        if (display_flag) printf("    Channel subsegment sizes\n");
        
        fprintf(xmlfp, "<Channel_subsegment_sizes>\n");
    
        for (ch = 0; ch < fip->nChans; ch++)
        {
            if (display_flag) printf("      Channel %d (%s): %d words\n", ch,    chanIDText[fip->progConfig][ch], fip->chanSubsegSz[ch]);
                
            fprintf(xmlfp, "<Channel_%d_%s>%d words</Channel_%d_%s>\n", ch,    chanIDText[fip->progConfig][ch], fip->chanSubsegSz[ch], ch, chanIDText[fip->progConfig][ch]);
        }
        fprintf(xmlfp, "</Channel_subsegment_sizes>\n");
    
        if (fip->lowFrameRate)
        {
            /* metadata_extension_segment_size */        
            if (display_flag) printf("    Metadata extension segment size: %d words\n", fip->Metadata.metadata_extension_segment_size);        
                
            fprintf(xmlfp, "<Metadata_extension_segment_size>%d words</Metadata_extension_segment_size>\n", fip->Metadata.metadata_extension_segment_size);        
    
        }
    
        /* meter_segment_size */
        if (display_flag) printf("    Meter segment size: %d words\n", fip->Metadata.meter_segment_size);
            
        fprintf(xmlfp, "<Meter_segment_size>%d words</Meter_segment_size>\n", fip->Metadata.meter_segment_size);
           
    
        for (pgm = 0; pgm < fip->nProgs; pgm++)
        {
            if (display_flag) printf("    Program %d metadata\n", pgm);
                
            fprintf(xmlfp, "<Program_%d_metadata>\n", pgm);
    
            /* description_text[pgm] */        
            if (display_flag) printf("      Description text: ");
                
            fprintf(xmlfp, "<Description_text>");
    
            switch (fip->description_text[pgm])
            {
                case 0x00:
                    if (display_flag) printf("NUL (0x00)\n"); 
                        
                    fprintf(xmlfp, "NUL (0x00)");
                    break;
                case 0x02:
                    if (display_flag) printf("STX (0x02)\n"); 
                        
                    fprintf(xmlfp, "STX (0x02)");
                    break;
                case 0x03:
                    if (display_flag) printf("ETX (0x03)\n"); 
                    
                    fprintf(xmlfp, "ETX (0x03)");
                    break;
                case 0x20:
                    if (display_flag) printf("SPACE (0x20)\n"); 
                    
                    fprintf(xmlfp, "SPACE (0x20)\n");
                    break;
                default:                
                    if (display_flag) printf("%c (0x%02x)\n", (char)fip->description_text[pgm], 
                        fip->description_text[pgm]); 
                        
                    fprintf(xmlfp, "%c (0x%02x)", (char)fip->description_text[pgm], 
                        fip->description_text[pgm]);
                    break;
            }
            fprintf(xmlfp, "</Description_text>\n");
    
            /* bandwidth_id[pgm] */        
            if (display_flag) printf("      Bandwidth id: %s (%d)\n",
                bandwidthIDText[fip->Metadata.bandwidth_id[pgm]], fip->Metadata.bandwidth_id[pgm]); 
                
            fprintf(xmlfp, "<Bandwidth_id>%s (%d)</Bandwidth_id>\n",
                bandwidthIDText[fip->Metadata.bandwidth_id[pgm]], fip->Metadata.bandwidth_id[pgm]);
    
            fprintf(xmlfp, "</Program_%d_metadata>\n", pgm);
        }
    
    
        for (ch = 0; ch < fip->nChans; ch++)
        {
            if (display_flag) printf("    Channel %d metadata\n", ch); 
            
            fprintf(xmlfp, "<Channel_%d_metadata>\n", ch);
            
    
            /* revision_id[ch] */        
            if (display_flag) printf("      Revision ID: %d\n", fip->Metadata.revision_id[ch]); 
                
            fprintf(xmlfp, "<Revision_ID>%d</Revision_ID>\n", fip->Metadata.revision_id[ch]);
            
            
            /* bitpool_type[ch] */        
            if (display_flag) printf("      Bitpool type: %s (%d)\n",
                bitpoolTypeText[fip->Metadata.bitpool_type[ch]], fip->Metadata.bitpool_type[ch]);
                
            fprintf(xmlfp, "<Bitpool_type>%s (%d)</Bitpool_type>\n",
                bitpoolTypeText[fip->Metadata.bitpool_type[ch]], fip->Metadata.bitpool_type[ch]);
            
    
            /* begin_gain[ch] */        
            if (display_flag) printf("      Begin gain: "); 
                
            fprintf(xmlfp, "<Begin_gain>");
            
    
            if (fip->Metadata.begin_gain[ch] == 0)
            {
                if (display_flag) printf("-inf dB (0x%03x)\n", fip->Metadata.begin_gain[ch]);
                
                fprintf(xmlfp, "-inf dB (0x%03x)", fip->Metadata.begin_gain[ch]);
            }
            else
            {
                if (display_flag) printf("%6.2f dB (0x%03x)\n",
                    0.094071873645 * (fip->Metadata.begin_gain[ch] - 0x3c0), 
                    fip->Metadata.begin_gain[ch]);
                    
                fprintf(xmlfp, "%6.2f dB (0x%03x)",
                    0.094071873645 * (fip->Metadata.begin_gain[ch] - 0x3c0), 
                    fip->Metadata.begin_gain[ch]);
            }
            fprintf(xmlfp, "</Begin_gain>\n");
    
            /* end_gain[ch] */        
            if (display_flag) printf("      End gain: "); 
            
            fprintf(xmlfp, "<End_gain>");
    
            if (fip->Metadata.end_gain[ch] == 0)
            {
                if (display_flag) printf("-inf dB (0x%03x)\n", fip->Metadata.end_gain[ch]);
                
                fprintf(xmlfp, "-inf dB (0x%03x)", fip->Metadata.end_gain[ch]);
            }
            else
            {
                if (display_flag) printf("%6.2f dB (0x%03x)\n",
                    0.094071873645 * (fip->Metadata.end_gain[ch] - 0x3c0), fip->Metadata.end_gain[ch]);
                
                fprintf(xmlfp, "%6.2f dB (0x%03x)",
                    0.094071873645 * (fip->Metadata.end_gain[ch] - 0x3c0), fip->Metadata.end_gain[ch]);
                
            }
            fprintf(xmlfp, "</End_gain>\n");
            fprintf(xmlfp, "</Channel_%d_metadata>\n", ch);
        }
    
        while (1)
        {
            /* metadata_subsegment_id */        
            if (display_flag) printf("    Metadata subsegment: %s (%d)\n",
                metaSubSegText[fip->Metadata.metadata_subsegment_id[seg]], 
                fip->Metadata.metadata_subsegment_id[seg]);
            
            fprintf(xmlfp, "<Metadata_subsegment_id>%s (%d)</Metadata_subsegment_id>\n",
                metaSubSegText[fip->Metadata.metadata_subsegment_id[seg]], 
                fip->Metadata.metadata_subsegment_id[seg]);
            
    
            if (fip->Metadata.metadata_subsegment_id[seg] == 0) break;        
    
            if(fip->Metadata.metadata_subsegment_id[seg] < 3)
            {
                if (display_flag) printf("    Metadata subsegment length: %d bits\n",
                    fip->Metadata.metadata_subsegment_length[seg]); 
                
                fprintf(xmlfp, "<Metadata_subsegment_length>%d bits</Metadata_subsegment_length>\n",
                    fip->Metadata.metadata_subsegment_length[seg]);
                    display_ac3_metadata_subsegment(xmlfp, fip, display_flag, 
                    fip->Metadata.metadata_subsegment_id[seg]);
                
                if (display_flag) printf("    Unused metadata subsegment length: %d bits\n", 
                    fip->Metadata.unused_metadata_subsegment_bits[seg]); 
                    
                fprintf(xmlfp, "<Unused_metadata_subsegment_length>%d bits</Unused_metadata_subsegment_length>\n", 
                    fip->Metadata.unused_metadata_subsegment_bits[seg]);
            }
            else
            {
                return(-1);
            }    
    
            seg++;
        }
    
        /* unused_metadata_bits */
        if (display_flag) printf("    Unused metadata segment length: %d bits\n", 
            fip->Metadata.unused_metadata_bits);                     
        fprintf(xmlfp, "<Unused_metadata_segment_length>%d bits</Unused_metadata_segment_length>\n", 
            fip->Metadata.unused_metadata_bits);
        
    
        /* metadata_crc */
        if (display_flag) printf("    Metadata CRC: ");
            
        fprintf(xmlfp, "<Metadata_CRC>");
        
        
        switch (fip->wordSz)
        {
            case 16:
                if (display_flag) printf("0x%04x\n", fip->metadata_crc);
                    
                fprintf(xmlfp, "0x%04x", fip->metadata_crc);
                break;
            case 20:
                if (display_flag) printf("0x%05x\n", fip->metadata_crc);
                    
                fprintf(xmlfp, "0x%05x", fip->metadata_crc);
                break;
            case 24:
                if (display_flag) printf("0x%06x\n", fip->metadata_crc);
                    
                fprintf(xmlfp, "0x%06x", fip->metadata_crc);
                break;
            default:
                return(-1);
        }
        fprintf(xmlfp, "</Metadata_CRC>\n"); 
        fprintf(xmlfp, "</Metadata_Segment>\n");
        
        return(0);
    }    /* display_metadata_segment() */
    
    /*****************************************************************************
    *    ac3_metadata_segment: parse the AC-3 metadata segment
    *
    *    inputs:
    *        fip                    pointer to frame info structure
    *
    *    outputs:
    *        return value        0 if no error, nonzero if error
    *****************************************************************************/
    int DolbyEParser::ac3_metadata_subsegment(FrameInfoStruct *fip, int subseg_id)
    {
        int    err;
        int pgm, i;
    
        for (pgm = 0; pgm < fip->nProgs; pgm++)
        {
            /* ac3_datarate */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_datarate[pgm], 1, 5))) return(err);

        /* ac3_bsmod */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_bsmod[pgm], 1, 3))) return(err);

        /* ac3_acmod */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_acmod[pgm], 1, 3))) return(err);

        /* ac3_cmixlev */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_cmixlev[pgm], 1, 2))) return(err);

        /* ac3_surmixlev */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_surmixlev[pgm], 1, 2))) return(err);

        /* ac3_dsurmod */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dsurmod[pgm], 1, 2))) return(err);

        /* ac3_lfeon */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_lfeon[pgm], 1, 1))) return(err);

        /* ac3_dialnorm */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dialnorm[pgm], 1, 5))) return(err);

        /* ac3_langcode */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_langcode[pgm], 1, 1))) return(err);

        /* ac3_langcod */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_langcod[pgm], 1, 8))) return(err);

        /* ac3_audprodie */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_audprodie[pgm], 1, 1))) return(err);

        /* ac3_mixlevel */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_mixlevel[pgm], 1, 5))) return(err);

        /* ac3_roomtyp */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_roomtyp[pgm], 1, 2))) return(err);        

        /* ac3_copyrightb */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_copyrightb[pgm], 1, 1))) return(err);

        /* ac3_origbs */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_origbs[pgm], 1, 1))) return(err);

        if (subseg_id == 1)  /* XBSI metadata */
          {
            /* xbsi1e */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_xbsi1e[pgm], 1, 1))) return(err);
            
            /* dmixmod */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dmixmod[pgm], 1, 2))) return(err);     

            /* ltrtcmixlev */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_ltrtcmixlev[pgm], 1, 3))) return(err);

            /* ltrtsurmixlev */                         
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_ltrtsurmixlev[pgm], 1, 3))) return(err);

            /* lorocmixlev */                         
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_lorocmixlev[pgm], 1, 3))) return(err);

            /* lorosurmixlev */                         
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_lorosurmixlev[pgm], 1, 3))) return(err);

            /* xbsi2e */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_xbsi2e[pgm], 1, 1))) return(err);

            /* dsurexmod */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dsurexmod[pgm], 1, 2))) return(err);

            /* dheadphonmod */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dheadphonmod[pgm], 1, 2))) return(err);
            
            /* adconvtyp */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_adconvtyp[pgm], 1, 1))) return(err);

            /* xbsi2  */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_xbsi2[pgm], 1, 8))) return(err);
            
            /* encinfo */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_encinfo[pgm], 1, 1))) return(err);
          }
        
        else
        {
            /* ac3_timecod1e */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_timecod1e[pgm], 1, 1))) return(err);
            
            /* ac3_timecod1 */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_timecod1[pgm], 1, 14))) return(err);
            
            /* ac3_timecod2e */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_timecod2e[pgm], 1, 1))) return(err);
            
            /* ac3_timecod2 */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_timecod2[pgm], 1, 14))) return(err);            
          }
        
        /* ac3_hpfon */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_hpfon[pgm], 1, 1))) return(err);

        /* ac3_bwlpfon */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_bwlpfon[pgm], 1, 1))) return(err);

        /* ac3_lfelpfon */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_lfelpfon[pgm], 1, 1))) return(err);

        /* ac3_sur90on */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_sur90on[pgm], 1, 1))) return(err);

        /* ac3_suratton */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_suratton[pgm], 1, 1))) return(err);

        /* ac3_rfpremphon */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_rfpremphon[pgm], 1, 1))) return(err);

        /* ac3_compre */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_compre[pgm], 1, 1))) return(err);

        /* ac3_compr1 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_compr1[pgm], 1, 8))) return(err);

        /* ac3_dynrnge */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dynrnge[pgm], 1, 1))) return(err);

        /* ac3_dynrng1 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dynrng1[pgm], 1, 8))) return(err);

        /* ac3_dynrng2 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dynrng2[pgm], 1, 8))) return(err);

        /* ac3_dynrng3 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dynrng3[pgm], 1, 8))) return(err);

        /* ac3_dynrng4 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_dynrng4[pgm], 1, 8))) return(err);
    }

    for (pgm = 0; pgm < fip->nProgs; pgm++)
    {
        /* ac3_addbsie */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_addbsie[pgm], 1, 1))) return(err);

        if (fip->AC3Metadata.ac3_addbsie[pgm])
        {
            /* ac3_addbsil */
            if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_addbsil[pgm], 1, 6))) return(err);
            fip->AC3Metadata.ac3_addbsil[pgm]++;

            for (i = 0; i < fip->AC3Metadata.ac3_addbsil[pgm]; i++)
            {
                /* ac3_addbsi */
                if ((err = dolbyEFile.BitUnp_rj(&fip->AC3Metadata.ac3_addbsi[pgm][i], 1, 8))) return(err);                
            }
        }
    }

    return(0);
}    /* ac3_metadata_subsegment() */

/*****************************************************************************
*    display_ac3_metadata_segment: display the AC-3 metadata segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/
int DolbyEParser::display_ac3_metadata_subsegment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag, int subseg_id)
{
    int pgm, i;

    if (display_flag) printf("      AC-3 Metadata Subsegment\n");
        
    fprintf(xmlfp, "<AC3_Metadata_Subsegment>");
    

    for (pgm = 0; pgm < fip->nProgs; pgm++)
    {
        if (display_flag) printf("        Program %d AC-3 metadata\n", pgm);
            
        fprintf(xmlfp, "<Program_%d_AC3_metadata>\n", pgm);
        
        /* ac3_datarate */        
        if (display_flag) printf("          AC-3 datarate: %s (%d)\n",
            AC3datarateText[fip->AC3Metadata.ac3_datarate[pgm]], fip->AC3Metadata.ac3_datarate[pgm]);
            
        fprintf(xmlfp, "<AC3_datarate>%s (%d)</AC3_datarate>\n",
            AC3datarateText[fip->AC3Metadata.ac3_datarate[pgm]], fip->AC3Metadata.ac3_datarate[pgm]);
        
        /* ac3_bsmod */        
        if((fip->AC3Metadata.ac3_acmod[pgm] >= 2) && (fip->AC3Metadata.ac3_bsmod[pgm] == 7)) {
                if (display_flag) printf("          AC-3 bsmod: %s (%d)\n",
                    AC3bsmodText[fip->AC3Metadata.ac3_bsmod[pgm] + 1], fip->AC3Metadata.ac3_bsmod[pgm]);
                
                fprintf(xmlfp, "<AC3_bsmod>%s (%d)</AC3_bsmod>\n",
                    AC3bsmodText[fip->AC3Metadata.ac3_bsmod[pgm] + 1], fip->AC3Metadata.ac3_bsmod[pgm]);
        }
        else {
                if(display_flag) printf("          AC-3 bsmod: %s (%d)\n",
                    AC3bsmodText[fip->AC3Metadata.ac3_bsmod[pgm]], fip->AC3Metadata.ac3_bsmod[pgm]);
                
                fprintf(xmlfp, "<AC3_bsmod>%s (%d)</AC3_bsmod>\n",
                    AC3bsmodText[fip->AC3Metadata.ac3_bsmod[pgm]], fip->AC3Metadata.ac3_bsmod[pgm]);
        }
        

        /* ac3_acmod */        
        if (display_flag) printf("          AC-3 acmod: %s (%d)\n",
            AC3acmodText[fip->AC3Metadata.ac3_acmod[pgm]], fip->AC3Metadata.ac3_acmod[pgm]);
            
        fprintf(xmlfp, "<AC3_acmod>%s (%d)</AC3_acmod>\n",
            AC3acmodText[fip->AC3Metadata.ac3_acmod[pgm]], fip->AC3Metadata.ac3_acmod[pgm]);
        

        /* ac3_cmixlev */        
        if (display_flag) printf("          AC-3 cmixlev: %s (%d)\n",
            AC3cmixlevText[fip->AC3Metadata.ac3_cmixlev[pgm]], fip->AC3Metadata.ac3_cmixlev[pgm]);
            
        fprintf(xmlfp, "<AC3_cmixlev>%s (%d)</AC3_cmixlev>\n",
            AC3cmixlevText[fip->AC3Metadata.ac3_cmixlev[pgm]], fip->AC3Metadata.ac3_cmixlev[pgm]);
        

        /* ac3_surmixlev */        
        if (display_flag) printf("          AC-3 surmixlev: %s (%d)\n",
            AC3surmixlevText[fip->AC3Metadata.ac3_surmixlev[pgm]], 
            fip->AC3Metadata.ac3_surmixlev[pgm]);
            
        fprintf(xmlfp, "<AC3_surmixlev>%s (%d)</AC3_surmixlev>\n",
            AC3surmixlevText[fip->AC3Metadata.ac3_surmixlev[pgm]], 
            fip->AC3Metadata.ac3_surmixlev[pgm]);
        

        /* ac3_dsurmod */    
        if (display_flag) printf("          AC-3 dsurmod: %s (%d)\n",
            AC3dsurmodText[fip->AC3Metadata.ac3_dsurmod[pgm]], fip->AC3Metadata.ac3_dsurmod[pgm]);
            
        fprintf(xmlfp, "<AC3_dsurmod>%s (%d)</AC3_dsurmod>\n",
            AC3dsurmodText[fip->AC3Metadata.ac3_dsurmod[pgm]], fip->AC3Metadata.ac3_dsurmod[pgm]);
        
        
        /* ac3_lfeon */        
        if (display_flag) printf("          AC-3 lfeon: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_lfeon[pgm]], fip->AC3Metadata.ac3_lfeon[pgm]);
            
        fprintf(xmlfp, "<AC3_lfeon>%s (%d)</AC3_lfeon>\n",
            onOffText[fip->AC3Metadata.ac3_lfeon[pgm]], fip->AC3Metadata.ac3_lfeon[pgm]);
        
        
        /* ac3_dialnorm */        
        if (display_flag) printf("          AC-3 dialnorm: ");
            
        fprintf(xmlfp, "<AC3_dialnorm>");
        
        if (fip->AC3Metadata.ac3_dialnorm[pgm] == 0)
        {
            if(display_flag) printf("reserved (%d)\n", fip->AC3Metadata.ac3_dialnorm[pgm]);
                
            fprintf(xmlfp, "reserved (%d)", fip->AC3Metadata.ac3_dialnorm[pgm]);
        }
        else
        {
            if(display_flag) printf("-%d dBFS (%d)\n", fip->AC3Metadata.ac3_dialnorm[pgm], 
                    fip->AC3Metadata.ac3_dialnorm[pgm]);
            
            fprintf(xmlfp, "-%d dBFS (%d)", fip->AC3Metadata.ac3_dialnorm[pgm], 
                    fip->AC3Metadata.ac3_dialnorm[pgm]);
        }
        fprintf(xmlfp, "</AC3_dialnorm>\n");
        

        /* ac3_langcode */        
        if (display_flag) printf("          AC-3 langcode: %s (%d)\n",
            yesNoText[fip->AC3Metadata.ac3_langcode[pgm]], fip->AC3Metadata.ac3_langcode[pgm]);
            
        fprintf(xmlfp, "<AC3_langcode>%s (%d)</AC3_langcode>\n",
            yesNoText[fip->AC3Metadata.ac3_langcode[pgm]], fip->AC3Metadata.ac3_langcode[pgm]);
        

        /* ac3_langcod */        
        if (display_flag) printf("          AC-3 langcod: 0x%02x\n", 
            fip->AC3Metadata.ac3_langcod[pgm]);
            
        fprintf(xmlfp, "<AC3_langcod>0x%02x</AC3_langcod>\n", 
            fip->AC3Metadata.ac3_langcod[pgm]);
        

        /* ac3_audprodie */        
        if (display_flag) printf("          AC-3 audprodie: %s (%d)\n",
            yesNoText[fip->AC3Metadata.ac3_audprodie[pgm]], fip->AC3Metadata.ac3_audprodie[pgm]);
            
        fprintf(xmlfp, "<AC3_audprodie>%s (%d)</AC3_audprodie>\n",
            yesNoText[fip->AC3Metadata.ac3_audprodie[pgm]], fip->AC3Metadata.ac3_audprodie[pgm]);
        
        /* ac3_mixlevel */        
        if (display_flag) printf("          AC-3 mixlevel: %d dB (%d)\n",
            80 + fip->AC3Metadata.ac3_mixlevel[pgm], fip->AC3Metadata.ac3_mixlevel[pgm]);
            
        fprintf(xmlfp, "<AC3_mixlevel>%d dB (%d)</AC3_mixlevel>\n",
            80 + fip->AC3Metadata.ac3_mixlevel[pgm], fip->AC3Metadata.ac3_mixlevel[pgm]);

        /* ac3_roomtyp */        
        if (display_flag) printf("          AC-3 roomtyp: %s (%d)\n",
            AC3roomtypText[fip->AC3Metadata.ac3_roomtyp[pgm]], fip->AC3Metadata.ac3_roomtyp[pgm]);
        
        fprintf(xmlfp, "<AC3_roomtyp>%s (%d)</AC3_roomtyp>\n",
            AC3roomtypText[fip->AC3Metadata.ac3_roomtyp[pgm]], fip->AC3Metadata.ac3_roomtyp[pgm]);
        
         
        /* ac3_copyrightb */        
        if (display_flag) printf("          AC-3 copyrightb: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_copyrightb[pgm]], fip->AC3Metadata.ac3_copyrightb[pgm]);
            
        fprintf(xmlfp, "<AC3_copyrightb>%s (%d)</AC3_copyrightb>\n",
            onOffText[fip->AC3Metadata.ac3_copyrightb[pgm]], fip->AC3Metadata.ac3_copyrightb[pgm]);
        
        
        /* ac3_origbs */        
        if (display_flag) printf("          AC-3 origbs: %s (%d)\n",
            yesNoText[fip->AC3Metadata.ac3_origbs[pgm]], fip->AC3Metadata.ac3_origbs[pgm]);
            
        fprintf(xmlfp, "<AC3_origbs>%s (%d)</AC3_origbs>\n",
            yesNoText[fip->AC3Metadata.ac3_origbs[pgm]], fip->AC3Metadata.ac3_origbs[pgm]);
        

        if (subseg_id == 1)  /* XBSI metadata */
        {
            /* xbsi1e */            
            if (display_flag) printf("         AC-3 xbsi1e: %s (%d)\n",
                         yesNoText[fip->AC3Metadata.ac3_xbsi1e[pgm]], 
                         fip->AC3Metadata.ac3_xbsi1e[pgm]);
            
            fprintf(xmlfp, "<AC3_xbsi1e>%s (%d)</AC3_xbsi1e>\n",
                         yesNoText[fip->AC3Metadata.ac3_xbsi1e[pgm]], 
                         fip->AC3Metadata.ac3_xbsi1e[pgm]);
            
            
            /* dmixmod */            
            if (display_flag) printf("         AC-3 dmixmod: %s (%d)\n",
                         AC3dmixmodText[fip->AC3Metadata.ac3_dmixmod[pgm]], 
                         fip->AC3Metadata.ac3_dmixmod[pgm]);
            
            fprintf(xmlfp, "<AC3_dmixmod>%s (%d)</AC3_dmixmod>\n",
                         AC3dmixmodText[fip->AC3Metadata.ac3_dmixmod[pgm]], 
                         fip->AC3Metadata.ac3_dmixmod[pgm]);                         
            

            /* ltrtcmixlev */           
            if (display_flag) printf("          AC-3 ltrtcmixlev: %s (%d)\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_ltrtcmixlev[pgm]], 
                         fip->AC3Metadata.ac3_ltrtcmixlev[pgm]);
            
            fprintf(xmlfp, "<AC3_ltrtcmixlev>%s (%d)</AC3_ltrtcmixlev>\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_ltrtcmixlev[pgm]], 
                         fip->AC3Metadata.ac3_ltrtcmixlev[pgm]);
            

            /* ltrtsurmixlev */             
            if (display_flag) printf("          AC-3 ltrtsurmixlev: %s (%d)\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_ltrtsurmixlev[pgm]], 
                         fip->AC3Metadata.ac3_ltrtsurmixlev[pgm]);
                         
            fprintf(xmlfp, "<AC3_ltrtsurmixlev>%s (%d)</AC3_ltrtsurmixlev>\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_ltrtsurmixlev[pgm]], 
                         fip->AC3Metadata.ac3_ltrtsurmixlev[pgm]);
            

            /* lorocmixlev */                 
            if (display_flag) printf("          AC-3 lorocmixlev: %s (%d)\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_lorocmixlev[pgm]], 
                         fip->AC3Metadata.ac3_lorocmixlev[pgm]);
            
            fprintf(xmlfp, "<AC3_lorocmixlev>%s (%d)</AC3_lorocmixlev>\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_lorocmixlev[pgm]], 
                         fip->AC3Metadata.ac3_lorocmixlev[pgm]);

            /* lorosurmixlev */    
            if (display_flag) printf("          AC-3 lorosurmixlev: %s (%d)\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_lorosurmixlev[pgm]], 
                         fip->AC3Metadata.ac3_lorosurmixlev[pgm]);
                         
            fprintf(xmlfp, "<AC3_lorosurmixlev>%s (%d)</AC3_lorosurmixlev>\n",
                         AC3newmixlevText[fip->AC3Metadata.ac3_lorosurmixlev[pgm]], 
                         fip->AC3Metadata.ac3_lorosurmixlev[pgm]);
            

            /* xbsi2e */
            if (display_flag) printf("         AC-3 xbsi2e: %s (%d)\n",
                         yesNoText[fip->AC3Metadata.ac3_xbsi2e[pgm]], 
                         fip->AC3Metadata.ac3_xbsi2e[pgm]);
                         
            fprintf(xmlfp, "<AC3_xbsi2e>%s (%d)</AC3_xbsi2e>\n",
                         yesNoText[fip->AC3Metadata.ac3_xbsi2e[pgm]], 
                         fip->AC3Metadata.ac3_xbsi2e[pgm]);
            

            /* dsurexmod */
            if (display_flag) printf("         AC-3 dsurexmod: %s (%d)\n",
                         AC3dsurexmodText[fip->AC3Metadata.ac3_dsurexmod[pgm]], 
                         fip->AC3Metadata.ac3_dsurexmod[pgm]);
                         
            fprintf(xmlfp, "<AC3_dsurexmod>%s (%d)</AC3_dsurexmod>\n",
                         AC3dsurexmodText[fip->AC3Metadata.ac3_dsurexmod[pgm]], 
                         fip->AC3Metadata.ac3_dsurexmod[pgm]);
            
            
            /* dheadphonmod */
            if (display_flag) printf("         AC-3 dheadphonmod: %s (%d)\n",
                         AC3dheadphonmodText[fip->AC3Metadata.ac3_dheadphonmod[pgm]], 
                         fip->AC3Metadata.ac3_dheadphonmod[pgm]);
                         
            fprintf(xmlfp, "<AC3_dheadphonmod>%s (%d)</AC3_dheadphonmod>\n",
                         AC3dheadphonmodText[fip->AC3Metadata.ac3_dheadphonmod[pgm]], 
                         fip->AC3Metadata.ac3_dheadphonmod[pgm]);
            
            
            /* adconvtyp */
            if (display_flag) printf("         AC-3 adconvtyp: %s (%d)\n",
                         AC3adconvtyp[fip->AC3Metadata.ac3_adconvtyp[pgm]], 
                         fip->AC3Metadata.ac3_adconvtyp[pgm]);
                         
            fprintf(xmlfp, "<AC3_adconvtyp>%s (%d)</AC3_adconvtyp>\n",
                         AC3adconvtyp[fip->AC3Metadata.ac3_adconvtyp[pgm]], 
                         fip->AC3Metadata.ac3_adconvtyp[pgm]);
            
            
            /* xbsi2  */
            if (display_flag) printf("         AC-3 xbsi2: 0x%04x\n", 
                fip->AC3Metadata.ac3_xbsi2[pgm]);
                
            fprintf(xmlfp, "<AC3_xbsi2>0x%04x</AC3_xbsi2>\n", 
                fip->AC3Metadata.ac3_xbsi2[pgm]);
            
            
            /* encinfo */
            if (display_flag) printf("         AC-3 encinfo: 0x%04x\n", 
                fip->AC3Metadata.ac3_encinfo[pgm]);
            
            fprintf(xmlfp, "<AC3_encinfo>0x%04x</AC3_encinfo>\n", 
                fip->AC3Metadata.ac3_encinfo[pgm]);
                
          }
          else
          {

            /* ac3_timecod1e */
            if (display_flag) printf("          AC-3 timecod1e: %s (%d)\n",
                         yesNoText[fip->AC3Metadata.ac3_timecod1e[pgm]], 
                         fip->AC3Metadata.ac3_timecod1e[pgm]);
                         
            fprintf(xmlfp, "<AC3_timecod1e>%s (%d)</AC3_timecod1e>\n",
                         yesNoText[fip->AC3Metadata.ac3_timecod1e[pgm]], 
                         fip->AC3Metadata.ac3_timecod1e[pgm]);
            
            
            /* ac3_timecod1 */
            if (display_flag) printf("          AC-3 timecod1: 0x%04x\n", 
                fip->AC3Metadata.ac3_timecod1[pgm]);
                
            fprintf(xmlfp, "<AC3_timecod1>0x%04x</AC3_timecod1>\n", 
                fip->AC3Metadata.ac3_timecod1[pgm]);
            
            
            /* ac3_timecod2e */
            if (display_flag) printf("          AC-3 timecod2e: %s (%d)\n",
                         yesNoText[fip->AC3Metadata.ac3_timecod2e[pgm]], 
                         fip->AC3Metadata.ac3_timecod2e[pgm]);
            
            fprintf(xmlfp, "<AC3_timecod2e>%s (%d)</AC3_timecod2e>\n",
                         yesNoText[fip->AC3Metadata.ac3_timecod2e[pgm]], 
                         fip->AC3Metadata.ac3_timecod2e[pgm]);
            
            
            /* ac3_timecod2 */
            if (display_flag) printf("          AC-3 timecod2: 0x%04x\n", 
                fip->AC3Metadata.ac3_timecod2[pgm]);
                
            fprintf(xmlfp, "<AC3_timecod2>0x%04x<AC3_timecod2>\n", 
                fip->AC3Metadata.ac3_timecod2[pgm]);
            
            
          }
        
        /* ac3_hpfon */
        if (display_flag) printf("          AC-3 high-pass filter: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_hpfon[pgm]], fip->AC3Metadata.ac3_hpfon[pgm]);
        
        fprintf(xmlfp, "<AC3_high_pass_filter> %s (%d)</AC3_high_pass_filter>\n",
            onOffText[fip->AC3Metadata.ac3_hpfon[pgm]], fip->AC3Metadata.ac3_hpfon[pgm]);
        

        /* ac3_bwlpfon */
        if (display_flag) printf("          AC-3 bandwidth low-pass filter: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_bwlpfon[pgm]], fip->AC3Metadata.ac3_bwlpfon[pgm]);
            
        fprintf(xmlfp, "<AC3_bandwidth_low_pass_filter>%s (%d)</AC3_bandwidth_low_pass_filter>\n",
            onOffText[fip->AC3Metadata.ac3_bwlpfon[pgm]], fip->AC3Metadata.ac3_bwlpfon[pgm]);
        

        /* ac3_lfelpfon */
        if (display_flag) printf("          AC-3 LFE low-pass filter: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_lfelpfon[pgm]], fip->AC3Metadata.ac3_lfelpfon[pgm]);
            
        fprintf(xmlfp, "<AC3_LFE_low_pass_filter>%s (%d)</AC3_LFE_low_pass_filter>\n",
            onOffText[fip->AC3Metadata.ac3_lfelpfon[pgm]], fip->AC3Metadata.ac3_lfelpfon[pgm]);
        

        /* ac3_sur90on */
        if (display_flag) printf("          AC-3 surround phase shift filter: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_sur90on[pgm]], fip->AC3Metadata.ac3_sur90on[pgm]);
            
        fprintf(xmlfp, "<AC3_surround_phase_shift_filter>%s (%d)</AC3_surround_phase_shift_filter>\n",
            onOffText[fip->AC3Metadata.ac3_sur90on[pgm]], fip->AC3Metadata.ac3_sur90on[pgm]);
        

        /* ac3_suratton */
        if (display_flag) printf("          AC-3 surround attenuation: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_suratton[pgm]], fip->AC3Metadata.ac3_suratton[pgm]);
            
        fprintf(xmlfp, "<AC3_surround_attenuation>%s (%d)</AC3_surround_attenuation>\n",
            onOffText[fip->AC3Metadata.ac3_suratton[pgm]], fip->AC3Metadata.ac3_suratton[pgm]);
        

        /* ac3_rfpremphon */
        if (display_flag) printf("          AC-3 RF overmodulation protection: %s (%d)\n",
            onOffText[fip->AC3Metadata.ac3_rfpremphon[pgm]], fip->AC3Metadata.ac3_rfpremphon[pgm]);
            
        fprintf(xmlfp, "<AC3_RF_overmodulation_protection>%s (%d)</AC3_RF_overmodulation_protection>\n",
            onOffText[fip->AC3Metadata.ac3_rfpremphon[pgm]], fip->AC3Metadata.ac3_rfpremphon[pgm]);
        
        /* ac3_compre */
        if (display_flag) printf("          AC-3 compre: %s (%d)\n",
            yesNoText[fip->AC3Metadata.ac3_compre[pgm]], fip->AC3Metadata.ac3_compre[pgm]);
            
        fprintf(xmlfp, "<AC3_compre>%s (%d)</AC3_compre>\n",
            yesNoText[fip->AC3Metadata.ac3_compre[pgm]], fip->AC3Metadata.ac3_compre[pgm]);
        
        
        /* ac3_compr1 */
            if (display_flag) printf("          AC-3 compr1: ");
            
            fprintf(xmlfp, "<AC3_compr1>");
            if (fip->AC3Metadata.ac3_compre[pgm])
            {
                if(display_flag) printf("%6.2f dB (%d)\n", GetComprDB(fip->AC3Metadata.ac3_compr1[pgm] * 2), 
                    fip->AC3Metadata.ac3_compr1[pgm]);
                
                fprintf(xmlfp, "%6.2f dB (%d)", GetComprDB(fip->AC3Metadata.ac3_compr1[pgm] * 2), 
                    fip->AC3Metadata.ac3_compr1[pgm]);
            }
            else
            {
                if (fip->AC3Metadata.ac3_compr1[pgm] < NCOMPPRESETS)
                {
                    if(display_flag) printf("%s preset (%d)\n", AC3compPresetText[fip->AC3Metadata.ac3_compr1[pgm]], 
                        fip->AC3Metadata.ac3_compr1[pgm]);
                    
                    fprintf(xmlfp, "%s preset (%d)", AC3compPresetText[fip->AC3Metadata.ac3_compr1[pgm]], 
                        fip->AC3Metadata.ac3_compr1[pgm]);
                }
                else
                {
                    if(display_flag) printf("undefined preset (%d)\n", fip->AC3Metadata.ac3_compr1[pgm]);
                    
                    fprintf(xmlfp, "undefined preset (%d)", fip->AC3Metadata.ac3_compr1[pgm]);
                }
            }
        fprintf(xmlfp, "</AC3_compr1>\n");
        

        /* ac3_dynrnge */
        if (display_flag) printf("          AC-3 dynrnge: %s (%d)\n",
            yesNoText[fip->AC3Metadata.ac3_dynrnge[pgm]], fip->AC3Metadata.ac3_dynrnge[pgm]);
            
        fprintf(xmlfp, "<AC3_dynrnge>%s (%d)</AC3_dynrnge>\n",
            yesNoText[fip->AC3Metadata.ac3_dynrnge[pgm]], fip->AC3Metadata.ac3_dynrnge[pgm]);

        /* ac3_dynrng1 */
            if (display_flag) printf("          AC-3 dynrng1: ");
            
            fprintf(xmlfp, "<AC3_dynrng1>");
            if (fip->AC3Metadata.ac3_dynrnge[pgm])
            {
                if(display_flag) printf("%6.2f dB (%d)\n", GetComprDB(fip->AC3Metadata.ac3_dynrng1[pgm]), 
                    fip->AC3Metadata.ac3_dynrng1[pgm]);
                
                fprintf(xmlfp, "%6.2f dB (%d)", GetComprDB(fip->AC3Metadata.ac3_dynrng1[pgm]), 
                    fip->AC3Metadata.ac3_dynrng1[pgm]);
            }
            else
            {
                if (fip->AC3Metadata.ac3_dynrng1[pgm] < NCOMPPRESETS)
                {
                    if(display_flag) printf("%s preset (%d)\n", AC3compPresetText[fip->AC3Metadata.ac3_dynrng1[pgm]], 
                        fip->AC3Metadata.ac3_dynrng1[pgm]);
                    
                    fprintf(xmlfp, "%s preset (%d)", AC3compPresetText[fip->AC3Metadata.ac3_dynrng1[pgm]], 
                        fip->AC3Metadata.ac3_dynrng1[pgm]);
                }
                else
                {
                    if(display_flag) printf("undefined preset (%d)\n", fip->AC3Metadata.ac3_dynrng1[pgm]);
                    
                    fprintf(xmlfp, "undefined preset (%d)", fip->AC3Metadata.ac3_dynrng1[pgm]);
                }
            }
            fprintf(xmlfp, "</AC3_dynrng1>\n");
        

        /* ac3_dynrng2 */
        if (display_flag) printf("          AC-3 dynrng2: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3Metadata.ac3_dynrng2[pgm]), fip->AC3Metadata.ac3_dynrng2[pgm]);
            
        fprintf(xmlfp, "<AC3_dynrng2>%6.2f dB (%d)</AC3_dynrng2>\n",
            GetComprDB(fip->AC3Metadata.ac3_dynrng2[pgm]), fip->AC3Metadata.ac3_dynrng2[pgm]);
        
        
        /* ac3_dynrng3 */
        if (display_flag) printf("          AC-3 dynrng3: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3Metadata.ac3_dynrng3[pgm]), fip->AC3Metadata.ac3_dynrng3[pgm]);
            
        fprintf(xmlfp, "<AC3_dynrng3>%6.2f dB (%d)</AC3_dynrng3>\n",
            GetComprDB(fip->AC3Metadata.ac3_dynrng3[pgm]), fip->AC3Metadata.ac3_dynrng3[pgm]);
        
        
        /* ac3_dynrng4 */
        if (display_flag) printf("          AC-3 dynrng4: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3Metadata.ac3_dynrng4[pgm]), fip->AC3Metadata.ac3_dynrng4[pgm]);
        
        fprintf(xmlfp, "<AC3_dynrng4>%6.2f dB (%d)</AC3_dynrng4>\n",
            GetComprDB(fip->AC3Metadata.ac3_dynrng4[pgm]), fip->AC3Metadata.ac3_dynrng4[pgm]);
       
        
        fprintf(xmlfp, "</Program_%d_AC3_metadata>\n", pgm);
    }

    for (pgm = 0; pgm < fip->nProgs; pgm++)
    {
        if (display_flag) printf("        Program %d AC-3 additional BSI metadata\n", pgm);
            
        fprintf(xmlfp, "<Program_%d_AC3_additional_BSI_metadata>\n", pgm);
        

        /* ac3_addbsie */
        if (display_flag) printf("          AC-3 addbsie: %s (%d)\n",
            yesNoText[fip->AC3Metadata.ac3_addbsie[pgm]], fip->AC3Metadata.ac3_addbsie[pgm]);
            
        fprintf(xmlfp, "<AC3_addbsie>%s (%d)</AC3_addbsie>\n",
            yesNoText[fip->AC3Metadata.ac3_addbsie[pgm]], fip->AC3Metadata.ac3_addbsie[pgm]);
        
        
        if (fip->AC3Metadata.ac3_addbsie[pgm])
        {
            /* ac3_addbsil */
            if (display_flag) printf("          AC-3 addbsil: %d words (%d)\n",
                fip->AC3Metadata.ac3_addbsil[pgm], fip->AC3Metadata.ac3_addbsil[pgm] - 1);
                
            fprintf(xmlfp, "<AC3_addbsil>%d words (%d)</AC3_addbsil>\n",
                fip->AC3Metadata.ac3_addbsil[pgm], fip->AC3Metadata.ac3_addbsil[pgm] - 1);
            
            
            if (display_flag) printf("          AC-3 addbsi:\n");
                
            fprintf(xmlfp, "<AC3_addbsi>");
            
            for (i = 0; i < fip->AC3Metadata.ac3_addbsil[pgm]; i++)
            {
                /* ac3_addbsi */
                if (display_flag) printf("            0x%02x\n", 
                    fip->AC3Metadata.ac3_addbsi[pgm][i]);
                fprintf(xmlfp, "<0x%02x>\n", 
                    fip->AC3Metadata.ac3_addbsi[pgm][i]);
            }
            
            fprintf(xmlfp, "</AC3_addbsi>\n");
        }
        fprintf(xmlfp, "</Program_%d_AC3_additional_BSI_metadata>\n", pgm);
    }

    fprintf(xmlfp, "</AC3_Metadata_Subsegment>\n");
    return(0);
}    /* display_ac3_metadata_subsegment() */

/*****************************************************************************
*    audio_segment: parse the audio segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::audio_segment(FrameInfoStruct *fip)
{
    ChannelSubsegInfoStruct chanInfo, *cip = &chanInfo;
    int    value, err;
    int ch, keycount;

    cip->lowFrameRate = fip->lowFrameRate;
    cip->priExtFlag = 0;

    /* audio_subsegment0_key */
    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz))) return(err);
        fip->metadata_key = value;
        keycount = 1;
        for (ch = 0; ch < fip->nChans/2; ch++)
        {
            keycount += fip->chanSubsegSz[ch];
        }
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, keycount))) return(err);
    }

    for (ch = 0; ch < fip->nChans/2; ch++)
    {
        cip->lfeFlag = (ch == fip->lfeChan);

        /* channel_subsegment[ch] */        
        if ((err = dolbyEFile.SetDnCntr(0, fip->chanSubsegSz[ch] * fip->wordSz))) return(err);
        if ((err = channel_subsegment(cip))) return(err);
        fip->prevGroupTypeCode[ch] = cip->groupTypeCode;
        value = dolbyEFile.GetDnCntr(0);        
        if ((err = dolbyEFile.SkipBits(value))) return(err);                        
    }

    /* audio_subsegment0_crc */
    dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz);

    /* audio_subsegment1_key */
    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz))) return(err);    
        fip->metadata_key = value;
        keycount = 1;
        for (ch = fip->nChans/2; ch < fip->nChans; ch++)
        {
            keycount += fip->chanSubsegSz[ch];
        }
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, keycount))) return(err);
    }

    for (ch = fip->nChans/2; ch < fip->nChans; ch++)
    {
        cip->lfeFlag = (ch == fip->lfeChan);

        /* channel_subsegment[ch] */    
        if ((err = dolbyEFile.SetDnCntr(0, fip->chanSubsegSz[ch] * fip->wordSz))) return(err);
        if ((err = channel_subsegment(cip))) return(err);
        fip->prevGroupTypeCode[ch] = cip->groupTypeCode;
        value = dolbyEFile.GetDnCntr(0);    
        if ((err = dolbyEFile.SkipBits(value))) return(err);                        
    }

    /* audio_subsegment1_crc */
    dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz);

    return(0);
}    /* audio_segment() */

/*****************************************************************************
*    metadata_extension_segment: parse the metadata extension segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::metadata_extension_segment(FrameInfoStruct *fip)
{
    int    err, seg = 0;

    /* metadata_extension_key */
    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnp_rj(&fip->MetadataExt.metadata_extension_key, 1, fip->wordSz))) return(err);
        fip->metadata_key = fip->MetadataExt.metadata_extension_key;
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, fip->metaExtSz + 1))) return(err);
    }

    if ((err = dolbyEFile.SetDnCntr(0, fip->metaExtSz * fip->wordSz))) return(err);

    while (1)
    {
        /* metadata_extension_subsegment_id */
        if ((err = dolbyEFile.BitUnp_rj(&fip->MetadataExt.metadata_extension_subsegment_id[seg], 1, 4)))
            return(err);

        if (fip->MetadataExt.metadata_extension_subsegment_id[seg] == 0){ break; }
        else if(fip->MetadataExt.metadata_extension_subsegment_id[seg] < 3)
        {
            /* metadata_extension_subsegment_length */
            if ((err = dolbyEFile.BitUnp_rj(&fip->MetadataExt.metadata_extension_subsegment_length[seg], 1, 12))) 
                return(err);                

            if ((err = dolbyEFile.SetDnCntr(1, fip->MetadataExt.metadata_extension_subsegment_length[seg]))) 
                return(err);
            ac3_metadata_extension_subsegment(fip);
            fip->MetadataExt.unused_metadata_extension_subsegment_bits[seg] = dolbyEFile.GetDnCntr(1);                
            if ((err = dolbyEFile.SkipBits(fip->MetadataExt.unused_metadata_extension_subsegment_bits[seg]))) 
                return(err);
        }
        else{ return(-1); }

        seg++;
    }

    /* unused_metadata_extension_bits */
    fip->MetadataExt.unused_metadata_extension_bits = dolbyEFile.GetDnCntr(0);
    if ((err = dolbyEFile.SkipBits(fip->MetadataExt.unused_metadata_extension_bits))) return(err);                        

    /* metadata_extension_crc */
    dolbyEFile.BitUnp_rj(&fip->metadata_extension_crc, 1, fip->wordSz);

    return(0);
}    /* metadata_extension_segment() */

/*****************************************************************************
*    display_metadata_extension_segment: display the metadata extension segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::display_metadata_extension_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag)
{
    int seg = 0;

    if (display_flag) printf("  Metadata Extension Segment\n");
        
    fprintf(xmlfp, "<Metadata_Extension_Segment>\n");

    /* metadata_extension_key */
    if (fip->keyPresent)
    {
        if (display_flag) printf("    Metadata extension key: ");
            
        fprintf(xmlfp, "<Metadata_extension_key>");
        switch (fip->wordSz)
        {
            case 16:
                if (display_flag) printf("0x%04x\n", fip->MetadataExt.metadata_extension_key);
                    
                fprintf(xmlfp, "0x%04x", fip->MetadataExt.metadata_extension_key);
                break;
            case 20:
                if (display_flag) printf("0x%05x\n", fip->MetadataExt.metadata_extension_key);
                    
                fprintf(xmlfp, "0x%05x", fip->MetadataExt.metadata_extension_key);
                break;
            case 24:
                if (display_flag) printf("0x%06x\n", fip->MetadataExt.metadata_extension_key);
                    
                fprintf(xmlfp, "0x%06x", fip->MetadataExt.metadata_extension_key);
                break;
            default:
                return(-1);
        }
        fprintf(xmlfp, "</Metadata_extension_key>\n");
    }

    while (1)
    {
        /* metadata_extension_subsegment_id */        
        if (display_flag) printf("    Metadata extension subsegment: %s (%d)\n",
            metaSubSegText[fip->MetadataExt.metadata_extension_subsegment_id[seg]], 
            fip->MetadataExt.metadata_extension_subsegment_id[seg]);
            
        fprintf(xmlfp, "<Metadata_extension_subsegment>%s (%d)</Metadata_extension_subsegment>\n",
            metaSubSegText[fip->MetadataExt.metadata_extension_subsegment_id[seg]], 
            fip->MetadataExt.metadata_extension_subsegment_id[seg]);

        if (fip->MetadataExt.metadata_extension_subsegment_id[seg] == 0){ break; }
        else if(fip->MetadataExt.metadata_extension_subsegment_id[seg] < 3)
        {
            /* metadata_extension_subsegment_length */
            if (display_flag) printf("    Metadata extension subsegment length: %d bits\n", 
                fip->MetadataExt.metadata_extension_subsegment_length[seg]);
                
            fprintf(xmlfp, "<Metadata_extension_subsegment_length>%d bits</Metadata_extension_subsegment_length>\n", 
                fip->MetadataExt.metadata_extension_subsegment_length[seg]);

            display_ac3_metadata_extension_subsegment(xmlfp, fip, display_flag);

            if (display_flag) printf("    Unused metadata extension subsegment length: %d bits\n", 
                fip->MetadataExt.unused_metadata_extension_subsegment_bits[seg]);
                
            fprintf(xmlfp, "<Unused_metadata_extension_subsegment_length>%d bits</Unused_metadata_extension_subsegment_length>\n", 
                fip->MetadataExt.unused_metadata_extension_subsegment_bits[seg]);
        }
        else{ return(-1); }

        seg++;
    }

    /* unused_metadata_extension_bits */
    if (display_flag) printf("    Unused metadata extension segment length: %d bits\n", 
        fip->MetadataExt.unused_metadata_extension_bits);
        
    fprintf(xmlfp, "<Unused_metadata_extension_segment_length>%d bits</Unused_metadata_extension_segment_length>\n", 
        fip->MetadataExt.unused_metadata_extension_bits);
                

    /* metadata_extension_crc */
    if (display_flag) printf("    Metadata extension CRC: ");
        
    fprintf(xmlfp, "<Metadata_extension_CRC>");
    switch (fip->wordSz)
    {
        case 16:
            if (display_flag) printf("0x%04x\n", fip->metadata_extension_crc);
                
            fprintf(xmlfp, "0x%04x", fip->metadata_extension_crc);
            break;
        case 20:
            if (display_flag) printf("0x%05x\n", fip->metadata_extension_crc);
                
            fprintf(xmlfp, "0x%05x", fip->metadata_extension_crc);
            break;
        case 24:
            if (display_flag) printf("0x%06x\n", fip->metadata_extension_crc);
                
            fprintf(xmlfp, "0x%06x", fip->metadata_extension_crc);
            break;
        default:
            return(-1);
    }
    fprintf(xmlfp, "</Metadata_extension_CRC>\n");
    fprintf(xmlfp, "</Metadata_Extension_Segment>\n");
    return(0);
}    /* display_metadata_extension_segment() */

/*****************************************************************************
*    ac3_metadata_extension_segment: parse the AC-3 metadata ext segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::ac3_metadata_extension_subsegment(FrameInfoStruct *fip)
{
    int    err;
    int pgm;

    for (pgm = 0; pgm < fip->nProgs; pgm++)
    {
        /* ac3_compr2 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3MetadataExt.ac3_compr2[pgm], 1, 8))) return(err);

        /* ac3_dynrng5 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3MetadataExt.ac3_dynrng5[pgm], 1, 8))) return(err);

        /* ac3_dynrng6 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3MetadataExt.ac3_dynrng6[pgm], 1, 8))) return(err);

        /* ac3_dynrng7 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3MetadataExt.ac3_dynrng7[pgm], 1, 8))) return(err);

        /* ac3_dynrng8 */
        if ((err = dolbyEFile.BitUnp_rj(&fip->AC3MetadataExt.ac3_dynrng8[pgm], 1, 8))) return(err);
    }

    return(0);
}    /* ac3_metadata_extension_subsegment() */

/*****************************************************************************
*    display_ac3_metadata_extension_segment: display the AC-3 metadata ext segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::display_ac3_metadata_extension_subsegment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag)
{
    int pgm;

    if (display_flag) printf("      AC-3 Metadata Extension Subsegment\n");

    fprintf(xmlfp, "<AC3_Metadata_Extension_Subsegment>\n");

    for (pgm = 0; pgm < fip->nProgs; pgm++)
    {
        if (display_flag) printf("        Program %d AC-3 extension metadata\n", pgm);
            
        fprintf(xmlfp, "<Program_%d_AC3_extension_metadata>\n", pgm);
        

        /* ac3_compr2 */
        if (display_flag) printf("          AC-3 compr2: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3MetadataExt.ac3_compr2[pgm] * 2), 
            fip->AC3MetadataExt.ac3_compr2[pgm]);
            
        fprintf(xmlfp, "<AC3_compr2>%6.2f dB (%d)</AC3_compr2>\n",
            GetComprDB(fip->AC3MetadataExt.ac3_compr2[pgm] * 2), 
            fip->AC3MetadataExt.ac3_compr2[pgm]);
        

        /* ac3_dynrng5 */
        if (display_flag) printf("          AC-3 dynrng5: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng5[pgm]), fip->AC3MetadataExt.ac3_dynrng5[pgm]);
            
        fprintf(xmlfp, "<AC3_dynrng5>%6.2f dB (%d)</AC3_dynrng5>\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng5[pgm]), fip->AC3MetadataExt.ac3_dynrng5[pgm]);
        
        
        /* ac3_dynrng6 */
        if (display_flag) printf("          AC-3 dynrng6: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng6[pgm]), fip->AC3MetadataExt.ac3_dynrng6[pgm]);
            
        fprintf(xmlfp, "<AC3_dynrng6>%6.2f dB (%d)</AC3_dynrng6>\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng6[pgm]), fip->AC3MetadataExt.ac3_dynrng6[pgm]);
        
        
        /* ac3_dynrng7 */
        if (display_flag) printf("          AC-3 dynrng7: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng7[pgm]), fip->AC3MetadataExt.ac3_dynrng7[pgm]);
            
        fprintf(xmlfp, "<AC3_dynrng7>%6.2f dB (%d)</AC3_dynrng7>\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng7[pgm]), fip->AC3MetadataExt.ac3_dynrng7[pgm]);
        
        
        /* ac3_dynrng8 */
        if (display_flag) printf("          AC-3 dynrng8: %6.2f dB (%d)\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng8[pgm]), fip->AC3MetadataExt.ac3_dynrng8[pgm]);
            
        fprintf(xmlfp, "<AC3_dynrng8>%6.2f dB (%d)</AC3_dynrng8>\n",
            GetComprDB(fip->AC3MetadataExt.ac3_dynrng8[pgm]), fip->AC3MetadataExt.ac3_dynrng8[pgm]);
        
        
        fprintf(xmlfp, "</Program_%d_AC3_extension_metadata>\n", pgm);
    }

    fprintf(xmlfp, "</AC3_Metadata_Extension_Subsegment>\n");
    return(0);
}    /* display_ac3_metadata_extension_subsegment() */

/*****************************************************************************
*    audio_extension_segment: parse the audio extension segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::audio_extension_segment(FrameInfoStruct *fip)
{
    ChannelSubsegInfoStruct chanInfo, *cip = &chanInfo;
    int    value, err;
    int ch, keycount;

    cip->lowFrameRate = fip->lowFrameRate;
    cip->priExtFlag = 1;

    /* audio_extension_subsegment0_key */
    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz))) return(err);
        fip->metadata_key = value;
        keycount = 1;
        for (ch = 0; ch < fip->nChans/2; ch++)
        {
            keycount += fip->chanSubsegSz[ch];
        }
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, keycount))) return(err);
    }

    for (ch = 0; ch < fip->nChans/2; ch++)
    {
        cip->lfeFlag = (ch == fip->lfeChan);

        /* channel_extension_subsegment[ch] */        
        if ((err = dolbyEFile.SetDnCntr(0, fip->chanSubsegSz[ch] * fip->wordSz))) return(err);
        cip->prevGroupTypeCode = fip->prevGroupTypeCode[ch];
        if ((err = channel_subsegment(cip))) return(err);
        value = dolbyEFile.GetDnCntr(0);        
        if ((err = dolbyEFile.SkipBits(value))) return(err);                        
    }

    /* audio_extension_subsegment0_crc */
    dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz);

    /* audio_extension_subsegment1_key */
    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz))) return(err);
        fip->metadata_key = value;
        keycount = 1;
        for (ch = fip->nChans/2; ch < fip->nChans; ch++)
        {
            keycount += fip->chanSubsegSz[ch];
        }
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, keycount))) return(err);
    }

    for (ch = fip->nChans/2; ch < fip->nChans; ch++)
    {
        cip->lfeFlag = (ch == fip->lfeChan);

        /* channel_extension_subsegment[ch] */        
        if ((err = dolbyEFile.SetDnCntr(0, fip->chanSubsegSz[ch] * fip->wordSz))) return(err);
        cip->prevGroupTypeCode = fip->prevGroupTypeCode[ch];
        if ((err = channel_subsegment(cip))) return(err);
        value = dolbyEFile.GetDnCntr(0);
        if ((err = dolbyEFile.SkipBits(value))) return(err);                        
    }

    /* audio_extension_subsegment1_crc */
    dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz);

    return(0);
}    /* audio_extension_segment() */

/*****************************************************************************
*    meter_segment: parse the meter segment
*
*    inputs:
*        fip                    pointer to frame info structure
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::meter_segment(FrameInfoStruct *fip)
{
    int    value, err;
    int ch;

    /* meter_key */
    if (fip->keyPresent)
    {
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz))) return(err);
        fip->metadata_key = value;
        if ((err = dolbyEFile.BitUnkey(fip->metadata_key, fip->meterSz + 1))) return(err);
    }

    if ((err = dolbyEFile.SetDnCntr(0, fip->meterSz * fip->wordSz))) return(err);

    for (ch = 0; ch < fip->nChans; ch++)
    {
        /* peak_meter[ch] */
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, 10))) return(err);
        fip->Meter.peak_meter[ch] = value;
    }
    for (ch = 0; ch < fip->nChans; ch++)
    {
        /* rms_meter[ch] */
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, 10))) return(err);
        fip->Meter.rms_meter[ch] = value;
    }

    /* unused_meter_bits */
    value = dolbyEFile.GetDnCntr(0);
    if ((err = dolbyEFile.SkipBits(value))) return(err);                        

    /* meter_crc */
    dolbyEFile.BitUnp_rj(&value, 1, fip->wordSz);

    return(0);
}  /* meter_segment() */

int DolbyEParser::display_meter_segment(FILE *xmlfp, FrameInfoStruct *fip, int display_flag)
{
    int ch;
    double peak_meter[MAX_NCHANS], rms_meter[MAX_NCHANS];

    if (display_flag) printf("  Meter Segment\n");
        
    fprintf(xmlfp, "<Meter_Segment>\n");
    

        if (display_flag) printf("    Peak Meter\n");
        
        fprintf(xmlfp, "<Peak_Meter>\n");
        for (ch = 0; ch < fip->nChans; ch++)
        {
            /* convert coded meter values to logarithmic peak dB values and display*/
            peak_meter[ch] = -1 * ((0x3c0 - fip->Meter.peak_meter[ch]) * 0.094);

            if (fip->Meter.peak_meter[ch] == 0) {
                if (display_flag) printf("      Channel %i (%s): -inf dB (%i)\n", ch, chanIDText[fip->progConfig][ch],
                    fip->Meter.peak_meter[ch]);
                
                fprintf(xmlfp, "<Channel_%i_%s>-inf dB (%i)</Channel_%i_%s>\n", ch, chanIDText[fip->progConfig][ch],
                    fip->Meter.peak_meter[ch], ch, chanIDText[fip->progConfig][ch]);
            }
            else if (fip->Meter.peak_meter[ch] == 0x3ff) {
                if (display_flag) printf("      Channel %i (%s): clipping: unspecified (%i)\n", ch, 
                    chanIDText[fip->progConfig][ch], fip->Meter.peak_meter[ch]); 
                
                fprintf(xmlfp, "<Channel_%i_%s>clipping: unspecified (%i)</Channel_%i_%s>\n", ch, 
                    chanIDText[fip->progConfig][ch], fip->Meter.peak_meter[ch], ch, chanIDText[fip->progConfig][ch]); 
            }
            else if (fip->Meter.peak_meter[ch] > 0x3c0) {
                if (display_flag) printf("      Channel %i (%s): clipping: +%2.2f dB (%i)\n", ch, 
                    chanIDText[fip->progConfig][ch], peak_meter[ch], fip->Meter.peak_meter[ch]); 
                
                fprintf(xmlfp, "<Channel_%i_%s>clipping: +%2.2f dB (%i)<Channel_%i_%s>\n", ch, 
                    chanIDText[fip->progConfig][ch], peak_meter[ch], fip->Meter.peak_meter[ch], ch, chanIDText[fip->progConfig][ch]); 
            }
            else {
                if (display_flag) printf("      Channel %i (%s): %2.2f dB (%i)\n", ch,  
                    chanIDText[fip->progConfig][ch], peak_meter[ch], fip->Meter.peak_meter[ch]);
                
                fprintf(xmlfp, "<Channel_%i_%s>%2.2f dB (%i)</Channel_%i_%s>\n", ch,  
                    chanIDText[fip->progConfig][ch], peak_meter[ch], fip->Meter.peak_meter[ch], ch, chanIDText[fip->progConfig][ch]);
            }
        }
        
        fprintf(xmlfp, "</Peak_Meter>\n");
        
        if (display_flag) printf("    RMS Meter\n");
        fprintf(xmlfp, "<RMS_Meter>\n");

        for (ch = 0; ch < fip->nChans; ch++)
        {
            /* convert coded meter values to logarithmic rms dB values and display*/
            rms_meter[ch] = -1 * ((0x3c0 - fip->Meter.rms_meter[ch]) * 0.094);

            if (fip->Meter.rms_meter[ch] == 0) {
                if (display_flag) printf("      Channel %i (%s): -inf dB (%i)\n", ch, 
                    chanIDText[fip->progConfig][ch], fip->Meter.rms_meter[ch]);
                
                fprintf(xmlfp, "<Channel_%i_%s>-inf dB (%i)</Channel_%i_%s>\n", ch, 
                    chanIDText[fip->progConfig][ch], fip->Meter.rms_meter[ch], ch, chanIDText[fip->progConfig][ch]); 
            }
            else if (fip->Meter.rms_meter[ch] == 0x3ff) {
                if (display_flag) printf("      Channel %i (%s): clipping: unspecified (%i)\n", ch, 
                    chanIDText[fip->progConfig][ch], fip->Meter.rms_meter[ch]); 
                
                fprintf(xmlfp, "<Channel_%i_%s>clipping: unspecified (%i)</Channel_%i_%s>\n", ch, 
                    chanIDText[fip->progConfig][ch], fip->Meter.rms_meter[ch], ch, chanIDText[fip->progConfig][ch]); 
            }
            else if (fip->Meter.rms_meter[ch] > 0x3c0) {
                if (display_flag) printf("      Channel %i (%s): clipping: +%2.2f dB (%i)\n", ch, 
                    chanIDText[fip->progConfig][ch], rms_meter[ch], fip->Meter.rms_meter[ch]);
                
                fprintf(xmlfp, "<Channel_%i_%s>clipping: +%2.2f dB (%i)</Channel_%i_%s>\n", ch, 
                    chanIDText[fip->progConfig][ch], rms_meter[ch], fip->Meter.rms_meter[ch], ch, chanIDText[fip->progConfig][ch]);
            }
            else {
                if (display_flag) printf("      Channel %i (%s): %2.2f dB (%i)\n", ch, 
                    chanIDText[fip->progConfig][ch], rms_meter[ch], fip->Meter.rms_meter[ch]);
                
                fprintf(xmlfp, "<Channel_%i_%s>%2.2f dB (%i)</Channel_%i_%s>\n", ch, 
                    chanIDText[fip->progConfig][ch], rms_meter[ch], fip->Meter.rms_meter[ch], ch, chanIDText[fip->progConfig][ch]);
            }
        }
        fprintf(xmlfp, "</RMS_Meter>\n");
    

    fprintf(xmlfp, "</Meter_Segment>\n");
    return(0);
}    /* display_meter_segment() */

/*****************************************************************************
*    channel_subsegment: parse the channel subsegment
*
*    inputs:
*        fip                    pointer to frame info structure
*        lfeFlag                0 if not LFE, nonzero if LFE
*
*    outputs:
*        return value        0 if no error, nonzero if error
*****************************************************************************/

int DolbyEParser::channel_subsegment(ChannelSubsegInfoStruct *cip)
{
    int expStrat[MAX_NBLKS];
    int value, err;
    int blk, reg, bnd;

/*    group structure fields */

    if (cip->lfeFlag)    {
        
        cip->groupTypeCode = -1;
        cip->bandwidthCode = -1;
    }
    else
    {
        /* group_type_code */
        if (cip->lowFrameRate)
        {
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 2))) return(err);
        }
        else
        {
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 1))) return(err);
        }
        cip->groupTypeCode = value;

        /* bandwidth */
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, 3))) return(err);
        cip->bandwidthCode = value;
    }

    if (cip->priExtFlag == 1)
    {
        if (((cip->prevGroupTypeCode == SHORT) && (cip->groupTypeCode != SHORT))
            || ((cip->prevGroupTypeCode != SHORT) && (cip->groupTypeCode == SHORT)))
        {
            printf("Error: Illegal group type in extension subsegment\n");
            return(-1);
        }
    }

/*    Initialize channel subseg info structure */

    if ((err = init_channel_subseg_info(cip))) return(err);

/*    exponent fields */

    for (blk = 0; blk < cip->blockCount; blk++)
    {
        /* exponent_strategy[blk] */
        if (blk == 0)
        {
            value = 1;            
        }
        else if (cip->bandCount[blk] != cip->bandCount[blk - 1])
        {
            value = 1;            
        }
        else
        {
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 1))) return(err);            
        }
        expStrat[blk] = value;

        if (value)
        {
            /* master_exponent[blk][reg] */            
            for (reg = 0; reg < cip->regionCount[blk]; reg++)
            {
                if ((err = dolbyEFile.BitUnp_rj(&value, 1, 2))) return(err);                
            }

            /* biased_exponent[blk][bnd] */            
            for (bnd = 0; bnd < cip->bandCount[blk]; bnd++)
            {
                if ((err = dolbyEFile.BitUnp_rj(&value, 1, 5))) return(err);                
            }
        }
    }

/*    masking model parameters */

    for (blk = 0; blk < cip->blockCount; blk++)
    {
        /* mask_model_info_exists[blk] */        
        if (blk == 0)
        {
            value = 1;            
        }
        else
        {
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 1))) return(err);            
        }

        if (value)
        {
            /* fast_gain_spectrum[blk] */
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 2))) return(err);            

            /* fast_gain_offset[blk] */
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 3))) return(err);            

            /* mask_model[blk] */
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 1))) return(err);            
        }
    }

/*    bit allocation fields */


    /* delta_bit_info_exists[blk] */
    if ((err = dolbyEFile.BitUnp_rj(&value, 1, 1))) return(err);

    /* snr_offset */
    if ((err = dolbyEFile.BitUnp_rj(&value, 1, 8))) return(err);

/*    gain adaptive quantization fields */

    for (blk = 0; blk < cip->blockCount; blk++)
    {
        /* gaq_info_exists[blk] */
        if ((err = dolbyEFile.BitUnp_rj(&value, 1, 1))) return(err);

        if (value == 1)
        {
            /* first_gaband[blk] */
            if ((err = dolbyEFile.BitUnp_rj(&value, 1, 6))) return(err);

            if (value == 63)
            {                
            }
            else
            {
                if (value >= cip->bandCount[blk])
                {
                    value = cip->bandCount[blk];
                }

                for (bnd = value; bnd < cip->bandCount[blk]; bnd++)
                {
                    /* adaptive_gain[blk][bnd] */
                    if ((err = dolbyEFile.BitUnp_rj(&value, 1, 2))) return(err);
                }
            }
        }
    }

    return(0);
}    /* channel_subsegment() */



/********************************************************************
 * Helpers *
********************************************************************/


static int init_channel_subseg_info(ChannelSubsegInfoStruct *cip)
{
    int blk;

/*    Set up control variables */

    if (cip->lowFrameRate == 0)    /* high frame rate */
    {
        if (cip->lfeFlag)    /* LFE */
        {
            cip->blockCount = 1;
            cip->regionCount[0] = 1;
            cip->bandCount[0] = 21;
        }
        else    /* not LFE */
        {
            switch (cip->groupTypeCode)
            {
                case LONG:
                    cip->blockCount = 1;
                    cip->regionCount[0] = 2;
                    cip->bandCount[0] = 50;
                    break;
                case SHORT:
                    cip->blockCount = 9;
                    for (blk = 0; blk < 9; blk++)
                    {
                        cip->regionCount[blk] = 2;
                        cip->bandCount[blk] = 38;
                    }
                    break;
                default:
                    return(-1);
            }
        }
    }
    else    /* low frame rate */
    {
        if (cip->priExtFlag == 0)    /* primary subsegment */
        {
            if (cip->lfeFlag)    /* LFE */
            {
                cip->blockCount = 1;
                cip->regionCount[0] = 1;
                cip->bandCount[0] = 21;
            }
            else    /* not LFE */
            {
                switch (cip->groupTypeCode)
                {
                    case LONG:
                        cip->blockCount = 1;
                        cip->regionCount[0] = 2;
                        cip->bandCount[0] = 50;
                        break;
                    case SHORT:
                        cip->blockCount = 8;
                        for (blk = 0; blk < 8; blk++)
                        {
                            cip->regionCount[blk] = 2;
                            cip->bandCount[blk] = 38;
                        }
                        break;
                    case BRIDGE:
                        cip->blockCount = 7;
                        for (blk = 0; blk < 6; blk++)
                        {
                            cip->regionCount[blk] = 2;
                            cip->bandCount[blk] = 38;
                        }
                        cip->regionCount[6] = 2;
                        cip->bandCount[6] = 44;
                        break;
                    default:
                        return(-1);
                }
            }
        }
        else    /* extension subsegment */
        {
            if (cip->lfeFlag)    /* LFE */
            {
                cip->blockCount = 1;
                cip->regionCount[0] = 1;
                cip->bandCount[0] = 21;
            }
            else    /* not LFE */
            {
                switch (cip->groupTypeCode)
                {
                    case LONG:
                        cip->blockCount = 1;
                        cip->regionCount[0] = 2;
                        cip->bandCount[0] = 50;
                        break;
                    case SHORT:
                        cip->blockCount = 8;
                        for (blk = 0; blk < 8; blk++)
                        {
                            cip->regionCount[blk] = 2;
                            cip->bandCount[blk] = 38;
                        }
                        break;
                    case BRIDGE:
                        cip->blockCount = 7;
                        cip->regionCount[0] = 2;
                        cip->bandCount[0] = 44;
                        for (blk = 1; blk < 7; blk++)
                        {
                            cip->regionCount[blk] = 2;
                            cip->bandCount[blk] = 38;
                        }
                        break;
                    default:
                        return(-1);
                }
            }
        }
    }

/*    Adjust for bandwidth code */

    if (cip->lfeFlag == 0)
    {
        for (blk = 0; blk < cip->blockCount; blk++)
        {
            cip->bandCount[blk] -= cip->bandwidthCode;
        }
    }

    return(0);
}

static double GetComprDB(int value)
{
    int mant, exp;
    double gainval;

    mant = value & 0x1f;
    gainval = ((double)mant + 32) / 64;
    exp = (value & 0x01e0) >> 5;
    if (exp >= 8) exp -= 16;
    exp += 1;
    if (exp > 0)
    {
        while (exp--) gainval *= 2.0;
    }
    else if (exp < 0)
    {
        while (exp++) gainval *= 0.5;
    }
    return(20*log10(gainval));
}

static int check_time_code(int *current_tc, int *last_tc, int frame_rate)
{
    short next_tc[4];

    if( (current_tc[1] & 0x3f) == 0x3f)    /* if invalid timecode, exit now */
        return 0;

    /*    convert timecode of previous frame to decimal */

    next_tc[0] = BCD2DEC( (last_tc[1] & 0x3f) );
    next_tc[1] = BCD2DEC( (last_tc[3] & 0x7f) );
    next_tc[2] = BCD2DEC( (last_tc[5] & 0x7f) );
    next_tc[3] = BCD2DEC( (last_tc[7] & 0x3f) );

    /*    calculate what next frame timecode should be, based upon previous frame*/

    next_tc[3]++;                /* increment frames */
    next_tc[2] += (next_tc[3] / DolbyEParser::last_frame_tab[frame_rate - 1]);  
                                      /* increment seconds on frames roll over */
    next_tc[1] += (next_tc[2] / 60);  /* increment minutes on seconds roll over */
    next_tc[0] += (next_tc[1] / 60);  /* increment hours on minutes roll over */
    
    /*    check for drop frame condition */
    if(    (DolbyEParser::drop_frame_tab[frame_rate - 1]) &&                /* 23.98fps & 29.97fps use drop-frame */
        (next_tc[2] == 60) &&    /* perform drop on minute boundaries */
        (next_tc[1] % 10))        /* do not perform on 10-minute boundaries */
    {
        next_tc[3] += 2;        /* perform drop frame */
    }
    
    next_tc[0] %= 24;            /* hours roll over at 24 */
    next_tc[1] %= 60;            /* minutes roll over at 60 */
    next_tc[2] %= 60;            /* seconds roll over at 60 */
    next_tc[3] %= DolbyEParser::last_frame_tab[frame_rate - 1]; /* frames roll at frame rate */

    /*    verify timecode of current frame matches what next value from previous
        frame says it should be */
    if( ((current_tc[7] & 0x3f) != DEC2BCD(next_tc[3]))
        || ((current_tc[5] & 0x7f) != DEC2BCD(next_tc[2]))
        || ((current_tc[3] & 0x7f) != DEC2BCD(next_tc[1]))
        || ((current_tc[1] & 0x3f) != DEC2BCD(next_tc[0])) )
    {
        return 1;
    }

    return 0;
}
