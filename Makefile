# bidirection_single_chan_pkt_fwd
#
PROG := bidirection
SRCS := bidirection.c radio.c network.c base64.c timer.c queue.c jsonjob.c parson.c
HDRS := bidirection.h radio.h network.h base64.h timer.h queue.h jsonjob.h parson.h config.h
OBJS := $(SRCS:%.c=%.o)

CC=gcc
CFLAGS= -Wall
LIBS=-lwiringPi

.PHONY: all
all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LIBS)  $^ -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm *.o $(PROG)
	