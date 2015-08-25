#include <string.h>
#include "tistdtypes.h"
#include "util.h"
#include "debug.h"
#include "device.h"

typedef struct davinci_usb_regs {
    VUint32 version;
    VUint32 ctrlr;
    VUint32 reserved[0x20];
    VUint32 intclrr;
    VUint32 intmskr;
    VUint32 intmsksetr;
} davinci_usb_regs_t;

/* EP0 */
struct musb_ep0_regs {
    VUint16 reserved4;
    VUint16 csr0;
    VUint16 reserved5;
    VUint16 reserved6;
    VUint16 count0;
    VUint8  host_type0;
    VUint8  host_naklimit0;
    VUint8  reserved7;
    VUint8  reserved8;
    VUint8  reserved9;
    VUint8  configdata;
};

/* EP 1-15 */
struct musb_epN_regs {
    VUint16 txmaxp;
    VUint16 txcsr;
    VUint16 rxmaxp;
    VUint16 rxcsr;
    VUint16 rxcount;
    VUint8  txtype;
    VUint8  txinterval;
    VUint8  rxtype;
    VUint8  rxinterval;
    VUint8  reserved0;
    VUint8  fifosize;
};

struct musb_regs {
    /* common registers */
    VUint8 faddr;
    VUint8 power;
    VUint16 intrtx;
    VUint16 intrrx;
    VUint16 intrtxe;
    VUint16 intrrxe;
    VUint8 intrusb;
    VUint8 intrusbe;
    VUint16 frame;
    VUint8 index;
    VUint8 testmode;
    /* indexed registers */
    VUint16 txmaxp;
    VUint16 txcsr;
    VUint16 rxmaxp;
    VUint16 rxcsr;
    VUint16 rxcount;
    VUint8 txtype;
    VUint8 txinterval;
    VUint8 rxtype;
    VUint8 rxinterval;
    VUint8 reserved0;
    VUint8 fifosize;
    /* fifo */
    VUint32 fifox[16];
    /* OTG, dynamic FIFO, version & vendor registers */
    VUint8 devctl;
    VUint8 reserved1;
    VUint8 txfifosz;
    VUint8 rxfifosz;
    VUint16 txfifoadd;
    VUint16 rxfifoadd;
    VUint32 vcontrol;
    VUint16 hwvers;
    VUint16 reserved2a[1];
    VUint8 ulpi_busctl;
    VUint8 reserved2b[1];
    VUint16 reserved2[3];
    VUint8 epinfo;
    VUint8 raminfo;
    VUint8 linkinfo;
    VUint8 vplen;
    VUint8 hseof1;
    VUint8 fseof1;
    VUint8 lseof1;
    VUint8 reserved3;
    /* target address registers */
    struct musb_tar_regs {
        VUint8 txfuncaddr;
        VUint8 reserved0;
        VUint8 txhubaddr;
        VUint8 txhubport;
        VUint8 rxfuncaddr;
        VUint8 reserved1;
        VUint8 rxhubaddr;
        VUint8 rxhubport;
    } tar[16];
    /* endpoint registers
       ep0 elements are valid when array index is 0
       otherwise epN is valid */
    union musb_ep_regs {
        struct musb_ep0_regs ep0;
        struct musb_epN_regs epN;
    } ep[16];
} __attribute__((packed, aligned(32)));

typedef struct musb_regs musb_regs_t;

static davinci_usb_regs_t *dregs;
static musb_regs_t        *musbr;

static void mdelay (unsigned int msec)
{
    if (!msec) return;

    do {
        DEVICE_TIMER0Start();
        while (DEVICE_TIMER0Status());
    } while (--msec);
}

/* Integrated highspeed/otg PHY */
#define USBPHY_CTL_PADDR    (0x01c40000 + 0x34)
#define USBPHY_PHY24MHZ     (1 << 13)
#define USBPHY_PHYCLKGD     (1 << 8)
#define USBPHY_SESNDEN      (1 << 7)    /* v(sess_end) comparator */
#define USBPHY_VBDTCTEN     (1 << 6)    /* v(bus) comparator */
#define USBPHY_PHYPLLON     (1 << 4)    /* override pll suspend */
#define USBPHY_CLKO1SEL     (1 << 3)
#define USBPHY_OSCPDWN      (1 << 2)
#define USBPHY_PHYPDWN      (1 << 0)

