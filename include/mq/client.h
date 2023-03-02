/* client.h: Message Queue client */

#ifndef CLIENT_H
#define CLIENT_H

#include "mq/queue.h"

#include <netdb.h>
#include <stdbool.h>

/* Structures */

typedef struct MessageQueue MessageQueue;
struct MessageQueue {
    char    name[NI_MAXHOST];	// Name of message queue
    char    host[NI_MAXHOST];	// Host of server
    char    port[NI_MAXSERV];	// Port of server
    Request* sentinel;

    Queue*  outgoing;		// Requests to be sent to server
    Queue*  incoming;		// Requests received from server
    bool    shutdown;		// Whether or not to shutdown
    int p[2];                // Pipe for communication main chat program
};

MessageQueue *	mq_create(const char *name, const char *host, const char *port);
void		mq_delete(MessageQueue *mq);

void		mq_publish(MessageQueue *mq, const char *topic, const char *body);
char *		mq_retrieve(MessageQueue *mq);

void		mq_subscribe(MessageQueue *mq, const char *topic);
void		mq_unsubscribe(MessageQueue *mq, const char *topic);

void		mq_start(MessageQueue *mq);
void		mq_stop(MessageQueue *mq);

bool		mq_shutdown(MessageQueue *mq);

#endif
