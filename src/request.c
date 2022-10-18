/* request.c: Request structure */

#include "mq/request.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * Create Request structure.
 * @param   method      Request method string.
 * @param   uri         Request uri string.
 * @param   body        Request body string.
 * @return  Newly allocated Request structure.
 */
Request * request_create(const char *method, const char *uri, const char *body) {
    Request *request;
    // Allocate the request on the heap
    if (!(request = malloc(sizeof(Request)))) fprintf(stderr, "malloc: %s\n", strerror(errno));
    // Allocate the request's method and handle errors
    if (method && !(request->method = strdup(method))) goto FAILURE;
    if (uri && !(request->uri = strdup(uri))) goto FAILURE;
    request->body = NULL;
    if (body && !(request->body = strdup(body))) goto FAILURE;

    request->next = NULL;
    return request;

FAILURE:
    fprintf(stderr, "error in creating request: %s\n", strerror(errno));
    free(request);
    return NULL;
}

/**
 * Delete Request structure.
 * @param   r           Request structure.
 */
void request_delete(Request *r) {
    if (r->method) free(r->method);
    if (r->uri) free(r->uri);
    if (r->body) free(r->body);
    free(r);
}

/**
 * Write HTTP Request to stream:
 *  
 *  $METHOD $URI HTTP/1.0\r\n
 *  Content-Length: Length($BODY)\r\n
 *  \r\n
 *  $BODY
 *      
 * @param   r           Request structure.
 * @param   fs          Socket file stream.
 */
void request_write(Request *r, FILE *fs) {
    // printf("ok writing the request of, %s, %s, %s\n", r->method, r->uri, r->body);
    fprintf(fs, "%s %s HTTP/1.0\r\n", r->method, r->uri);
    if (r->body) fprintf(fs, "Content-Length: %zu\r\n\r\n%s", strlen(r->body), r->body);
    else fprintf(fs, "\r\n");
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */ 
