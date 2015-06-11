#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "mongoose.h"

extern void http_home_page (struct mg_connection *conn, int log);
extern void http_progress_page (struct mg_connection *conn, int enable_abort);
extern void http_reboot_page (struct mg_connection *conn);
extern void http_refresh_page (struct mg_connection *conn, char *msg);
extern void http_log_page (struct mg_connection *conn);
extern void clear_log (void);

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

enum {
    RESCUE_STATE_HOME,
    RESCUE_STATE_PROGRESS,
    RESCUE_STATE_LOG,
    RESCUE_STATE_REBOOT,
};

enum {
    RESCUE_REQUEST_UPDATE,
    RESCUE_REQUEST_ABORT,
    RESCUE_REQUEST_BACK,
    RESCUE_REQUEST_LOG,
    RESCUE_REQUEST_REBOOT,
    RESCUE_REQUEST_FAVICON,
    RESCUE_REQUEST_UNKNOWN,
};

static pthread_t thread;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mg_mutex = PTHREAD_MUTEX_INITIALIZER;

static char exec_cmd[PATH_MAX];

static const char update_cmd[] = "/usr/bin/fwupdate.sh %s";

static int runned = 0,
           result = 0,
           history = 0,
           state = RESCUE_STATE_HOME,
           abort_enabled = 0,
           exec_pid = -1,
           upload = 0,
           req = RESCUE_REQUEST_UNKNOWN;

extern char **environ;
typedef void (*sighandler_t)(int);

