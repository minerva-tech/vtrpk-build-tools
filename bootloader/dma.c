/*
 * Copyright (C) 2007 RidgeRun
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

#include "dma.h"

/* DMA CHANNEL for NAND could be 48, the timer interrupt of TIMER0, since
 we are not using that event */
#define DMA_CHANNEL 48
struct edma_param *dma_param;

struct edma_param *get_dma_channel(int channel){
    u32 *iesr, *drae;
    int shift;
    
    if (channel > 31){
	iesr = (u32 *)EDMACC_IESRH;
	drae = (u32 *)EDMACC_DRAEH0;
	shift = channel - 32;
    } else {
	iesr = (u32 *)EDMACC_IESR;
	drae = (u32 *)EDMACC_DRAE0;
	shift = channel;
    }
    
    /* Enable the shadow region for ARM or irqs will not work */
    outl(inl(drae) | 1<<shift,drae);

    /* Enable interrupt */
    *iesr |= 1<<shift;
    
    return &(((struct edma_param*) EDMACC_PARAM_BASE)[channel]);
}

void dma_init(void){
    /* Let's try to keep it simple... this is rrload, no a full RTOS */

    /* Enable PSC */
    ENABLE_PSC_MODULE(PSC_EDMACC);
    ENABLE_PSC_MODULE(PSC_EDMATC0);
    ENABLE_PSC_MODULE(PSC_EDMATC1);

    /* We left every channel on his default queue: 0 */
    /* We left every channel on his default TC: 0 */
    /* We left the default priorities */

    dma_param = get_dma_channel(DMA_CHANNEL);
}

u32 do_dma(int channel){
    struct edma_param *param = &(((struct edma_param*) EDMACC_PARAM_BASE)[channel]);
    u32 *esr, *ipr, *icr;
    int shift;
    
    if (channel > 31){
	esr = (u32 *)EDMACC_ESRH;
	shift = channel - 32;
	ipr = (u32 *)EDMACC_IPRH;
	icr = (u32 *)EDMACC_ICRH;
    } else {
	shift = channel;
	esr = (u32 *)EDMACC_ESR;
	ipr = (u32 *)EDMACC_IPR;
	icr = (u32 *)EDMACC_ICR;
    }
    
    /* Be sure we will receive completion */
    param->opt &= ~0x3F000;
    param->opt |= channel<<12;
    param->opt |= 0x100000;

    /* Trigger the DMA */
    *esr |= 1<<shift;
    
    /* Wait for the completion */
    while (!(inl(ipr) & (1<<shift)));
    *icr |= 1<<shift;

    return E_PASS;
}

/* 8 bit read function with DMA, this assumes the NAND chip supports CE don't care */
u32 nand_dma_read_buf(u8 * src, u8 * dst, u32 len)
{
    dma_param->opt = 0x4;	// Source is a FIFO, 8-bit long (but we used in incremental mode -see below-), so we need AB sync
    // destination is incremental, the rest is cared by the do_dma function
    dma_param->src = (u32) src;
    dma_param->dst = (u32) dst;

    /* Tricky... see the AEMIF guide, section 2.5.6.5 */
    dma_param->acnt = 8;
    dma_param->bcnt = len / dma_param->acnt;
    dma_param->ccnt = 1;
    dma_param->bcntrld = dma_param->bcnt;
    dma_param->srcbidx = 0;
    dma_param->dstbidx = dma_param->acnt;
    dma_param->srccidx = 0;
    dma_param->dstcidx = 0;
    dma_param->link = 0xffff;

    return do_dma(DMA_CHANNEL);
}

