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

#include <termios.h>
#include <unistd.h>

/* Constants */

#define BACKSPACE   127

void toggle_raw_mode() {
    static struct termios OriginalTermios = {0};
    static bool enabled = false;

    if (enabled) {
    	tcsetattr(STDIN_FILENO, TCSAFLUSH, &OriginalTermios);
    } else {
	tcgetattr(STDIN_FILENO, &OriginalTermios);

	atexit(toggle_raw_mode);

	struct termios raw = OriginalTermios;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    	enabled = true;
    }
}

/* Threads */

void *incoming_thread(void *arg) {
	MessageQueue *mq = (MessageQueue *)arg;
    while (!mq_shutdown(mq)) {
    	char* name = mq_retrieve(mq);
		if (name) {
            char* body = strchr(name, ' ');	
            *(body++) = '\0';
            if (!strcmp(name, mq->name)) {
                free(name);
                continue;
            }
			printf("\r%s> %-80s\n", name, body);
			free(name);
		}
	}
	return NULL;
}

int help(){
	printf("\nEnter /subscribe:{topic} to switch topics\n");
	printf("Enter /topic to see current topic\n");
	printf("Enter /help for help\n");
	printf("Enter /exit or /quit to exit\n");
	return 0;
}

/* Main Execution */

int main(int argc, char *argv[]) {
    toggle_raw_mode();

	//create MessageQueue
	char *topic = "General";
	char *name = getenv("USER");
    char *host = "localhost";
    char *port = "9622";
	if(!name) name = "User";
	if (argc > 1) { 
		if(strcmp(argv[1], "--help") == 0){
			printf("Usage: ./chat [username] [topic] [host] [port]\n");
			return 0;
		}else name = argv[1]; 
	}
	if (argc > 2) { topic = argv[2]; }
    if (argc > 3) { host = argv[3]; }
	if (argc > 4) { port = argv[4]; }
	if (!name) name = "USER";
	MessageQueue *mq = mq_create(name, host, port);
	if(!mq){
		printf("Unable to create MessageQueue\n");
		return 0;
	}
	mq_start(mq);
	mq_subscribe(mq, topic);
	
    /* Background Thread */
    Thread incoming;
    thread_create(&incoming, NULL, incoming_thread, mq);

    /* Foreground Thread */
    char   input_buffer[BUFSIZ] = "";	
    size_t input_index = 0;

    while (!mq_shutdown(mq)) {
		char input_char = 0;
    	read(STDIN_FILENO, &input_char, 1);

    	if (input_char == '\n') {				// Process commands
    	    if (strcmp(input_buffer, "/exit") == 0 || strcmp(input_buffer, "/quit") == 0) {
				mq_stop(mq);
				break;
			} else if (strcmp(input_buffer, "/help") == 0) {
				help();
			} else if (strcmp(input_buffer, "/topic") == 0){
				printf("\n%s\n", topic);
			} else if (strncmp(input_buffer, "/subscribe", 10) == 0 && strlen(input_buffer) > 10){
				mq_unsubscribe(mq, topic);
				topic = strchr(input_buffer, ':');
				topic++;
				mq_subscribe(mq, topic);
	
			} else {
				mq_publish(mq, topic, input_buffer);
				printf("\r%s> %-80s\n", name, input_buffer);
			}
    	    input_index = 0;
    	    input_buffer[0] = 0;
		} else if (input_char == BACKSPACE && input_index) {	// Backspace
				input_buffer[--input_index] = 0;
		} else if (!iscntrl(input_char) && input_index < BUFSIZ) {
			input_buffer[input_index++] = input_char;
			input_buffer[input_index]   = 0;
		}

		printf("\r%-80s", "");			// Erase line (hack!)
		printf("\r%s> %s", name, input_buffer);		// Write
		fflush(stdout);
	}

	printf("\n");
	void *status;
	thread_join(incoming, &status);
	mq_delete(mq);
	return 0;
}
