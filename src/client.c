/* client.c: Message Queue Client */

#include "mq/client.h"
#include "mq/logging.h"
#include "mq/socket.h"
#include "mq/string.h"
#include <unistd.h>

/* Internal Constants */

#define SENTINEL "SHUTDOWN"
sem_t Lock;
Thread pusher_thread, puller_thread;
/* Internal Prototypes */

void * mq_pusher(void *);
void * mq_puller(void *);

/* External Functions */

/**
 * Create Message Queue withs specified name, host, and port.
 * @param   name        Name of client's queue.
 * @param   host        Address of server.
 * @param   port        Port of server.
 * @return  Newly allocated Message Queue structure.
 */
MessageQueue * mq_create(const char *name, const char *host, const char *port) {
    MessageQueue* mq;
    Queue* outgoing, *incoming;
    Request* sentinel;
    // Allocate the MQ 
    if (!(mq = malloc(sizeof(MessageQueue)))) return NULL;
    strcpy(mq->name, name);
    strcpy(mq->host, host);
    strcpy(mq->port, port);
    // Allocate the pusher and puller queues
    if (!(outgoing = queue_create())) return NULL;
    if (!(incoming = queue_create())) return NULL;
    mq->outgoing = outgoing;
    mq->incoming = incoming;
    mq->shutdown = false; 
    sem_init(&Lock, 0, 1);

    // Subscribe to a shutdown topic for the user
    mq_subscribe(mq, SENTINEL);
    char uri[BUFSIZ];
    sprintf(uri, "/topic/%s", SENTINEL);
    if (!(sentinel = request_create("PUT", uri, SENTINEL))) return NULL;
    mq->sentinel = sentinel;

    // Create the pipe
    if (pipe(mq->p) < 0) return NULL;
    
    return mq;
}

/**
 * Delete Message Queue structure (and internal resources).
 * @param   mq      Message Queue structure.
 */
void mq_delete(MessageQueue *mq) {
    queue_delete(mq->outgoing);
    queue_delete(mq->incoming);
    free(mq); 
}

/**
 * Publish one message to topic (by placing new Request in outgoing queue).
 * @param   mq      Message Queue structure.
 * @param   topic   Topic to publish to.
 * @param   body    Message body to publish.
 */
void mq_publish(MessageQueue *mq, const char *topic, const char *body) {
    Request* new_request;
    char dest[] = "/topic/";
    char new_body[BUFSIZ];
    sprintf(new_body, "%s %s %s", mq->name, topic, body);
    if ((new_request = request_create("PUT", strcat(dest, topic), new_body))) {
        queue_push(mq->outgoing, new_request);
    }
}

/**
 * Retrieve one message (by taking Request from incoming queue).
 * @param   mq      Message Queue structure.
 * @return  Newly allocated message body (must be freed).
 */
char * mq_retrieve(MessageQueue *mq) {
    Request* new_request;
    char* return_message = NULL;
    if ((new_request = queue_pop(mq->incoming))) {
        if (!streq(new_request->body, SENTINEL)){
            return_message = strdup(new_request->body);
            request_delete(new_request);
            return return_message;
        }
        // If it is the sentinel then just free it and don't send it to app
        request_delete(new_request);
    } 
    return NULL;
}

/**
 * Subscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to subscribe to.
 **/
void mq_subscribe(MessageQueue *mq, const char *topic) {
    // create a new string combining "/subscription/" and topic
    Request* new_request;
    char uri[BUFSIZ];
    sprintf(uri, "/subscription/%s/%s", mq->name, topic);
    if ((new_request = request_create("PUT", uri, NULL))) queue_push(mq->outgoing, new_request);

}

/**
 * Unubscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to unsubscribe from.
 **/
void mq_unsubscribe(MessageQueue *mq, const char *topic) {
    char uri[BUFSIZ];
    Request* new_request;
    sprintf(uri, "/subscription/%s/%s", mq->name, topic);
    if ((new_request = request_create("DELETE", uri, NULL))) queue_push(mq->outgoing, new_request);
}

/**
 * Start running the background threads:
 *  1. First thread should continuously send requests from outgoing queue.
 *  2. Second thread should continuously receive reqeusts to incoming queue.
 * @param   mq      Message Queue structure.
 */
void mq_start(MessageQueue *mq) {
    thread_create(&pusher_thread, NULL, mq_pusher, mq);
    thread_create(&puller_thread, NULL, mq_puller, mq); 
}

/**
 * Stop the message queue client by setting shutdown attribute and sending
 * sentinel messages
 * @param   mq      Message Queue structure.
 */
void mq_stop(MessageQueue *mq) {
    sem_wait(&Lock);
    mq->shutdown = true;
    sem_post(&Lock);
    queue_push(mq->outgoing, mq->sentinel);
    // Join the threads
    thread_join(pusher_thread, NULL);
    thread_join(puller_thread, NULL);
}

/**
 * Returns whether or not the message queue should be shutdown.
 * @param   mq      Message Queue structure.
 */
bool mq_shutdown(MessageQueue *mq) {
    sem_wait(&Lock);
    bool return_val = false;
    if (mq->shutdown) return_val = true;
    sem_post(&Lock);
    return return_val;
}

/* Internal Functions */

/**
 * Pusher thread takes messages from outgoing queue and sends them to server.
 **/
void * mq_pusher(void *arg) {
    MessageQueue* mq = (MessageQueue*)arg;
    FILE* server;
    char buffer[BUFSIZ];
    Request* message; 
    while (!mq_shutdown(mq)) {
        if (!(server = socket_connect(mq->host, mq->port))) continue;
        message = queue_pop(mq->outgoing);
        request_write(message, server);
        // Read response from server (can disregard for pusher)
        while (fgets(buffer, BUFSIZ, server));
        request_delete(message);
        fclose(server);
    }
    return NULL;
}

/**
 * Puller thread requests new messages from server and then puts them in
 * incoming queue.
 **/
void * mq_puller(void *arg) {
    MessageQueue* mq = (MessageQueue*)arg;
    FILE* server;
    Request* new_request;
    char uri[BUFSIZ];
    sprintf(uri, "/queue/%s", mq->name);
    char buffer[BUFSIZ];
    size_t content_length;
    char* body;
    
    while (!mq_shutdown(mq)) {
        if (!(server = socket_connect(mq->host, mq->port))) continue;
        if (!(new_request = request_create("GET", uri, NULL))) continue;
        // If we are able to connect to server and create request, then send it
        request_write(new_request, server);
        if (fgets(buffer, BUFSIZ, server) && strstr(buffer, "200 OK")) {
            while (fgets(buffer, BUFSIZ, server) && !streq(buffer, "\r\n")) {
                sscanf(buffer, "Content-Length: %ld", &content_length);
            }
            if (!(body = calloc(content_length + 1, sizeof(char)))) request_delete(new_request);
            else {
                new_request->body = body;
                fread(new_request->body, 1, content_length, server);
                queue_push(mq->incoming, new_request);
                write(mq->p[1], "incoming message", 17);
            } 
        // If we don't get a 200 status code, then delete the request
        } else request_delete(new_request);
        fclose(server);
    }
    return NULL;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
