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

static unsigned char *ubl_image = NULL;
static unsigned char *app_image = NULL;
static char out_str[128];

static char port_default[] = "/dev/ttyUSB0";
static char *port;
static char *ubl = NULL;
static char *app = NULL;

static int ubl_size, app_size;
static int fd = -1;

static int timeout = 15;

static int erase = 0;

static void
sfh_exit (void)
{
    if (fd >= 0) { close(fd); fd = -1; }
    if (ubl_image) { free(ubl_image); ubl_image = NULL; }
    if (app_image) { free(app_image); app_image = NULL; }
}

static void
alarm_handler (int sig)
{
    fprintf(stderr,"Timeout\n");
    sfh_exit();
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

static unsigned char *
read_image (char *name, int * size)
{
    struct stat st;
    unsigned char *p;
    int s;

    printf("Reading image '%s'...\n",name);

    if (stat(name, &st)) {
		perror("Failed to get image size");
		return NULL;
	}

	s = st.st_size;

    p = malloc(s);
    if (p == NULL) {
        perror("Failed to allocate buffer");
        return NULL;
    }

	if ((fd = open(name, O_RDONLY)) < 0) {
		perror("Failed to open image file");
		return NULL;
	}

	if (read(fd, p, s) != s) {
		perror("Failed to read image");
		return NULL;
	}

    *size = s;
	close(fd);
    return p;
}

static void
send_image (unsigned char *p, uint32_t magic, uint32_t start, uint32_t size, uint32_t load, int image_timeout)
{
    int r, i, s;

    printf("Waiting for 'SENDIMG'\n");
    if (wait_string("SENDIMG",1)) exit(EXIT_FAILURE);
    printf("\n");

    timeout = 5;

    printf("Send header\n");
    put_string("    ACK",8);
    snprintf(out_str, 37, "%08x%08x%08x%08x0000", magic, start, size, load);
    put_string(out_str,36);

    printf("Waiting for 'BEGIN'\n");
    if (wait_string("BEGIN",0)) exit(EXIT_FAILURE);

    s = size;
    r = 0;
    while (s) {
        if (s > 2048) i = 2048;
        else          i = s;
        put_string(p, i);
        p += i;
        s -= i;
        r += i;
        printf("Transferred %d of %d bytes\r", r, size);
        fflush(stdout);
    }
    printf("\n");

    printf("Waiting for 'DONE'\n");
    if (wait_string("DONE",0)) exit(EXIT_FAILURE);
    timeout = image_timeout;
}

static struct option options[] = {
    {"port",       required_argument, 0, 'p'},
    {"erase",      no_argument,       0, 'e'},
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
            if (erase) return;
            if (optind >= argc) {
                fprintf(stderr, "Expected image file name\n");
                ret = EXIT_FAILURE;
                goto print_usage;
            }
            ubl = argv[optind];
            app = argv[optind + 1];
            return;
        case 0:  break;
        case 'p':
            port = optarg;
            break;
        case 'e':
            erase = 1;
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
           "  -e, --erase           nand chip erasing\n"
           "  -h, --help            print usage and exit\n",
           argv[0], port_default);
    exit(ret);
}

int main(int argc, char **argv)
{
    int i;
    unsigned int *p;
    struct sigaction actions;
    struct termios old, cur;

    parse_args(argc, argv);

    if (!erase) {
        if (!ubl) {
            fprintf(stderr,"no 'ubl' is specified\n");
            return EXIT_FAILURE;
        }
        if (!app) {
            fprintf(stderr,"no 'application' is specified\n");
            return EXIT_FAILURE;
        }
        ubl_image = read_image(ubl, &ubl_size);
        if (!ubl_image) return EXIT_FAILURE;
        app_image = read_image(app, &app_size);
        if (!app_image) return EXIT_FAILURE;
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
    cur.c_cflag |= (CLOCAL | CS8);

    cur.c_lflag &= 0;

    cfsetspeed(&cur, B115200);

    if (tcsetattr(fd, TCSANOW | TCSAFLUSH, &cur)) {
        fprintf(stderr,"Failed to set terminal attributes: %s\n", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    atexit(sfh_exit);
    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = alarm_handler;
    sigaction(SIGALRM,&actions,NULL);

    printf("Waiting for 'BOOTUBL'\n");
    if (wait_string("BOOTUBL",0)) exit(EXIT_FAILURE);

    if (erase) {
        printf("Send 'UBL_MAGIC_NAND_ERASE' command\n");
        put_string("    CMD",8);
        put_string("a1aceddd",8);

        if (wait_string("DONE",0)) exit(EXIT_FAILURE);

        timeout = 600;

        printf("Waiting for 'DONE'\n");
        if (wait_string("DONE",1)) exit(EXIT_FAILURE);
        printf("\n");

        exit(0);
    }

    printf("Send 'UBL_MAGIC_NAND_FLASH' command\n");
    put_string("    CMD",8);
    put_string("a1acedcc",8);

    send_image(ubl_image, 0xa1aced00, 0x20, ubl_size, 0x20, 60);
    send_image(app_image, 0xa1aced66, 0x80008000, app_size, 0x80008000, 500);

    for (;;) {
        unsigned char c;
        if (read(fd,&c,1) == 1) write(1,&c,1);
    }

    return 0;
}
