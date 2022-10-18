/* queue.c: Concurrent Queue of Requests */

#include "mq/queue.h"
#include <stdio.h>
/**
 * Create queue structure.
 * @return  Newly allocated queue structure.
 */
Queue * queue_create() {
    Queue *q = calloc(1, sizeof(Queue));
    if (q) {
        sem_init(&q->lock, 0, 1);
        sem_init(&q->produced, 0, 0);
    }
    return q;
}

/**
 * Delete queue structure.
 * @param   q       Queue structure.
 */
void queue_delete(Queue *q) {
    // Recursively free all nodes in queue
    queue_delete_helper(q->head);  
    q->size = 0;
    free(q);
}

/**
 * Recursively free nodes in queue structure
 * @param  r       Request structure
 */
void queue_delete_helper(Request* r) {
    if (!r) return;
    queue_delete_helper(r->next);
    request_delete(r);
}

/**
 * Push request to the back of queue.
 * @param   q       Queue structure.
 * @param   r       Request structure.
 */
void queue_push(Queue *q, Request *r) {
    sem_wait(&q->lock);
    // If there is nothing in the queue yet then set tail and head
    if (!q->head) {
        q->head = r;
        q->tail = r;
        q->size = 1;
    }
    else {
        // Set the current tails next pointer to r
        q->tail->next = r;
        q->tail = r;
        q->size++;
    }
    sem_post(&q->lock);
    sem_post(&q->produced);
}

/**
 * Pop request to the front of queue (block until there is something to return).
 * @param   q       Queue structure.
 * @return  Request structure.
 */
Request * queue_pop(Queue *q) {
    sem_wait(&q->produced);
    sem_wait(&q->lock);
    Request *curr_request = q->head; 
        q->head = q->head->next;
        q->size--;
    sem_post(&q->lock);
    return curr_request;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
