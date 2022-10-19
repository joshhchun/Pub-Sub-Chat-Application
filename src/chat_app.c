#include "mq/thread.h"
#include "mq/client.h"
#include "mq/chat_app.h"

#include <ctype.h>
#include <ncurses.h>
#include <sys/epoll.h>
#include <unistd.h>

/* Main Execution */

int main(int argc, char *argv[]) {
    // ncurses init
    init_curses();

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
		fprintf(stderr, "could not create message queue\n");
		 exit(1);
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
        fprintf(stderr, "Error setting up epoll\n");
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
					} else if (!strcmp(input_buffer, "/channels")) {
					    print_channels(&channel_list);
					} else if (!strcmp(input_buffer, "/topic")){
					    attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(MAGENTA));
                        printw("Current topic: %s\n", current_chat->topic);
					    attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(MAGENTA));
				    } else if (!strcmp(input_buffer, "/menu")){
				        print_menu();
				    }  else if (strncmp(input_buffer, "/subscribe", 10) == 0 && strlen(input_buffer) > 10){
				        if (num_subs == MAX_SUBS) {
				            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
				            printw("Cannot subscribe, reached maximum number of subs\n");
				            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
				            continue;
				        } 
                        char* topic = strchr(input_buffer, ' ');
                        if (!topic){
                            printw("Correct usage: /subscribe [topic]\n");
                            continue;
                        }
				        topic++;
				        if (find_channel(&channel_list, topic)) {
				            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
				            printw("Already subscribed to that topic!\n");
				            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
				            continue;
				        }
				        attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(MAGENTA) );
				        printw("SUBSCRIBED TO TOPIC: %s\n", topic);
				        attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(MAGENTA));
				        mq_subscribe(mq, topic);
				        // Increment subs
				        num_subs++;
				        // Push topic into channels linked list
				        if (push_node(&channel_list, topic)) {
				            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
				            printw("Error creating channel, try again\n");
				            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
				            num_subs--;
				            mq_unsubscribe(mq, topic);
				        }
                    } else if (!strncmp(input_buffer, "/unsubscribe", 12) && strlen(input_buffer) > 12){
                        char* topic = strchr(input_buffer, ' ');
                        if (!topic) {
				            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            printw("Correct usage: /unsubscribe [topic]\n");
				            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            continue;
                        }
                        topic++;
                        int deleted = delete_channel(&channel_list, topic);
                        if (!deleted) {
				            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            printw("You were never subscribed to this channel\n");
				            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            continue;
                        }
				        attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(MAGENTA) );
				        printw("UNSUBSCRIBED TO TOPIC: %s\n", topic);
				        attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(MAGENTA));
				        mq_unsubscribe(mq, topic);
                        num_subs--;
                    }
                    else if (!strncmp(input_buffer, "/switch", 7) && strlen(input_buffer) > 7){
                        char* topic = strchr(input_buffer, ' ');
                        if (!topic) {
				            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            printw("Correct usage: /switch [topic]\n");
				            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            continue;
                        }
				        topic++;
                        Node* switched_channel = find_channel(&channel_list, topic);
                        if (!switched_channel) {
				            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            printw("You are not subscribed to that topic.\n");
				            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED));
                            continue;
                        }
                        // Switch current channel to the newly switched channel
                        current_chat = switched_channel;
                        clear();
                        int start = (current_chat->write > MAX_MESSAGES) ? current_chat->write : 0; 
                        for (int index = start; index < (start + MAX_MESSAGES); index++) {
                            if (!current_chat->buffer_history[index % MAX_MESSAGES]) break;
                            char msg[BUFSIZ];
                            strcpy(msg, current_chat->buffer_history[index%MAX_MESSAGES]);
            	            char* topic = strchr(msg, ' ');	
            	            *(topic++) = '\0';
            	            char* body = strchr(topic, ' ');
            	            *(body++) = '\0';
            	            // We sent the message
            	            if (!strcmp(msg, mq->name)) {
            	                attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(BLUE));
            	                printw("\r%s", name);
					            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(BLUE));
      		                } else {
                                unsigned long color = hash(msg) % NUM_COLORS;
      		                    attron(COLOR_PAIR(color));
					            printw("\r%s on ", msg);
					            attron(A_UNDERLINE | A_BOLD);
					            printw("%s>", topic);
					            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(color));
					        }
                            // If the message mentions our name, highlight it
                            if (strstr(body, mq->name)) attron(COLOR_PAIR(MENTION) | A_BOLD);
      		                printw(" %-80s\n", body);
      		                attroff(COLOR_PAIR(MENTION) | A_BOLD);
                        }
                        refresh();
                    } 
				    else {
				        // Im submitting a message
					    mq_publish(mq, current_chat->topic, input_buffer);
      		            attron(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED) );
                        printw("\r%s>", name);
      		            attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(RED) );
                        printw(" %-80s\n", input_buffer);
                        refresh();
					    char temp_buf[input_index + 1];
					    strcpy(temp_buf, input_buffer);
					    save_message(current_chat, name, temp_buf);
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
                        int color = (int)hash(name) % NUM_COLORS;
      		            attron(COLOR_PAIR(color));
					    printw("\r%s on ", name);
					    attron(A_UNDERLINE | A_BOLD);
					    printw("%s>", topic);
					    attroff(A_UNDERLINE | A_BOLD | COLOR_PAIR(color));
                        // If the message mentions our name, highlight it
                        if (strstr(body, mq->name)) attron(COLOR_PAIR(MENTION) | A_BOLD);
      		            printw(" %-80s\n", body);
      		            attroff(COLOR_PAIR(MENTION) | A_BOLD);
                    } 
                    Node* channel = find_channel(&channel_list, topic);
                    if (!channel) {
                        printw("Could not find proper channel\n");
                        continue;
                    }
                    save_message(channel, name, body);
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
