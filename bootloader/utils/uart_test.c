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
#include <time.h>

static char *image = NULL;
static unsigned char buf[1024];

static char default_image[] = "test.bin";

static char port_default[] = "/dev/ttyUSB0";
static char *port;

static int fd = -1;
static int image_fd = -1;

static int total = 0;

static int timeout = 10;

static void
test_exit (void)
{
    if (fd >= 0)       { close(fd); fd = -1; }
    if (image_fd >= 0) { close(image_fd); image_fd = -1; }
    exit(0);
}

static void
alarm_handler (int sig)
{
    fprintf(stderr,"\nTimeout\n");
    test_exit();
}

static void
write_data (unsigned char *p, int size)
{
    int i;
    
    while (size) {
        i = write(image_fd,p,size);
        if (i < 0) {
            perror("Failed to write image file");
            exit(EXIT_FAILURE);
        }
        size -= i;
        p += i;
    }
}

static int
read_data (int size)
{
    int i,s,r;
    unsigned char *p = buf;

    alarm(timeout);

    r = 0;
    while (size) {
        if (size > 1024) s = 1024;
        else             s = size;
        i = read(fd,p,s);
        if (i < 0) {
            perror("Failed to read data from serial port");
            return -1;
        }
        write_data(p,i);
        r += i;
        size -= i;
        p += i;
        total += i;
        printf("read %d bytes (%d)\r",r,total); fflush(stdout);
    }
    printf("\n");

    return 0;
}

static struct option options[] = {
    {"port",       required_argument, 0, 'p'},
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
        switch (getopt_long(argc, argv, "p:h", options, &index)) {
        case -1:
            if (optind < argc) image = argv[optind];
            else               image = default_image;
            return;
        case 0:  break;
        case 'p':
            port = optarg;
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
           "  -h, --help            print usage and exit\n",
           argv[0], port_default);
    exit(ret);
}

static void
test_sleep (int ms)
{
    struct timespec req = {
        .tv_sec = 0,
    };
    
    req.tv_nsec = ms * 1000000;
    
    nanosleep(&req,NULL);
}

int main(int argc, char **argv)
{
    int i;
    struct sigaction actions;
    struct termios old, cur;

    parse_args(argc, argv);
    
    image_fd = creat(image, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (image_fd < 0) {
        fprintf(stderr,"Failed to create '%s': %s\n",image,strerror(errno));
        exit(EXIT_FAILURE);
    }

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
    cur.c_cflag |= (CLOCAL | CS8 | CRTSCTS);

    cur.c_lflag &= 0;

    cfsetspeed(&cur, B115200);

    if (tcsetattr(fd, TCSANOW | TCSAFLUSH, &cur)) {
        fprintf(stderr,"Failed to set terminal attributes: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    atexit(test_exit);
    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = alarm_handler;
    sigaction(SIGALRM,&actions,NULL);
    
    sleep(10);

    for (i = 0; ; i++) {
        read_data(1024);
        timeout = 2;
        if (i > 64) {
            sleep(1);
            i = 0;
        }
    }

    return 0;
}
