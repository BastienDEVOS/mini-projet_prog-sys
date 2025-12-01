#ifndef COMMON_H
#define COMMON_H

#define MSG_SIZE 256

// Structure standardisée pour les messages envoyés via mq
typedef struct {
    char filename[128];   // Nom du fichier détecté
    int event_mask;       // Code inotify: IN_CREATE, IN_MODIFY, etc.
} file_event_t;

#endif
