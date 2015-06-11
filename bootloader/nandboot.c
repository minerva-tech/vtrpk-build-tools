/* --------------------------------------------------------------------------
  FILE        : nandboot.c                                                   
  PROJECT     : TI Booting and Flashing Utilities
  AUTHOR      : Daniel Allred
  DESC        : Module to boot the from a NAND flash device by finding the
                application (usually U-boot) and loading it to RAM.
----------------------------------------------------------------------------- */

#ifdef UBL_NAND

// General type include
#include "tistdtypes.h"

// Debug I/O module
#include "debug.h"

// Device specific functions
#include "device.h"

// Misc utility module
#include "util.h"

// Main UBL module
#include "ubl.h"

// NAND driver functions
#include "nand.h"

// Device specific NAND info
#include "device_nand.h"

// This module's header file
#include "nandboot.h"

/************************************************************
* Explicit External Declarations                            *
************************************************************/

extern __FAR__ Uint32 DDRStart;
extern __FAR__ Uint32 DDRSize;
extern __FAR__ Uint32 EMIFStart;

// Entrypoint for application we are decoding out of flash
extern Uint32 gEntryPoint;

/************************************************************
* Local Macro Declarations                                  *
************************************************************/


/************************************************************
* Local Typedef Declarations                                *
************************************************************/


/************************************************************
* Local Function Declarations                               *
************************************************************/


/************************************************************
* Local Variable Definitions                                *
************************************************************/


/************************************************************
* Global Variable Definitions                               *
************************************************************/

// structure for holding details about UBL stored in NAND
volatile NANDBOOT_HeaderObj  gNandBoot;

/************************************************************
* Global Function Definitions                               *
************************************************************/

// Function to find out where the application is and copy to RAM
Uint32 NANDBOOT_copy (Uint32 mode)
{
  NAND_InfoHandle hNandInfo;
  Uint32 count;
  Uint32 i;
  Uint32 magicNum;
  Uint8 *rxBuf;    // RAM receive buffer
  Uint32 block,page;
  Uint32 readError = E_FAIL;

  // Maximum application size is 16 MB
  rxBuf = (Uint8*)UTIL_allocMem((APP_IMAGE_SIZE>>1));

  //DEBUG_printString("Starting NAND Copy...\r\n");
  
  // NAND Initialization
  hNandInfo = NAND_open((Uint32)&EMIFStart, (Uint8) DEVICE_emifBusWidth());
  if (hNandInfo == NULL)
    return E_FAIL;

    DEBUG_printString("NAND: bytes per page ");
    DEBUG_printHexInt(hNandInfo->dataBytesPerPage);
    DEBUG_printString(", pages per block ");
    DEBUG_printHexInt(hNandInfo->pagesPerBlock);
    DEBUG_printString("\r\n");

    if (mode) goto load_rescue_image;

  for(count=DEVICE_NAND_KERNEL_SEARCH_START_BLOCK; count < DEVICE_NAND_KERNEL_SEARCH_END_BLOCK; count++)
  {
    if(NAND_readPage(hNandInfo,count,0,rxBuf) != E_PASS)
      continue;

    magicNum = ((Uint32 *)rxBuf)[0];

    /* Valid magic number found */
    if((magicNum & 0xFFFFFF00) == MAGIC_NUMBER_VALID)
    {
      DEBUG_printString("Valid magicnum, ");
      DEBUG_printHexInt(magicNum);
      DEBUG_printString(", found in block ");
      DEBUG_printHexInt(count);
      DEBUG_printString(".\r\n");
      goto image_found;
    }
  }

    // Never found valid header in any page 0 of any of searched blocks
    if (count >= DEVICE_NAND_KERNEL_SEARCH_END_BLOCK)
        DEBUG_printString("No valid boot image found!\r\n");

load_rescue_image:

  for(count=DEVICE_NAND_RESCUE_SEARCH_START_BLOCK; count < DEVICE_NAND_RESCUE_SEARCH_END_BLOCK; count++)
  {
    if(NAND_readPage(hNandInfo,count,0,rxBuf) != E_PASS)
      continue;

    magicNum = ((Uint32 *)rxBuf)[0];

    /* Valid magic number found */
    if((magicNum & 0xFFFFFF00) == MAGIC_NUMBER_VALID)
    {
      DEBUG_printString("Valid magicnum, ");
      DEBUG_printHexInt(magicNum);
      DEBUG_printString(", found in block ");
      DEBUG_printHexInt(count);
      DEBUG_printString(".\r\n");
      break;
    }
  }

  // Never found valid header in any page 0 of any of searched blocks
  if (count >= DEVICE_NAND_RESCUE_SEARCH_END_BLOCK)
  {
    DEBUG_printString("No valid rescue boot image found!\r\n");
    return E_FAIL;
  }

image_found:

  // Fill in NandBoot header
  gNandBoot.entryPoint = *(((Uint32 *)(&rxBuf[4])));/* The first "long" is entry point for Application */
  gNandBoot.numPage = *(((Uint32 *)(&rxBuf[8])));   /* The second "long" is the number of pages */
  gNandBoot.block = *(((Uint32 *)(&rxBuf[12])));   /* The third "long" is the block where Application is stored in NAND */
  gNandBoot.page = *(((Uint32 *)(&rxBuf[16])));   /* The fourth "long" is the page number where Application is stored in NAND */
  gNandBoot.ldAddress = *(((Uint32 *)(&rxBuf[20])));   /* The fifth "long" is the Application load address */

  // If the application is already in binary format, then our 
  // received buffer can point to the specified load address
  // instead of the temp location used for storing an S-record
  // Checking for the UBL_MAGIC_DMA guarantees correct usage with the 
  // Spectrum Digital CCS flashing tool, flashwriter_nand.out

  if ((magicNum == UBL_MAGIC_BIN_IMG) || (magicNum == UBL_MAGIC_DMA))
  {
      // Set the copy location to final run location
    rxBuf = (Uint8 *)gNandBoot.ldAddress;
    // Free temp memory rxBuf used to point to
    UTIL_setCurrMemPtr((void *)((Uint32)UTIL_getCurrMemPtr() - (APP_IMAGE_SIZE>>1)));
  }

  /* initialize block and page number to be used for read */
  block = gNandBoot.block;
  page = gNandBoot.page;

    while (NAND_badBlockCheck(hNandInfo, block) != E_PASS) {
        if (++block >= hNandInfo->numBlocks) return E_FAIL;
    }

    // Perform the actual copying of the application from NAND to RAM
    for (i=0;i<gNandBoot.numPage;i++) {
        // if page goes beyond max number of pages increment block number and reset page number
        if(page >= hNandInfo->pagesPerBlock) {
            page = 0;
            do {
                if (++block >= hNandInfo->numBlocks) return E_FAIL;
            } while (NAND_badBlockCheck(hNandInfo, block) != E_PASS);
        }

        readError = NAND_readPage(hNandInfo,block,page,(&rxBuf[i*(hNandInfo->dataBytesPerPage)]));  /* Copy the data */
        if (readError != E_PASS) {
            // We attempt to read the app data twice.
            readError = NAND_readPage(hNandInfo,block,page,(&rxBuf[i*(hNandInfo->dataBytesPerPage)]));
            if (readError != E_PASS) return E_FAIL;
        }
        page++;
    }

  // Application was read correctly, so set entrypoint
  gEntryPoint = gNandBoot.entryPoint;

  return E_PASS;
}

/************************************************************
* Local Function Definitions                                *
************************************************************/


/***********************************************************
* End file                                                 *
***********************************************************/
#endif  // #ifdef UBL_NAND