static int
phy_on (void)
{
	unsigned int msec = 5;

	/* Wait until the USB phy is turned on */
    *((volatile unsigned int *)USBPHY_CTL_PADDR) = USBPHY_PHY24MHZ | USBPHY_SESNDEN | USBPHY_VBDTCTEN;

	do {
		if (*((volatile unsigned int *)USBPHY_CTL_PADDR) & USBPHY_PHYCLKGD)
			return 0;
        mdelay(1);
    } while (--msec);

    DEBUG_printString("ERROR : USB phy was not turned on\r\n");
	return -1;
}

/* POWER */
#define MUSB_POWER_ISOUPDATE    0x80
#define MUSB_POWER_SOFTCONN     0x40
#define MUSB_POWER_HSENAB       0x20
#define MUSB_POWER_HSMODE       0x10
#define MUSB_POWER_RESET        0x08
#define MUSB_POWER_RESUME       0x04
#define MUSB_POWER_SUSPENDM     0x02
#define MUSB_POWER_ENSUSPEND    0x01

/* DEVCTL */
#define MUSB_DEVCTL_BDEVICE     0x80
#define MUSB_DEVCTL_FSDEV       0x40
#define MUSB_DEVCTL_LSDEV       0x20
#define MUSB_DEVCTL_VBUS        0x18
#define MUSB_DEVCTL_VBUS_SHIFT  3
#define MUSB_DEVCTL_HM          0x04
#define MUSB_DEVCTL_HR          0x02
#define MUSB_DEVCTL_SESSION     0x01

/* Base address of DAVINCI usb0 wrapper */
#define DAVINCI_USB0_BASE 0x01C64000
/* Base address of DAVINCI musb core */
#define MENTOR_USB0_BASE (DAVINCI_USB0_BASE+0x400)

#define DAVINCI_USB_TX_ENDPTS_MASK  0x1f /* ep0 + 4 tx */
#define DAVINCI_USB_RX_ENDPTS_MASK  0x1e /* 4 rx */
#define DAVINCI_USB_USBINT_SHIFT    16
#define DAVINCI_USB_TXINT_SHIFT     0
#define DAVINCI_USB_RXINT_SHIFT     8
#define DAVINCI_INTR_DRVVBUS        0x0100

#define DAVINCI_USB_USBINT_MASK     0x01ff0000  /* 8 Mentor, DRVVBUS */
#define DAVINCI_USB_TXINT_MASK (DAVINCI_USB_TX_ENDPTS_MASK << DAVINCI_USB_TXINT_SHIFT)
#define DAVINCI_USB_RXINT_MASK (DAVINCI_USB_RX_ENDPTS_MASK << DAVINCI_USB_RXINT_SHIFT)

int usb_detect (void)
{
    unsigned char b;

    /* start the on-chip USB phy and its pll */
	if (phy_on()) return -1;

	/* reset the controller */
    dregs = (davinci_usb_regs_t *)DAVINCI_USB0_BASE;
    dregs->ctrlr = 1;

    mdelay(10);

    /* Read zero if e.g. not clocked */
    if (!dregs->version) {
        DEBUG_printString("ERROR : USB is not clocked\r\n");
        return -2;
    }

    /* Disable all interrupts */
    dregs->intmsksetr = DAVINCI_USB_USBINT_MASK | DAVINCI_USB_RXINT_MASK | DAVINCI_USB_TXINT_MASK;

	musbr = (musb_regs_t *)MENTOR_USB0_BASE;

	/* Check if device is in b-peripheral mode */
	b = musbr->devctl;
#if 0
    DEBUG_printString("!!! devctl ");
    DEBUG_printHexInt(b);
    DEBUG_printString("\r\n");
#endif
	if (!(b & MUSB_DEVCTL_BDEVICE) || (b & MUSB_DEVCTL_HM)) {
        DEBUG_printString("ERROR : Unsupport USB mode\r\nCheck that mini-B USB cable is attached to the device\r\n");
        return -1;
    }

    return ((b >> 3) & 3);
}
