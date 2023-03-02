/* chat_app.h: Chat client */

#ifndef CHAT_APP_H
#define CHAT_APP_H

#include <netdb.h>
#include <stdbool.h>
#include <sys/types.h>
#include "mq/thread.h"
#include "mq/client.h"

/* Constants */

#define BACKSPACE    127
#define MAX_SUBS     10
#define MAX_MESSAGES 100
#define NUM_COLORS   5

/* Structures */
typedef struct Node {
    char   *topic;
    int    read;
    int    write;
    char   **buffer_history;
    struct Node* next;
} Node;

typedef struct Channels {
    Node* head;
} Channels;

enum COLOR {
    RED     = 1,
    YELLOW  = 2,
    CYAN    = 3,
    MAGENTA = 4,
    GREEN   = 5,
    BLUE    = 6,
    MENTION = 7
};

/* Function declarations */
int             epoll_setup(MessageQueue* mq);
int             push_node(Channels* channels, char* topic);
int             delete_channel(Channels* channels, char* topic);
Node*           find_channel(Channels* channels, char* topic);
void            print_channels(Channels* channels);
void            save_message(Node* current_chat, char* name, char* temp_buf);
void            free_buffers(Node* curr);
void            free_node(Node* curr);
unsigned long   hash(char* string);
void            init_curses();
void            print_menu();

#endif

