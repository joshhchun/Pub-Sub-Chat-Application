/* request.h: HTTP Request structure */

#ifndef REQUEST_H
#define REQUEST_H

#include <stdio.h>

/* Structures */

typedef struct Request Request;
struct Request {
    char *	method;
    char *	uri;
    char *	body;
    
    Request *	next;
};

/* Functions */

Request *   request_create(const char *method, const char *uri, const char *body);
void	    request_delete(Request *r);
void        request_write(Request *r, FILE *fs);

#endif
