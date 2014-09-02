#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

static char *image = NULL;
static unsigned char *buf = NULL;
static unsigned char tmp[5];

static char default_image[] = "image.bin";

static char port_default[] = "/dev/ttyUSB0";
static char *port;

static int fd = -1;

static int timeout = 15;

static int nand_skip_errors = 0;

static void
sdh_exit (void)
{
    if (fd >= 0) { close(fd); fd = -1; }
    if (buf) { free(buf); buf = NULL; }
}

static void
alarm_handler (int sig)
{
    fprintf(stderr,"Timeout\n");
    sdh_exit();
}

static int
read_data (unsigned char *p, int size)
{
    int i,s,r;

    alarm(timeout);

    r = 0;
    while (size) {
        if (size > 2048) s = 2048;
        else             s = size;
        i = read(fd,p,s);
        if (i < 0) {
            perror("Failed to read data from serial port");
            return -1;
        }
        r += i;
        size -= i;
        p += i;
        printf("read %d bytes (%d)\r",r,size); fflush(stdout);
    }
    printf("\n");

    return 0;
}

static int
wait_string (const char *str, int echo)
{
    char c;
    int i = 0;
    size_t n = strlen(str) + 1;

    alarm(timeout);

    for (;;) {
        switch (read(fd,&c,1)) {
        case 0: break;
        case 1:
            if (c == str[i]) {
                i++;
                if (i == n) {
                    alarm(0);
                    return 0;
                }
            }
            else i = 0;
            break;
        default:
            perror("Failed to read byte from serial port");
            return -1;
        }
        if (echo) write(1,&c,1);
    }

    return -1;
}

static int
put_string (char *s, int length)
{
    int r, i = 0;
    while (length) {
        r = write(fd,&s[i],length);
        if (r < 0) {
            perror("\nFailed to write data to serial port");
            exit(EXIT_FAILURE);
        }
        length -= r;
        i += r;
    }
    fsync(fd);
}

static struct option options[] = {
    {"port",       required_argument, 0, 'p'},
    {"error",      no_argument,       0, 'e'},
    {"help",       no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static void
parse_args (int argc, char **argv)
{
    int index = 0, ret = 0;
    int d;

    port = port_default;

    for (;;) {
        switch (getopt_long(argc, argv, "p:eh", options, &index)) {
        case -1:
            if (optind < argc) image = argv[optind];
            else               image = default_image;
            return;
        case 0:  break;
        case 'p':
            port = optarg;
            break;
        case 'e':
            nand_skip_errors = 1;
            break;
        case 'h':
            goto print_usage;
        case '?':
        default:
            goto print_error;
        }
    }

print_error:

    fprintf(stderr, "Error: wrong options\n");
    ret = EXIT_FAILURE;

print_usage:

    printf("Usage: %s [options] <image>\n"
           "Options:\n"
           "  -p, --port            serial port device, default = '%s'\n"
           "  -e, --error           ignore errors\n"
           "  -h, --help            print usage and exit\n",
           argv[0], port_default);
    exit(ret);
}

int main(int argc, char **argv)
{
    int i;
    unsigned int size;
    unsigned char *p;
    struct sigaction actions;
    struct termios old, cur;

    parse_args(argc, argv);

    fd = open(port, O_RDWR);
    if (fd < 0) {
        fprintf(stderr,"Failed to open %s: '%s'\n", port, strerror(errno));
        return EXIT_FAILURE;
    }

    if (tcgetattr(fd, &old)) {
        fprintf(stderr,"Failed to get terminal attributes: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }
    cur = old;

    cur.c_iflag &= ~(INPCK | ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON | IMAXBEL);
    cur.c_iflag |= (IGNBRK | IXANY);

    cur.c_oflag &= ~OPOST;

    cur.c_cflag &= ~(HUPCL | CSTOPB | PARENB | CSIZE);
    cur.c_cflag |= (CLOCAL | CS8);

    cur.c_lflag &= 0;

    cfsetspeed(&cur, B115200);

    if (tcsetattr(fd, TCSANOW | TCSAFLUSH, &cur)) {
        fprintf(stderr,"Failed to set terminal attributes: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    atexit(sdh_exit);
    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = alarm_handler;
    sigaction(SIGALRM,&actions,NULL);

    printf("Waiting for 'BOOTUBL'\n");
    if (wait_string("BOOTUBL",0)) exit(EXIT_FAILURE);

    if (nand_skip_errors) {
        printf("Send 'UBL_MAGIC_NAND_READ_ERR' command\n");
        put_string("    CMD",8);
        put_string("a1acedb8",8);
    }
    else {
        printf("Send 'UBL_MAGIC_NAND_READ' command\n");
        put_string("    CMD",8);
        put_string("a1acedbb",8);
    }

    printf("Waiting for 'DONE'\n");
    if (wait_string("DONE",0)) exit(EXIT_FAILURE);

    if (wait_string("DONE",1)) exit(EXIT_FAILURE);
    printf("\n");

    read_data(tmp,4);
    size = tmp[0] | (tmp[1] << 8) | (tmp[2] << 16) | (tmp[3] << 24);
    printf("Image size %d bytes, %02X %02X %02X %02X\n", size, tmp[0],tmp[1],tmp[2],tmp[3]);

    buf = malloc(size);
    if (buf == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    timeout = 600;

    read_data(buf,size);

    close(fd);

    fd = creat(image, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        fprintf(stderr,"Failed to create '%s': %s\n",image,strerror(errno));
        exit(EXIT_FAILURE);
    }

    p = buf;
    while (size) {
        i = write(fd,p,size);
        if (i < 0) {
            perror("Failed to write image file");
            exit(EXIT_FAILURE);
        }
        size -= i;
        p += i;
    }

    close(fd);

    return 0;
}
