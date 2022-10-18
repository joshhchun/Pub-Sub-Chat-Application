
/* shell.c
 * Demonstration of multiplexing I/O with a background thread also printing.
 **/

#include "mq/thread.h"
#include "mq/client.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <sys/epoll.h>


#include <unistd.h>

/* Constants */

#define BACKSPACE   127
#define MAX_SUBS    10
#define MAX_MESSAGES 50

typedef struct Node {
    // String that holds the topic of the buffer
    char *topic;
    // Read and write index for the  buffer
    int write;
    // Buffer to hold the messages of a topic
    char **buffer_history;
    // Pointer of the next buffer
    struct Node* next;
} Node;

typedef struct Channels {
    Node* head;
} Channels;

/* Function declarations */
int epoll_setup(MessageQueue* mq);
int push_node(Channels* channels, char* topic);
int delete_channel(Channels* channels, char* topic);
Node* find_channel(Channels* channels, char* topic);
void free_buffers(Node* curr);
void free_node(Node* curr);

/* Main Execution */

int main(int argc, char *argv[]) {
    /* ncurses stuff */
    initscr();
    scrollok(stdscr,TRUE);
    if (has_colors() && start_color());
    init_pair(1, COLOR_RED, 0);
    init_pair(2, COLOR_BLUE, 0);
    init_pair(3, COLOR_CYAN, 0);
    init_pair(4, COLOR_MAGENTA, 0);

	// create MessageQueue
	char *topic = "general";
    char *name = getenv("USER");
    char *host = "localhost";
    char *port = "9621";
    if (argc > 1) { host = argv[1]; }
    if (argc > 2) { port = argv[2]; }
    if (argc > 3) { name = argv[3]; }

	MessageQueue *mq = mq_create(name, host, port);
	if(!mq){
		printf("could not create messafe queue\n");
		return 1;
	}
	mq_start(mq);
	mq_subscribe(mq, topic);
    
    // Chat channels  setup
	Channels channel_list = { NULL };
	if (push_node(&channel_list, "general")) exit(1);
	Node* current_chat = channel_list.head;

    // Epoll setup
	struct epoll_event events[100];
    int epoll_fd = epoll_setup(mq);
    if (epoll_fd < 0) {
        printw("Error setting up epoll\n");
        exit(1);
    }
    int event_count;

    /* Foreground Thread */
    char   input_buffer[BUFSIZ] = "";	
    size_t input_index = 0;
    char inbuf[BUFSIZ];
    int num_subs = 1;
      
    while (!mq_shutdown(mq)) {
		printw("\r> %s", input_buffer);		// Write
		refresh();
    	event_count = epoll_wait(epoll_fd, events, 100, 30000);
    	for (int i = 0; i < event_count; i++) {
			// Stdin, user is writing
    		if (!events[i].data.fd) {
				char input_char = 0;
				input_char = getch();
    			if (input_char == '\n') {				// Process commands
    	  		    if (!strcmp(input_buffer, "/exit") || !strcmp(input_buffer, "/quit")) {
						mq_stop(mq);
						break;
					} else if (!strcmp(input_buffer, "/topic")){
					    attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(3));
                        printw("Current topic: %s\n", current_chat->topic);
					    attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(3));
				    } else if (!strcmp(input_buffer, "/menu")){
				        attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(4));
					    printw("/subscribe [topic]\n");
					    printw("/switch [topic]\n");
					    printw("/unsubscribe [topic]\n");
					    printw("/topic\n");
				        attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(4));
				    }  else if (strncmp(input_buffer, "/subscribe", 10) == 0 && strlen(input_buffer) > 10){
				        if (num_subs == MAX_SUBS) {
				            printw("Cannot subscribe, reached maximum number of subs\n");
				            continue;
				        } 
                        char* topic = strchr(input_buffer, ' ');
                        if (!topic){
                            printw("Correct usage: /subscribe [topic]\n");
                            continue;
                        }
				        topic++;
				        if (find_channel(&channel_list, topic)) {
				            printw("Already subscribed to that topic!\n");
				            continue;
				        }
				        attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(3) );
				        printw("SUBSCRIBED TO TOPIC: %s\n", topic);
				        attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(3));
				        mq_subscribe(mq, topic);
				        // Increment subs
				        num_subs++;
				        // Push topic into channels linked list
				        if (push_node(&channel_list, topic)) {
				            printw("Error creating channel\n");
				            num_subs--;
				            mq_unsubscribe(mq, topic);
				        }
                    } else if (!strncmp(input_buffer, "/unsubscribe", 12) && strlen(input_buffer) > 12){
                        char* topic = strchr(input_buffer, ' ');
                        if (!topic) {
                            printw("Correct usage: /unsubscribe [topic]\n");
                            continue;
                        }
                        topic++;
                        int deleted = delete_channel(&channel_list, topic);
                        if (!deleted) {
                            printw("You were never subscribed to this channel\n");
                            continue;
                        }
				        attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(3) );
				        printw("UNSUBSCRIBED TO TOPIC: %s\n", topic);
				        attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(3));
				        mq_unsubscribe(mq, topic);
                        num_subs--;
                    }
                    else if (!strncmp(input_buffer, "/switch", 7) && strlen(input_buffer) > 7){
                        char* topic = strchr(input_buffer, ' ');
                        if (!topic) {
                            printw("Correct usage: /switch [topic]\n");
                            continue;
                        }
				        topic++;
                        Node* switched_channel = find_channel(&channel_list, topic);
                        if (!switched_channel) {
                            printw("You are not subscribed to that topic.\n");
                            continue;
                        }
                        // Switch current channel to the newly switched channel
                        current_chat = switched_channel;
                        clear();
                        for (int i = 0; i < current_chat->write; i++) {
                            char msg[BUFSIZ];
                            strcpy(msg, current_chat->buffer_history[i]);
            	            char* topic = strchr(msg, ' ');	
            	            *(topic++) = '\0';
            	            char* body = strchr(topic, ' ');
            	            *(body++) = '\0';
            	            if (!strcmp(msg, name)) {
            	                attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(2));
            	                printw("\r%s", name);
					            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(2));
      		                } else {
      		                    attron(COLOR_PAIR(1));
					            printw("\r%s on ", msg);
					            attron(A_UNDERLINE | A_BOLD);
					            printw("%s", topic);
					            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(1));
					        }
      		                printw("> %-80s\n", body);
                        }
                        refresh();
                    } 
				    else {
				        // Im submitting a message
					    mq_publish(mq, current_chat->topic, input_buffer);
      		            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(2) );
                        printw("\r%s>", name);
      		            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(2) );
                        printw(" %-80s\n", input_buffer);
                        refresh();
                        if (current_chat->write >= 100) {
                            printw("cannot allocate any more messages to buffer\n");
                            continue;
                        }
					    char temp_buf[input_index + 1];
					    strcpy(temp_buf, input_buffer);
					    char msg[BUFSIZ];
					    sprintf(msg, "%s %s %s", name, current_chat->topic, temp_buf);
					    char* dyn_msg = strdup(msg);
					    if (!dyn_msg) {
					        printw("could not allocate message\n");	
					        continue;
					    }
					    current_chat->buffer_history[current_chat->write++] = dyn_msg; 
					    refresh();
				    }
    			input_index = 0;
    			input_buffer[0] = 0;
				} else if (input_char == BACKSPACE && input_index) {	// Backspace
					input_buffer[--input_index] = 0;
				} else if (!iscntrl(input_char) && input_index < BUFSIZ) {
					input_buffer[input_index++] = input_char;
					input_buffer[input_index]   = 0;
				}
				printw("\r%-80s", "");			// Erase line (hack!)
				printw("\r> %s", input_buffer);		// Write
	            refresh();
    		} 
    		// Someone else sent a message, display it
    		else if (events[i].data.fd == mq->p[0]) {
    			// Read the dummy message
    			read(mq->p[0], inbuf, 17);
    			// Pop message from incoming
    			char* name = mq_retrieve(mq);
				if (name) {
            	    char* topic = strchr(name, ' ');	
            	    *(topic++) = '\0';
            	    char* body = strchr(topic, ' ');
            	    *(body++) = '\0';
            	    // We sent this message so disregard it
            	    if (!strcmp(name, mq->name)) {
                	    free(name);
                	    continue;
            	    }
            	    // If the message is to our current topic then just print it (and store in buffer)
                    if (!strcmp(topic, current_chat->topic)) {
      		            attron(COLOR_PAIR(1));
					    printw("\r%s on ", name);
					    attron(A_UNDERLINE | A_BOLD);
					    printw("%s", topic);
					    attroff(A_UNDERLINE | A_BOLD);
					    attroff(COLOR_PAIR(1));
      		            printw("> %-80s\n", body);
                    } 
                    Node* channel = find_channel(&channel_list, topic);
                    if (!channel) {
                        printw("Could not find proper channel\n");
                        continue;
                    }
                    if (channel->write >= MAX_MESSAGES) {
                        printf("Reached maximum buffer size\n");
                        free(name);
                        continue;
                    }
					char msg[BUFSIZ] = { 0 };
					sprintf(msg, "%s %s %s", name, topic, body);
					char* dyn_msg = strdup(msg);
					if (!dyn_msg) {
					    printw("could not allocate message\n");	
					    free(name);
					    continue;
					}
					channel->buffer_history[channel->write++] = dyn_msg; 
				    refresh();
				    free(name);
				}
    		}
    	}
		refresh();
	}
    endwin();
	mq_delete(mq);
	free_buffers(channel_list.head);
	return 0;
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





