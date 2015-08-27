/* --------------------------------------------------------------------------
  FILE        : ubl.c                                                   
  PROJECT     : TI Booting and Flashing Utilities
  AUTHOR      : Daniel Allred
  DESC        : The main project file for the user boot loader
 ----------------------------------------------------------------------------- */
#include <string.h>

// General type include
#include "tistdtypes.h"

// This module's header file 
#include "ubl.h"

// Device specific CSL
#include "device.h"

// Misc. utility function include
#include "util.h"

// Project specific debug functionality
#include "debug.h"
#include "uartboot.h"


#ifdef UBL_NOR
// NOR driver include
#include "nor.h"
#include "norboot.h"
#endif

#ifdef UBL_NAND
// NAND driver include
#include "nand.h"
#include "nandboot.h"
#endif

#ifdef UBL_SD_MMC
// NAND driver include
#include "sdmmc.h"
#include "sdmmcboot.h"
#endif

/************************************************************
* Explicit External Declarations                            *
************************************************************/

extern int usb_detect (void);
extern Uint32 NANDBOOT_copy (Uint32 mode);

/************************************************************
* Local Macro Declarations                                  *
************************************************************/


/************************************************************
* Local Typedef Declarations                                *
************************************************************/


/************************************************************
* Local Function Declarations                               *
************************************************************/

static Uint32 LOCAL_boot(void);
static void LOCAL_bootAbort(void);
static void (*APPEntry)(unsigned, unsigned, unsigned);


/************************************************************
* Local Variable Definitions                                *
************************************************************/

/************************************************************
* Global Variable Definitions                               *
************************************************************/

Uint32 gEntryPoint;

/************************************************************
* Global Function Definitions                               *
************************************************************/

#define MACH_TYPE_DAVINCI_DM365_EVM    1939
#define ADDR_ATAGS 0x80000100

// Main entry point
void main (void)
{
    unsigned *tag = (void*) ADDR_ATAGS;

    // Call to real boot function code
    LOCAL_boot();
    
    // Jump to entry point
    DEBUG_printString("Jumping to entry point at ");
    DEBUG_printHexInt(gEntryPoint);
    DEBUG_printString(".\r\n");

    /* CORE */
	*tag++ = 2;
	*tag++ = 0x54410001;

	/* END */
	*tag++ = 0;
	*tag++ = 0;

    APPEntry = (void (*)(unsigned, unsigned, unsigned)) gEntryPoint;

    (*APPEntry)(0, MACH_TYPE_DAVINCI_DM365_EVM, ADDR_ATAGS);
    for(;;);

}

/************************************************************
* Local Function Definitions                                *
************************************************************/

static Uint32 LOCAL_boot(void)
{

Uint32 rescue;

#ifndef UBL_NAND
  DEVICE_BootMode bootMode;

  // Read boot mode 
  bootMode = DEVICE_bootMode();

  if (bootMode == DEVICE_BOOTMODE_UART)
  {
    // Wait until the RBL is done using the UART.
    while((UART0->LSR & 0x40) == 0 );
  }
#endif
  // Platform Initialization
  if ( DEVICE_init() != E_PASS )
  {
    DEBUG_printString(devString);
    DEBUG_printString(" initialization failed!\r\n");
    asm(" MOV PC, #0");
  }
#if 0
  else
  {
    DEBUG_printString(devString);
    DEBUG_printString(" initialization passed!\r\n");
  }
#endif

  // Set RAM pointer to beginning of RAM space
  UTIL_setCurrMemPtr(0);

  // Send some information to host
  DEBUG_printString("\r\nUBL Version: ");
  DEBUG_printString(UBL_VERSION_STRING);
  DEBUG_printString("\r\n");

  rescue = !!usb_detect();
  if (rescue)
      DEBUG_printString("USB cable is attached, loading rescue image ...\r\n");

  DEBUG_printString("\r\nBootMode = ");
  
  // Select Boot Mode
#if defined(UBL_NAND)
  {
    //Report Bootmode to host
    DEBUG_printString("NAND\r\n");

    // Copy binary image application from NAND to RAM
    if (NANDBOOT_copy(rescue) != E_PASS)
    {
      DEBUG_printString("NAND Boot failed.\r\n");
      LOCAL_bootAbort();
    }
  }
#elif defined(UBL_NOR)
  {
    //Report Bootmode to host
    DEBUG_printString("NOR \r\n");

    // Copy binary application image from NOR to RAM
    if (NORBOOT_copy() != E_PASS)
    {
      DEBUG_printString("NOR Boot failed.\r\n");
      LOCAL_bootAbort();
    }
  }
#elif defined(UBL_PCI)
  {
    //Report Bootmode to host
    DEBUG_printString("PCI \n\r");

    // Copy binary application image from PCI to RAM
    if (PCIBOOT_copy() != E_PASS)
    {
      DEBUG_printString("PCI Boot failed.\r\n");
      LOCAL_bootAbort();
    }
  }
#elif defined(UBL_SD_MMC)
  {
    //Report Bootmode to host
    DEBUG_printString("SD/MMC \r\n");

    // Copy binary of application image from SD/MMC card to RAM
    if (SDMMCBOOT_copy() != E_PASS)
    {
      DEBUG_printString("SD/MMC Boot failed.\r\n");
      LOCAL_bootAbort();
    }
  }
#else
  {
    //Report Bootmode to host
    DEBUG_printString("UART \r\n");
    UARTBOOT_copy();
  }

#endif
    
  //DEBUG_printString("   DONE");

  //GPIO->SETDATA01 = 0x00000100; // set PROG_B high
  //GPIO->CLRDATA01 = 0x00000800; // set FPGA reset low
  
  UTIL_waitLoop(10000);

  //GPIO->SETDATA45 = 0x08000000; // set led RxD to 1 (off the led)
  //GPIO->CLRDATA45 = 0x08000000; // set led RxD to 0 (on the led)

  // set led RxD to 1 (off the led)
  GPIO->SETDATA45 = 0x00850000;
  GPIO->CLRDATA45 = 0x1f788000;


  DEVICE_TIMER0Stop();

  return E_PASS;
}

static void LOCAL_bootAbort(void)
{
  DEBUG_printString("Aborting...\r\n");
  while (TRUE);
}

/************************************************************
* End file                                                  *
************************************************************/
