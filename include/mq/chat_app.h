/* chat_app.h: Chat client */

#ifndef CHAT_APP_H
#define CHAT_APP_H

#include <netdb.h>
#include <stdbool.h>
#include "mq/thread.h"
#include "mq/client.h"

/* Constants */

#define BACKSPACE   127
#define MAX_SUBS    10
#define MAX_MESSAGES 5

/* Structures */
typedef struct Node {
    // String that holds the topic of the buffer
    char *topic;
    // Read and write index for the  buffer
    int read;
    int write;
    // Buffer to hold the messages of a topic
    char **buffer_history;
    // Pointer of the next buffer
    struct Node* next;
} Node;

typedef struct Channels {
    Node* head;
} Channels;

enum COLOR {
    RED,
    BLUE,
    CYAN,
    MAGENTA,
    GREEN
};

/* Function declarations */
int epoll_setup(MessageQueue* mq);
int push_node(Channels* channels, char* topic);
int delete_channel(Channels* channels, char* topic);
Node* find_channel(Channels* channels, char* topic);
void print_channels(Channels* channels);
void save_message(Node* current_chat, char* name, char* temp_buf);
void free_buffers(Node* curr);
void free_node(Node* curr);
void init_curses();

#endif

