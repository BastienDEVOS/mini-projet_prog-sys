#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>

#include "common.h"

#define CONFIG_FILE "./config.ini"

static volatile sig_atomic_t running = 1;
char mq_name[256] = "/filewatch_queue";

void handle_sigterm(int sig) {
    (void)sig;
    running = 0;
}

// Lecture du MQ_NAME dans config.ini
void load_config(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[128], value[128];
        if (sscanf(line, "%127[^=]=%127s", key, value) == 2) {

            if (strcmp(key, "MQ_NAME") == 0)
                strncpy(mq_name, value, sizeof(mq_name));
        }
    }
    fclose(f);
}

// Petit helper pour écrire proprement dans log
void write_log(sem_t *sem, const char *msg) {
    sem_wait(sem);

    FILE *log = fopen("log_worker.txt", "a");
    if (log) {
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strlen(ts)-1] = '\0'; // remove \n

        fprintf(log, "[%s] %s\n", ts, msg);
        fclose(log);
    }

    sem_post(sem);
}

// Fonction de traitement selon type
void process_file(const char *file, sem_t *sem) {
    char logmsg[256];

    if (strstr(file, ".txt")) {
        mkdir("processed", 0755);

        char dest[256];
        snprintf(dest, sizeof(dest), "processed/%s", file);
        rename(file, dest);

        snprintf(logmsg, sizeof(logmsg),
                 "Fichier %s traité : déplacé vers processed/", file);
    }
    else if (strstr(file, ".log")) {
        // Compression simple via gzip
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "gzip -f %s", file);
        system(cmd);

        snprintf(logmsg, sizeof(logmsg),
                 "Fichier %s traité : compressé (.gz)", file);
    }
    else if (strstr(file, ".data")) {
        snprintf(logmsg, sizeof(logmsg),
                 "Fichier %s traité : analyse fictive réalisée", file);
    }
    else {
        snprintf(logmsg, sizeof(logmsg),
                 "Fichier %s ignoré (type non pris en charge)", file);
    }

    write_log(sem, logmsg);
}

int main(void) {
    load_config();

    signal(SIGTERM, handle_sigterm);

    // Ouverture de la file MQ
    mqd_t mq = mq_open(mq_name, O_RDONLY);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    // Création sémaphore (partagé par tous les forks)
    sem_t *sem = sem_open("/sem_log", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    printf("Worker lancé. En attente de messages...\n");

    // Boucle de réception
    while (running) {
        file_event_t event;
        
        ssize_t r = mq_receive(mq, (char *)&event, sizeof(event), NULL);
        if (r == -1) {
            if (errno == EINTR) continue;
            perror("mq_receive");
            break;
        }

        printf("Message reçu : %s\n", event.filename);

        // Traitement parallèle par fork()
        pid_t pid = fork();
        if (pid == 0) {
            // Processus enfant : traitement du fichier
            process_file(event.filename, sem);
            exit(0);
        }
    }

    mq_close(mq);
    sem_close(sem);

    return 0;
}