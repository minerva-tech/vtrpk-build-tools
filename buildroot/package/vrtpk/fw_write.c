#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <getopt.h>

#include <mtd/mtd-user.h>
#include <mtd/mtd-abi.h>
#include <mtd/libmtd.h>

static const char *label;
static const char *image;

static int verbose = 0;

static int page = 2048;

static unsigned int magic = 0xa1aced00;

static unsigned int entry = 0xffffffff;
static unsigned int addr = 0xffffffff;

static char dev_name[20];

static struct option options[] = {
    { "address"  , required_argument , 0, 'a' },
    { "entry"    , required_argument , 0, 'e' },
    { "file"     , required_argument , 0, 'f' },
    { "label"    , required_argument , 0, 'l' },
    { "magic"    , required_argument , 0, 'm' },
    { "page"     , required_argument , 0, 'p' },
    { "verbose"  , no_argument       , 0, 'v' },
    { "help"     , no_argument       , 0, 'h' },
    {0, 0, 0, 0}
};

static void
parse_args (int argc, char **argv)
{
    int index = 0, ret = 0;
    int d;
    unsigned int x;

    for (;;) {
        switch (getopt_long(argc, argv, "a:e:f:l:m:p:vh", options, &index)) {
        case -1: return;
        case 0:  break;
        case 'a':
            if (sscanf(optarg,"%x",&x) != 1) {
                fprintf(stderr,"Error: wrong 'address' option value: %s: $s\n", optarg, strerror(errno));
                goto print_usage;
            }
            addr = x;
            break;
        case 'e':
            if (sscanf(optarg,"%x",&x) != 1) {
                fprintf(stderr,"Error: wrong 'entry' option value: %s: $s\n", optarg, strerror(errno));
                goto print_usage;
            }
            entry = x;
            break;
        case 'f':
            image = optarg;
            break;
        case 'l':
            label = optarg;
            break;
        case 'm':
            if (sscanf(optarg,"%x",&x) != 1) {
                fprintf(stderr,"Error: wrong 'page' option value: %s: $s\n", optarg, strerror(errno));
                goto print_usage;
            }
            magic = x;
            break;
        case 'p':
            if (sscanf(optarg,"%d",&d) != 1) {
                fprintf(stderr,"Error: wrong 'page' option value: %s: $s\n", optarg, strerror(errno));
                goto print_usage;
            }
            page = d;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
            goto print_usage;
        case '?':
            goto print_error;
        default:
            abort();
        }
    }

print_error:

    fprintf(stderr, "Error: wrong options\n");
    ret = EXIT_FAILURE;

print_usage:

    printf("Usage: %s [options]\n"
           "Options:\n"
           "  -a, --address=XXXXXXXX   image load address (hex)\n"
           "  -e, --entry=XXXXXXXX     image entry address (hex)\n"
           "  -f, --file=IMAGE         firmware image file\n"
           "  -l, --label=LABEL        mtd device label\n"
           "  -m, --magic=XXXXXXXX     magic number (hex), default - %08x\n"
           "  -p, --page=dddd          NAND page size, default - %d\n"
           "  -v, --verbose            verbose output\n"
           "  -h, --help               print usage and exit\n",
           argv[0], magic, page);
    exit(ret);
}

/* returns number of erased blocks */
static int
erase_block (libmtd_t mtd, struct mtd_dev_info *dev, int fd, int number)
{
    int r = mtd_is_bad(dev, fd, number);

    if (r > 0) {
        if (verbose) printf("Skipping bad block %d\n", number);
        return 1;
    }

    if (r < 0) {
        if (errno == EOPNOTSUPP)
            fprintf(stderr, "Error: bad block check not available\n");
        else
            fprintf(stderr, "Error: get bad block failed\n");
        return -1;
    }

    if (mtd_erase(mtd, dev, fd, number)) {
        fprintf(stderr,"Error: block %d erasing failed\n", number);
        return 1;
    }

    if (verbose)
        printf("Block %d is erased\n", number);

    return 0;
}

static int
write_block (libmtd_t mtd, struct mtd_dev_info *dev, int fd, int number, unsigned char *buf, int size)
{
    int r = mtd_write(mtd, dev, fd, number, 0, buf, size, NULL, 0, 0);

    if (!r) {
        if (verbose) printf("Block %d is written\n", number);
        return 0;
    }

    if (errno != EIO) {
        fprintf(stderr,"Error: writing failed: %s\n", strerror(errno));
        return -1;
    }

    if (verbose)
        fprintf(stderr, "Marking block %d bad\n", number);

    if (mtd_mark_bad(mtd, fd, number)) {
        fprintf(stderr, "Error: bad block marking failed: %s\n", strerror(errno));
        return -1;
    }

    return 1;
}