static int do_system_cmd (const char *cmd)
{
    int status, pid;
    sighandler_t save_quit, save_int, save_chld;
    char *argv[4] = {"sh", "-c", NULL, NULL};

    if (cmd == NULL)
        return 1;

    argv[2] = (char*)cmd;

    save_quit = signal(SIGQUIT, SIG_IGN);
    save_int = signal(SIGINT, SIG_IGN);
    save_chld = signal(SIGCHLD, SIG_DFL);

    pid = fork();
    if ((pid) < 0) {
        status = -1;
        goto exit;
    }
    if (pid == 0) {
        signal(SIGQUIT, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        execve("/bin/sh", (char *const *) argv, environ);
        _exit(127);
    }

    pthread_mutex_lock(&mutex);
    exec_pid = pid;
    pthread_mutex_unlock(&mutex);

    if (waitpid(pid, &status, 0) == -1) status = -1;

exit:
    signal(SIGQUIT, save_quit);
    signal(SIGINT, save_int);
    signal(SIGCHLD, save_chld);

    if (WIFEXITED(status)) return(WEXITSTATUS(status));

    return -1;
}

static void * thread_func (void *cmd)
{
    int r;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    if (cmd) {
        r = do_system_cmd(cmd);
        printf("Run '%s'\n",cmd);
    }
    else fprintf(stderr,"couldn't to exec NULL cmd\n");

    pthread_mutex_lock(&mutex);
    result = r;
    runned = 0;
    pthread_mutex_unlock(&mutex);

    printf("result %d\n", r);

    return NULL;
}

static void run (char *cmd)
{
    if (runned) return;

    if (pthread_create(&thread, NULL, thread_func, cmd)) {
        perror("failed to create thread");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&mutex);
    runned = 1;
    history = 1;
    pthread_mutex_unlock(&mutex);
}

static void stop (void)
{
    int pid;

    if (!runned) return;

    pthread_mutex_lock(&mutex);
    pid = exec_pid;
    pthread_mutex_unlock(&mutex);

    if (pid > 0) kill(pid,SIGKILL);

    if (!pthread_cancel(thread)) pthread_join(thread, NULL);

    pthread_mutex_lock(&mutex);
    runned = 0;
    exec_pid = -1;
    result = -1;
    pthread_mutex_unlock(&mutex);
}

static int process (struct mg_connection *conn, const char *cmd, int abort, int new_state)
{
    clear_log();
    abort_enabled = abort;
    state = new_state;
    run(cmd);
    sleep(1);
    http_progress_page(conn, abort);
}

static void rescue_reboot (struct mg_connection *conn)
{
    http_reboot_page(conn);
    pthread_mutex_unlock(&mg_mutex);
    sleep(2);
    run("/sbin/reboot");
    for(;;);
}

typedef struct {
    const char  *name;
    int         id;
} rescue_request_t;

static rescue_request_t reqs[] = {
    { "/upload",        RESCUE_REQUEST_UPDATE },
    { "/abort",         RESCUE_REQUEST_ABORT },
    { "/back",          RESCUE_REQUEST_BACK },
    { "/log",           RESCUE_REQUEST_LOG },
    { "/reboot",        RESCUE_REQUEST_REBOOT },
    { "/favicon.ico",   RESCUE_REQUEST_FAVICON },
};

static char buf[1024];

static int parse_request (struct mg_request_info *info)
{
    char i;
    const char *uri = info->uri;

    for (i = 0; i < sizeof(reqs)/sizeof(reqs[0]); i++) {
        //printf("uri %s, name %s, %d\n", uri, reqs[i].name, reqs[i].id);
        if (!strcmp(uri, reqs[i].name))  return (reqs[i].id);
    }

    return RESCUE_REQUEST_UNKNOWN;
}

static char * form_values[4];
static char * form_names[4];

static const char form_prefix[] = "Content-Disposition: form-data; name=";
//static const char form_boundary[] = "------WebKitFormBoundary";
static const char form_boundary[] = "----";

static void parse_form (struct mg_connection *conn)
{
    int len, prefix_size, boundary_size, i, j, n, w;
    char *p;

    len = mg_read(conn, buf, 1024);
    if (len <= 0) {
        fprintf(stderr, "parse_form: failed to read data (%d)\n", len);
        return;
    }

    printf("%s\n\n",buf);

    prefix_size = sizeof(form_prefix) - 1;
    boundary_size = sizeof(form_boundary) - 1;

    for (i = 0; i < sizeof(form_values)/sizeof(form_values[0]); i++)
        form_values[i] = NULL;

    buf[1023] = 0;

    for (i = 0; i < len;) {
        if (strncmp(form_prefix, &buf[i], prefix_size)) { i++; continue; }
        i += prefix_size;
        w = 0;
        for (j = 0; j < sizeof(form_names)/sizeof(form_names[0]); j++) {
            if (form_names[j] == NULL) break;
            n = strlen(form_names[j]);
            if (!strncmp(form_names[j], &buf[i], n)) {
                w = 1;
                i += n;
                break;
            }
        }
        if (!w) continue;
        while (buf[i] == '\r' && buf[i+1] == '\n') i += 2;
        for (n = i; n < len; n++) {
            if (!strncmp(form_boundary, &buf[n], boundary_size)) {
                if (i != n) {
                    form_values[j] = &buf[i];
                    buf[n - 2] = 0;
                }
                i = n + boundary_size;
                break;
            }
        }
    }
}

static int begin_request_handler (struct mg_connection *conn)
{
    int r, s, h, w;
    struct mg_request_info *info;

    pthread_mutex_lock(&mg_mutex);

    info = mg_get_request_info(conn);
    req = parse_request(info);

    if (req == RESCUE_REQUEST_FAVICON) {
        pthread_mutex_unlock(&mg_mutex);
        return 0;
    }

    pthread_mutex_lock(&mutex);
    s = runned;
    r = result;
    h = history;
    pthread_mutex_unlock(&mutex);

    switch (state) {
    case RESCUE_STATE_HOME:
        switch (req) {
        case RESCUE_REQUEST_UPDATE:
            upload = 0;
            if (mg_upload(conn, "/tmp") == 1 && upload) {
                process(conn,exec_cmd,1,RESCUE_STATE_PROGRESS);
                goto exit;
            }
            break;
        case RESCUE_REQUEST_LOG:
            state = RESCUE_STATE_LOG;
            http_log_page(conn);
            goto exit;
        case RESCUE_REQUEST_REBOOT:
            state = RESCUE_STATE_REBOOT;
            rescue_reboot(conn);
            break;
        }
        break;
    case RESCUE_STATE_PROGRESS:
        if (s) {
            if (req == RESCUE_REQUEST_ABORT) {
                state = RESCUE_STATE_LOG;
                stop();
                http_log_page(conn);
            }
            else http_progress_page(conn, abort_enabled);
        } else {
            state = RESCUE_STATE_LOG;
            http_log_page(conn);
        }
        goto exit;
    case RESCUE_STATE_LOG:
        if (req == RESCUE_REQUEST_BACK) break;
        http_log_page(conn);
        goto exit;
    case RESCUE_STATE_REBOOT:
        rescue_reboot(conn);
    }

    state = RESCUE_STATE_HOME;

    http_home_page(conn, h);

exit:
    pthread_mutex_unlock(&mg_mutex);
    /* always return '1' to mark request as processed */
    return 1;
}

static void upload_handler (struct mg_connection *conn, const char *path)
{
    const char *cmd = update_cmd;

    if (path) upload = 1;
    snprintf(exec_cmd, sizeof(exec_cmd) - 1, cmd, path);
    printf("%s uploaded.\n", path);
}

int main(void)
{
    struct stat st;
    struct mg_context *ctx;
    const char *options[] = {"listening_ports", "80", "document_root", "/var/www", NULL};
    struct mg_callbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = begin_request_handler;
    callbacks.upload = upload_handler;
    ctx = mg_start(&callbacks, NULL, options);
    for(;;) sleep(100);

    return 0;
}
