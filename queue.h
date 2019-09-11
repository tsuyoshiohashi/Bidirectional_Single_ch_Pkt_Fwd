/*
* queue.h
*/

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdint.h>

#define BUF_SIZE 2048
#define QUEUE_SIZE 7

#define queue_next(n) (((n)+1)%QUEUE_SIZE)

typedef struct {
     int bytes;
     uint8_t buf[BUF_SIZE];
} buf_t;

typedef struct {
     int head;
     int tail;
     buf_t buf[QUEUE_SIZE];
} que_t;

int qty_que(que_t );
void init_que(que_t *);
int isempty_que(que_t );
int en_que(que_t *, buf_t );
int de_que(que_t *, buf_t *);

#endif  //  _QUEUE_H_