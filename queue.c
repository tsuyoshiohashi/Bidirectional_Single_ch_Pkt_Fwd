/*
* queue.c
*/
#include <stdio.h>
#include <stdint.h>

#include "queue.h"

buf_t tbuf;    // temporary buffer for in/out
que_t rxq; // rx radio pkt and send to server
que_t txq; // recv from server and tx radio pkt

int qty_que(que_t que){
     return((que.tail - que.head + QUEUE_SIZE )%QUEUE_SIZE);
}

void init_que(que_t *que){
     que->head = 0;
     que->tail = 0;
}

int isempty_que(que_t que){
     return(que.head == que.tail);
}

int en_que(que_t *que, buf_t data){
     if(queue_next(que->tail) == que->head)
         return(-1);    // queue is full
     que->buf[que->tail] = data;
     que->tail = queue_next(que->tail);
     return(0);
}

int de_que(que_t *que, buf_t *data){
     if(que->head == que->tail)
         return(-1);    // queue is empty
     *data = que->buf[que->head];
     que->head = queue_next(que->head);
     return(0);
}
// end of queue.c
