/*
* network.h
*/

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "queue.h"

//void initport();
void initportd();
void initports();
void init_buff_up();
void init_buf_req();
void init_buff_req();
void init_status_report();
void sendudp(uint8_t *, int );
void sendstat(uint8_t ,uint8_t );
void send2serv_rxpkt(uint8_t ,uint8_t , int );
void send2serv_que(uint8_t, uint8_t, buf_t* );
void send2serv(uint8_t , uint8_t , uint8_t );
void send2serv_pkt(uint8_t , uint8_t , uint8_t );
int recvfserv(void);
void gen_token(void);

#endif // _NETWORK_H_