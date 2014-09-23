/*
 * Copyright (C) 2006 RidgeRun
 *
 *  This source code has a dual license.  If this file is linked with other
 *  source code that has a GPL license, then this file is licensed with a GPL
 *  license as described below.  Otherwise the source code contained in this
 *  file is property of RidgeRun. This source code is protected under
 *  copyright law.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef EDMA_H
#define EDMA_H

#include "tistdtypes.h"

/* EDMA Registers */
#define DM365_EDMACC_BASE 0x01c00000
#define DM365_EDMATC0_BASE 0x01c10000
#define DM365_EDMATC1_BASE 0x01c10400
#define _EDMACC(x) (DM365_EDMACC_BASE+(x))
#define EDMACC_CCCFG	_EDMACC(0x04)
#define EDMACC_DRAE0	_EDMACC(0x340)
#define EDMACC_DRAEH0	_EDMACC(0x344)
#define EDMACC_PARAM_BASE _EDMACC(0x4000)
// Use the versions for shadow region 0 (ARM)
#define EDMACC_ESR _EDMACC(0x2010)
#define EDMACC_ESRH _EDMACC(0x2014)
#define EDMACC_IER  _EDMACC(0x2050)
#define EDMACC_IERH  _EDMACC(0x2054)
#define EDMACC_IECR  _EDMACC(0x2058)
#define EDMACC_IECRH  _EDMACC(0x205C)
#define EDMACC_IESR  _EDMACC(0x2060)
#define EDMACC_IESRH  _EDMACC(0x2064)
#define EDMACC_IPR  _EDMACC(0x2068)
#define EDMACC_IPRH  _EDMACC(0x206C)
#define EDMACC_ICR  _EDMACC(0x2070)
#define EDMACC_ICRH  _EDMACC(0x2074)

/* Power and Sleep Controller (PSC) Module Registers */
#define DM365_PSC_BASE 0x01C41000
#define _PSC(x) (DM365_PSC_BASE+(x))
#define DM365_PSC_PTSTAT _PSC(0x128)
#define DM365_PSC_PTCMD _PSC(0x120)
#define DM365_PSC_MDCTL(x) _PSC(0xA00 + ((x)*4))

#define PTCMD_GO 0x1

#define MDCTL_NEXT_SwRstDisable (0x0)
#define MDCTL_NEXT_SyncReset (0x1)
#define MDCTL_NEXT_Disable (0x2)
#define MDCTL_NEXT_Enable (0x3)

#define PSC_EDMACC	2
#define PSC_EDMATC0	3
#define PSC_EDMATC1	4

#define outl(v,a)   (*(volatile unsigned int *)(a) = (v))
#define inl(a)      (*(volatile unsigned int *)(a))

#define ENABLE_PSC_MODULE(x) \
    { \
	while(inl(DM365_PSC_PTSTAT)); /* Wait previous transitions to finalize */ \
	int t = inl(DM365_PSC_MDCTL(x)); \
	t &= ~0x7; \
	outl(t | MDCTL_NEXT_Enable,DM365_PSC_MDCTL(x)); \
	outl(PTCMD_GO,DM365_PSC_PTCMD); /* GO */ \
	while(inl(DM365_PSC_PTSTAT)); /* Wait previous transitions to finalize */ \
    }

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;
typedef int            s32;
typedef short          s16;
typedef char           s8;

struct edma_param {
    u32 opt;
    u32 src;
    u16 acnt;
    u16 bcnt;
    u32 dst;
    s16 srcbidx;
    s16 dstbidx;
    u16 link;
    u16 bcntrld;
    s16 srccidx;
    s16 dstcidx;
    u16 ccnt;
    u16 rsv;
};

void dma_init(void);
struct edma_param *get_dma_channel(int channel);
u32 do_dma(int channel);
u32 nand_dma_read_buf(u8 * src, u8 * dst, u32 len);

#endif
