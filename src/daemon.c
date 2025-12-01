#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <mqueue.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#define BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))
#define DEFAULT_WATCH_DIR "/tmp/default_watchdir"
#define DEFAULT_MQ_NAME "/filewatch_queue"
#define CONFIG_FILE "./config.ini"

static volatile sig_atomic_t running = 1;

char watch_dir[256] = DEFAULT_WATCH_DIR;
char mq_name[256] = DEFAULT_MQ_NAME;

void load_config(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) syslog(LOG_INFO,"Configuation introuvable");
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0]=='#'||line[0]=='\n') continue;
        char key[128], value[128];
        if (sscanf(line,"%127[^=]=%127s",key,value)==2){
            if(strcmp(key,"WATCH_DIR")==0) strncpy(watch_dir,value,sizeof(watch_dir));
            if(strcmp(key,"MQ_NAME")==0) strncpy(mq_name,value,sizeof(mq_name));
        }
    }
    fclose(f);
}

void handle_sigterm(int sig) { (void)sig; running = 0; }
void handle_sighup(int sig) {
    (void)sig;
    syslog(LOG_INFO,"SIGHUP reçu, relecture config...");
    load_config();
}

void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void load_config(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char key[128], value[128];
        if (sscanf(line, "%127[^=]=%127s", key, value) == 2) {
            if (strcmp(key, "WATCH_DIR") == 0) strncpy(watch_dir, value, sizeof(watch_dir));
            if (strcmp(key, "MQ_NAME") == 0) strncpy(mq_name, value, sizeof(mq_name));
        }
    }
    fclose(f);
}

int main(void) {
    int inotify_fd, wd;
    mqd_t mq;
    struct mq_attr attr;
    char buffer[BUF_LEN];

    openlog("filewatch_daemon", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Démarrage du daemon...");

    load_config();

    daemonize();

    signal(SIGTERM, handle_sigterm);
    signal(SIGHUP, handle_sighup);

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 256;
    attr.mq_curmsgs = 0;

    mq_unlink(mq_name);
    mq = mq_open(mq_name, O_CREAT | O_WRONLY, 0644, &attr);
    if (mq == (mqd_t)-1) {
        syslog(LOG_ERR, "Erreur mq_open: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    mkdir(watch_dir, 0755);

    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        syslog(LOG_ERR, "Erreur inotify_init: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    wd = inotify_add_watch(inotify_fd, watch_dir, IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE);
    if (wd == -1) {
        syslog(LOG_ERR, "Erreur inotify_add_watch sur %s: %s", watch_dir, strerror(errno));
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Surveillance de %s démarrée", watch_dir);

    while (running) {
        ssize_t len = read(inotify_fd, buffer, BUF_LEN);
        if (len <= 0) { usleep(200000); continue; }

        for (char *ptr = buffer; ptr < buffer + len;) {
            struct inotify_event *event = (struct inotify_event *)ptr;
            if (event->len > 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Fichier: %s - Event: 0x%x", event->name, event->mask);

                if (mq_send(mq, msg, strlen(msg) + 1, 0) == -1)
                    syslog(LOG_ERR, "Erreur mq_send: %s", strerror(errno));
                else
                    syslog(LOG_INFO, "Événement envoyé: %s", msg);
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(inotify_fd, wd);
    close(inotify_fd);
    mq_close(mq);
    syslog(LOG_INFO, "Daemon arrêté proprement.");
    closelog();

    return 0;
}