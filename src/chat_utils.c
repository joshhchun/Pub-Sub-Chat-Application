#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <sys/epoll.h>
#include <unistd.h>


#include <unistd.h>
#include "mq/chat_app.h"

void init_curses() {
    initscr();
    scrollok(stdscr,TRUE);
    if (!has_colors() || start_color()) {
        fprintf(stderr, "Need colored terminal\n");
        exit(1);
    }
    init_pair(1, COLOR_RED, 0);
    init_pair(2, COLOR_YELLOW, 0);
    init_pair(3, COLOR_CYAN, 0);
    init_pair(4, COLOR_MAGENTA, 0);
    init_pair(5, COLOR_GREEN, 0);
    init_pair(6, COLOR_BLUE, 0);
    init_pair(7, COLOR_WHITE, COLOR_YELLOW);
}
int epoll_setup(MessageQueue* mq) {
	// Epoll setup
	int s;
	struct epoll_event user, server;
	// Create the epoll fd
	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		fprintf(stderr, "Failed to create epoll file descriptor\n");
		return -1;
	}
    // Register stdin fd
	user.events = EPOLLIN;
	user.data.fd = 0;
  	user.events = EPOLLIN | EPOLLET;
  	s = epoll_ctl (epoll_fd, EPOLL_CTL_ADD, 0, &user);
  	if (s == -1)
    {
      fprintf(stderr, "Failed to add stdin to epoll interest list");
      return -1;
    }
    server.data.fd = mq->p[0];
    server.events = EPOLLIN | EPOLLET;
  	s = epoll_ctl (epoll_fd, EPOLL_CTL_ADD, mq->p[0], &server);
  	if (s == -1)
    {
      fprintf(stderr, "Failed to add second pipe to epoll interest list");
      return -1;
    }
    return epoll_fd;
}

// Function to push a new channel node into the channel list
int push_node(Channels* channels, char* topic) {
    char* dyn_topic = strdup(topic);
    if (!dyn_topic) return 1;
    Node* new_node = calloc(1, sizeof(Node));
    if (!new_node) {
        free(dyn_topic);
        return 1;
    }
    char** temp_buf = (char**)calloc(MAX_MESSAGES, sizeof(char*));
    if (!temp_buf) {
        free(dyn_topic);
        free(new_node);
        return 1;
    };
    new_node->topic = dyn_topic;
    new_node->write = 0;
    new_node->read = 0;
    new_node->buffer_history = temp_buf;
    new_node->next = NULL;
    if (!channels->head) {
       channels->head = new_node; 
    } else {
        new_node->next = channels->head;
        channels->head = new_node;
    }
    return 0;
}

// Function to save the message in the ring buffer
void save_message(Node* curr_chat, char* name, char* body) {
	char msg[BUFSIZ];
	sprintf(msg, "%s %s %s", name, curr_chat->topic, body);
	char* dyn_msg = strdup(msg);
	if (!dyn_msg) {
		printw("could not allocate message\n");	
		return;
	}
	// If we reached the end of the circular buffer, wrap around and remove oldest entry and update read
	if (curr_chat->write >= MAX_MESSAGES) {
	    curr_chat->read++;
	    free(curr_chat->buffer_history[curr_chat->write % MAX_MESSAGES]);
	}
	curr_chat->buffer_history[curr_chat->write++ % MAX_MESSAGES] = dyn_msg;
	refresh();
}

// Function to delete a channel node from the channel list
int delete_channel(Channels* channels, char* topic) {
    Node* prev = NULL;
    for (Node* curr_node = channels->head; curr_node; curr_node = curr_node->next) {
        if (!strcmp(curr_node->topic, topic)) {
            // Handle removing the head
            if (!prev) {
                channels->head = NULL;
            } else {
                prev->next = curr_node->next;
            }
            free_node(curr_node);
            return 1;
        }
        prev = curr_node;
    }
    return 0;
}

// Function to find a certain channel node from the channel list
Node* find_channel(Channels* channels, char* topic) {
    for (Node* curr_node = channels->head; curr_node; curr_node = curr_node->next) {
        if (!strcmp(curr_node->topic, topic)) {
            return curr_node;
        }
    }
    return NULL;
};

// Function to print all of the current subscriptions
void print_channels(Channels* channels) {
	attron(A_BOLD | COLOR_PAIR(MAGENTA));
	printw("--------------------------\n");
	attron(A_UNDERLINE);
    printw("Channels\n");
	attroff(A_UNDERLINE);
    for (Node* curr_node = channels->head; curr_node; curr_node = curr_node->next) {
        printw("> %s\n", curr_node->topic);
    }
	printw("--------------------------\n");
    attroff(A_BOLD | COLOR_PAIR(MAGENTA));
}

void print_menu() {
	attron(A_BOLD | COLOR_PAIR(MAGENTA));
	printw("--------------------------\n");
	attron(A_UNDERLINE);
	printw("Menu Options:\n");
	attroff(A_UNDERLINE);
	printw("/subscribe [topic]\n");
	printw("/switch [topic]\n");
	printw("/unsubscribe [topic]\n");
	printw("/topic\n");
	printw("--------------------------\n");
	attroff(A_BOLD | COLOR_PAIR(MAGENTA));
}
// Function to free all of the buffers
void free_buffers(Node* curr) {
    if (!curr) return;
    free_buffers(curr->next);
    free_node(curr);
}

// Function to delete a particular channel
void free_node(Node* curr) {
    free(curr->topic);
    for (int i = 0; i < MAX_MESSAGES; i++) {
        if (curr->buffer_history[i]) free(curr->buffer_history[i]);
        else break;
    }
    free(curr->buffer_history);
    free(curr);
}
/* djb2 hash (http://www.cse.yorku.ca/~oz/hash.html) */
unsigned long hash(char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}