int
main (int argc, char **argv)
{
    unsigned char *buf;
    unsigned int *header;
    struct stat st;
    libmtd_t libmtd;
	struct mtd_info mtd_info;
    struct mtd_dev_info *mtd_devs;
    struct mtd_dev_info *dev;
    int i, j, r, size, fd, dev_fd, pages, blocks, copies;
    int ret = EXIT_FAILURE;
    int dev_num = -1;
    unsigned long long offset = 0;

    parse_args (argc, argv);

    if (label == NULL) {
        fprintf(stderr,"Error: device label is not specified\n");
        return EXIT_FAILURE;
    }

    if (image == NULL) {
        fprintf(stderr,"Error: image file is not specified\n");
        return EXIT_FAILURE;
    }

    if (addr == 0xffffffff) {
        fprintf(stderr,"Error: load address is not specified\n");
        return EXIT_FAILURE;
    }

    if (entry == 0xffffffff) entry = addr;

    if (verbose)
        printf("Magic number = %08x, page size = %d, load address = %x, entry point = %x\n", magic, page, addr, entry);


    libmtd = libmtd_open();
	if (libmtd == NULL) {
		if (errno == 0)
            fprintf(stderr,"Error: MTD is not present in the system\n");
        else
            fprintf(stderr,"Error: cannot open libmtd: %s\n",strerror(errno));
        goto mtd_close;
	}

	if (mtd_get_info(libmtd, &mtd_info)) {
		if (errno == ENODEV)
			fprintf(stderr,"Error: MTD is not present");
        else
            fprintf(stderr,"Error: cannot get MTD information: %s\n",strerror(errno));
        goto mtd_close;
	}

    mtd_devs = malloc(sizeof(struct mtd_dev_info) * mtd_info.mtd_dev_cnt);
    if (mtd_devs == NULL) {
        fprintf(stderr,"Error: cannot allocate memory\n",strerror(errno));
        goto mtd_close;
    }

    for (i = mtd_info.lowest_mtd_num; i < mtd_info.highest_mtd_num; i++) {
        dev = &mtd_devs[i];
        if (mtd_get_dev_info1(libmtd, i, dev)) {
            fprintf(stderr,"Error: cannot get info about mtd%d: %s\n", i, strerror(errno));
            goto mtd_free;
        }
        if (!strcmp(dev->type_str,"nand") && dev->type == MTD_NANDFLASH) {
            if (!strcmp(dev->name,label)) {
                dev_num = i;
                break;
            }
            offset += (unsigned long long)(dev->size);
        }
    }

    if (dev_num < 0) {
        fprintf(stderr,"Error: mtd device with label '%s' is not found\n", label);
        goto mtd_free;
    }

    dev = &mtd_devs[dev_num];

    if (verbose) {
        printf("MTD%d: name - '%s', size = %lld(%lldM), offset = %llu(%lluM), block size = %d(%dK)\n",
               dev_num, dev->name, dev->size, (dev->size)/(1024 * 1024),
               offset, offset/(1024 * 1024), dev->eb_size, (dev->eb_size)/1024);
    }

    offset /= dev->eb_size;

    sprintf(dev_name,"/dev/mtd%d",dev_num);
    
    if ((dev_fd = open(dev_name, O_RDWR)) < 0) {
        fprintf(stderr,"Error: cannot open %s: %s\n", dev_name, strerror(errno));
        goto mtd_free;
    }

    stat(image,&st);
    pages = (st.st_size + page - 1) / page;
    size = (pages + 1) * page;
    blocks = (size + dev->eb_size - 1) / dev->eb_size;

    buf = malloc(size);
    if (buf == NULL) {
        fprintf(stderr,"Error: cannot allocate image buffer\n",strerror(errno));
        goto dev_close;
    }

    /* fill first & last pages with 0xff */
    for (i = 0; i < page; i++) {
        buf[i] = buf[pages + i] = 0xff;
    }

    /* fill header */
    header = (unsigned int *)buf;
    header[0] = magic;
    header[1] = entry;
    header[2] = pages;  /* number of pages */
    header[3] = offset; /* start block number */
    header[4] = 1;      /* always start data in page 1 (this header goes in page 0) */
    header[5] = addr;

    fd = open(image, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: failed to open %s: %s\n", image, strerror(errno));
        goto fd_close;
    }

    i = read(fd, &buf[page], st.st_size);
    if (i < 0) {
        fprintf(stderr,"Error: failed to read %s: %s\n", image, strerror(errno));
        goto fd_close;
    }
    if (i != st.st_size) {
        fprintf(stderr,"Error: read %d image bytes, less than expected - %d\n", i, st.st_size);
        goto fd_close;
    }

    for (i = 0; i < dev->eb_cnt; i++) {
        if (erase_block(libmtd, dev, dev_fd, i) < 0)
            goto dev_close;
    }

    copies = 0;

    for (i = 0, j = 0; j + (blocks - i) <= dev->eb_cnt;) {

        int a = i * dev->eb_size;
        int s = size - a;

        if (s > dev->eb_size) s = dev->eb_size;

        if (!i) header[3] = j + offset; /* header: start block number */

        r = write_block(libmtd, dev, dev_fd, j, &buf[a], s);

        if (r < 0) {
            for (i = j; i < dev->eb_cnt; i++)
                erase_block(libmtd, dev, dev_fd, i);
            break;
        }

        if (++i >= blocks) {
            i = 0;
            copies++;
            if (verbose) printf("Image copy number %d is written\n", copies);
        }
        j += (r + 1);
    }

    if (!copies) fprintf(stderr,"Error: no one copy of '%s' is written\n", image);
    else         ret = 0;

fd_close:
    close(fd);
dev_close:
    close(dev_fd);
mtd_free:
    free(mtd_devs);
mtd_close:
    libmtd_close(libmtd);
buf_free:
    free(buf);
    return ret;
}
