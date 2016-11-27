#include <assert.h>
#include <pthread.h>
#include "cs537.h"
#include "request.h"

//
// server.c: A very, very simple web server
//
// To run:
//  server <portn (above 2000)> <threads> <buffers>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

pthread_t **pthreads;

int numfull, filled_to, use, max_buffers;
int *buffer;

void getargs(int *port, int *threads, int *buffers, int argc, char *argv[])
{
    if (argc != 4) {
	    fprintf(stderr, "Usage: %s <portnum> <threads> <buffers>\n", argv[0]);
	    exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *buffers = atoi(argv[3]);
}

void *consumer(void *arg) {
  while(1) {
	  pthread_mutex_lock(&lock);
		while(numfull == 0) {
			pthread_cond_wait(&fill, &lock);
		}
    int requestfd = buffer[use];
    use = (use + 1) % max_buffers;

    numfull--;

    pthread_cond_signal(&empty);
    pthread_mutex_unlock(&lock);

    // Fails if inside the locks
    requestHandle(requestfd);
    Close(requestfd);
  }
}


int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, threads, buffers;
    struct sockaddr_in clientaddr;

    getargs(&port, &threads, &buffers, argc, argv);

    // Check validity of args
    if (port < 0 || threads <= 0 || buffers <= 0) {
      fprintf(stderr, "Value of <threads> and <buffers> have to be positive ");
      fprintf(stderr, "integers and <portnum> has to be non-negative\n");
      exit(1);
    }

    // Set up buffer
    max_buffers = buffers;
    numfull = use = filled_to = 0;
    buffer = malloc(buffers*sizeof(*buffer));

    // Create the threads
    int i, rc;
    pthreads = malloc(threads * sizeof(pthreads));
    for (i = 0; i < threads; i++) {
      pthreads[i] = malloc(sizeof(pthread_t));
      rc = pthread_create(pthreads[i], NULL, consumer, NULL);
      assert(rc == 0);
    }

    listenfd = Open_listenfd(port);
    while (1) {
	    clientlen = sizeof(clientaddr);
	    connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

	    //
    	// CS537: In general, don't handle the request in the main thread.
    	// Save the relevant info in a buffer and have one of the worker threads
    	// do the work. However, for SFF, you may have to do a little work
	    // here (e.g., a stat() on the filename) ...
	    //

      pthread_mutex_lock(&lock);
      while (numfull == max_buffers) {
        pthread_cond_wait(&empty, &lock);
      }

      buffer[filled_to] = connfd;
      filled_to = (filled_to + 1) % max_buffers;

      numfull++;
      pthread_cond_signal(&fill);
      pthread_mutex_unlock(&lock);
    }

}
