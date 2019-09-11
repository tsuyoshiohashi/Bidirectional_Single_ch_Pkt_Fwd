/*  bidirection.h
* Copyright (c) 2019 Tsuyoshi Ohashi
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#ifndef _BIDIRECTION_H_
#define _BIDIRECTION_H_

#define HEADER_SND_LEN 12
#define HEADER_RCV_LEN 4

#define PROTOCOL_VERSION  2
#define PKT_PUSH_DATA 0
#define PKT_PUSH_ACK  1
#define PKT_PULL_DATA 2
#define PKT_PULL_RESP 3
#define PKT_PULL_ACK  4
#define PKT_TX_ACK  5

#define SND_BUFF_SIZE  2048
#define STATUS_SIZE	  1024
#define DOWNSTRM_SIZE   1024

// typedef
typedef bool boolean;
typedef unsigned char byte;

// proto type declaration 
void die( const char *);
void show_tm(void);
void show_time(const char *);
void show_time2(const char *, int);
void show_timeh(const char *, int);

#endif // _BIDIRECTION_H_