/*  
* network.c
* Copyright (c) 2019 Tsuyoshi Ohashi
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <sys/ioctl.h> 
#include <net/if.h>  
#include <arpa/inet.h>

#include "config.h"
#include "bidirection.h"
#include "network.h"

struct ifreq ifr;
struct sockaddr_in d_addr;
socklen_t slen = sizeof(d_addr);
int sock_down;

/* Informal status fields */
static char platform[24]    = PLATFORM;
static char email[40]       = EMAIL;              /* used for contact email */
static char description[64] = DESCRIPTION;       /* used for free form description */

// Location is in config
float lat=LATITUDE;
float lon=LONGITUDE;
int   alt=ALTITUDE;

extern uint32_t cp_nb_rx_rcv;
extern uint32_t cp_nb_rx_ok;
extern uint32_t cp_nb_rx_bad;
extern uint32_t cp_nb_rx_nocrc;
extern uint32_t cp_up_pkt_fwd;
extern char stat_timestamp[24];
extern time_t t;
extern uint8_t token_h;
extern uint8_t token_l;

//static char status_report[STATUS_SIZE];
uint8_t status_report[STATUS_SIZE];
uint8_t buf_down[DOWNSTRM_SIZE];
uint8_t buf_req[HEADER_SND_LEN];
uint8_t buff_up[SND_BUFF_SIZE];
uint8_t buff_req[1024];
int buff_index = 0;

void initportd(){
    
    struct timeval pull_timeout;
    int val = 1;
    
    /* try to open socket for downstream traffic */
    if( (sock_down=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
    //if( (sock_down=socketa() ) == -1){
       die("Error: socket sock_down.");
    }

    d_addr.sin_family = AF_INET;
    d_addr.sin_port = htons(PORT_DOWN);
    d_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    // get mac address for Gateway ID
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
    //ioctl(sock_down, SIOCGIFHWADDR, &ifr);
    int io;
    io=ioctl(sock_down, SIOCGIFHWADDR, &ifr);
    if(io == -1){ // error, try wlan0
        strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);
        ioctl(sock_down, SIOCGIFHWADDR, &ifr);
    }
    // NON-Blocking
    ioctl(sock_down, FIONBIO, &val);

    // ソケットに名前を付ける
    if( bind(sock_down, (struct sockaddr *)&d_addr, sizeof(d_addr)) == -1){
        die("Error: socket bind sock_down.");
    }
    /* set downstream socket RX timeout */
    pull_timeout.tv_sec = 1;
    pull_timeout.tv_usec = 0;
    if ( setsockopt(sock_down, SOL_SOCKET, SO_RCVTIMEO, (void *)&pull_timeout, sizeof( pull_timeout)) != 0) {
        die("Error: setsockopt sock_down." );
        exit(EXIT_FAILURE);
    }
}
///
void init_buff_up(){
    /* pre-fill the data buffer with fixed fields */
    buff_up[0] = PROTOCOL_VERSION;
    buff_up[3] = PKT_PUSH_DATA;
    buff_up[4] = (unsigned char)ifr.ifr_hwaddr.sa_data[0];
    buff_up[5] = (unsigned char)ifr.ifr_hwaddr.sa_data[1];
    buff_up[6] = (unsigned char)ifr.ifr_hwaddr.sa_data[2];
    buff_up[7] = 0xFF;
    buff_up[8] = 0xFF;
    buff_up[9] = (unsigned char)ifr.ifr_hwaddr.sa_data[3];
    buff_up[10] = (unsigned char)ifr.ifr_hwaddr.sa_data[4];
    buff_up[11] = (unsigned char)ifr.ifr_hwaddr.sa_data[5];
}

void init_buf_req(){
    buf_req[0] = PROTOCOL_VERSION;
    buf_req[3] = PKT_PULL_DATA;
    buf_req[4] = (unsigned char)ifr.ifr_hwaddr.sa_data[0];
    buf_req[5] = (unsigned char)ifr.ifr_hwaddr.sa_data[1];
    buf_req[6] = (unsigned char)ifr.ifr_hwaddr.sa_data[2];
    buf_req[7] = 0xFF;
    buf_req[8] = 0xFF;
    buf_req[9] = (unsigned char)ifr.ifr_hwaddr.sa_data[3];
    buf_req[10] = (unsigned char)ifr.ifr_hwaddr.sa_data[4];
    buf_req[11] = (unsigned char)ifr.ifr_hwaddr.sa_data[5];
}

void init_buff_req(){
    buff_req[0] = PROTOCOL_VERSION;
    buff_req[3] = PKT_PULL_DATA;
    buff_req[4] = (unsigned char)ifr.ifr_hwaddr.sa_data[0];
    buff_req[5] = (unsigned char)ifr.ifr_hwaddr.sa_data[1];
    buff_req[6] = (unsigned char)ifr.ifr_hwaddr.sa_data[2];
    buff_req[7] = 0xFF;
    buff_req[8] = 0xFF;
    buff_req[9] = (unsigned char)ifr.ifr_hwaddr.sa_data[3];
    buff_req[10] = (unsigned char)ifr.ifr_hwaddr.sa_data[4];
    buff_req[11] = (unsigned char)ifr.ifr_hwaddr.sa_data[5];
}

void init_status_report(){
    status_report[0] = PROTOCOL_VERSION;
    status_report[3] = PKT_PUSH_DATA;
    status_report[4] = (unsigned char)ifr.ifr_hwaddr.sa_data[0];
    status_report[5] = (unsigned char)ifr.ifr_hwaddr.sa_data[1];
    status_report[6] = (unsigned char)ifr.ifr_hwaddr.sa_data[2];
    status_report[7] = 0xFF;
    status_report[8] = 0xFF;
    status_report[9] = (unsigned char)ifr.ifr_hwaddr.sa_data[3];
    status_report[10] = (unsigned char)ifr.ifr_hwaddr.sa_data[4];
    status_report[11] = (unsigned char)ifr.ifr_hwaddr.sa_data[5];
}
///
void initports(){
    d_addr.sin_port = htons(PORT);
    inet_aton(SERVER1 , &d_addr.sin_addr); 
}

//void sendudp(char *msg, int length) {
void sendudp(uint8_t *msg, int length) {
   //send the update
/*
    inet_aton(SERVER1 , &si_other.sin_addr);
    printf("\tSend SERVER1 IP:%s, Port:%d \n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port) );
    if (sendto(sl, (char *)msg, length, 0 , (struct sockaddr *) &si_other, slen)==-1){
        die("SERVER1 sendto()");
    }
    */
    inet_aton(SERVER1 , &d_addr.sin_addr);
    printf("\tSend SERVER IP:%s, Port:%d, ", inet_ntoa(d_addr.sin_addr), ntohs(d_addr.sin_port) );
    //if (sendto(sl, (char *)msg, length, 0 , (struct sockaddr *) &si_other, slen)==-1){
    if (sendto(sock_down, (char *)msg, length, 0 , (struct sockaddr *) &d_addr, slen)==-1){
        die("sendudp  sendto()");
    }
}

void sendstat(uint8_t token_h,uint8_t token_l) {

    int stat_index = HEADER_SND_LEN; /* 12-byte header */
    int j;

    status_report[1] = token_h;
    status_report[2] = token_l;

    /* get timestamp for statistics */
    t = time(NULL);
    strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));

    j = snprintf((char *)(status_report + stat_index), STATUS_SIZE-stat_index, "{\"stat\":{\"time\":\"%s\",\"lati\":%.5f,\"long\":%.5f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}", stat_timestamp, lat, lon, (int)alt, cp_nb_rx_rcv, cp_nb_rx_ok, cp_up_pkt_fwd, (float)0, 0, 0,platform,email,description);
    stat_index += j;
    status_report[stat_index] = 0; /* add string terminator, for safety */

    //for(i=4;i<stat_index;i++){
    //    printf("%02x:",status_report[i]);
    //}
    //printf("\n");

    //printf("%s: sendstat\n", stat_timestamp);
    //send the update
    sendudp(status_report, stat_index);
    printf("send token: %02x:%02x: TYPE:%d, Length:%d\n", status_report[1], status_report[2], status_report[3], stat_index);  
    printf("stat update: %s\n", (char *)(status_report+HEADER_SND_LEN)); /* DEBUG: display JSON stat */
}

void send2serv_rxpkt(uint8_t token_h,uint8_t token_l, int tx_len){

    buff_up[1] = token_h;
    buff_up[2] = token_l;
    buff_up[3] = PKT_PUSH_DATA;
    sendudp(buff_up, tx_len);
    printf("send token: %02x:%02x: TYPE:%d, Length:%d\n", buff_up[1], buff_up[2], buff_up[3], tx_len);
    printf("srpk update: %s\n", (char *)(buff_up+12)); /* DEBUG: display JSON stat */
}

void send2serv_que(uint8_t token_h,uint8_t token_l, buf_t* data){

    data->buf[0] = PROTOCOL_VERSION;;
    data->buf[1] = token_h;
    data->buf[2] = token_l;
    data->buf[3] = PKT_PUSH_DATA;

    sendudp(data->buf, data->bytes);
    printf("send token: %02x:%02x: TYPE:%d, QLength:%d\n", data->buf[1], data->buf[2], data->buf[3], data->bytes);
    printf("srpk update: %s\n", (char *)(data->buf+12)); /* DEBUG: display JSON stat */
}

void send2serv(uint8_t token_h, uint8_t token_l, uint8_t pkt_type){

    buf_req[1] = token_h;
    buf_req[2] = token_l;
    buf_req[3] = pkt_type;

    //d_addr.sin_port = htons(PORT);
    //inet_aton(SERVER1 , &d_addr.sin_addr); 
    printf("\tSEND IP:%s, Port:%d, ", inet_ntoa(d_addr.sin_addr), ntohs(d_addr.sin_port) );
    if (sendto(sock_down, (void*)buf_req, sizeof( buf_req), 0 , (struct sockaddr *) &d_addr, slen)==-1){
        die("send2serv sendto()");
    }
    printf("send token: %02x:%02x: TYPE:%d\n", buf_req[1], buf_req[2], buf_req[3]);
}

void send2serv_pkt(uint8_t token_h, uint8_t token_l, uint8_t pkt_type){

    int rq_len = HEADER_SND_LEN;
    buff_req[1] = token_h;
    buff_req[2] = token_l;
    buff_req[3] = pkt_type;

    d_addr.sin_port = htons(PORT);
    inet_aton(SERVER1 , &d_addr.sin_addr); 
    printf("\tSEND IP:%s, Port:%d, ", inet_ntoa(d_addr.sin_addr), ntohs(d_addr.sin_port) );
    if (sendto(sock_down, (void*)buf_req, rq_len, 0 , (struct sockaddr *) &d_addr, slen)==-1){
        die("send2serv_pkt sendto()");
    }
    printf("send token: %02x:%02x: TYPE:%d\n", buff_req[1], buff_req[2], buff_req[3]);
}

int recvfserv(void){
    int m_len;

    memset(buf_down, 0, sizeof(buf_down));
	inet_aton(SERVER1 , &d_addr.sin_addr);
    //si_other.sin_port = htons(PORT_DOWN);
    d_addr.sin_port = htons(PORT_DOWN);
    m_len = recvfrom(sock_down, buf_down, sizeof( buf_down)-1, 0 , (struct sockaddr *) &d_addr, &slen);
    return(m_len);        
}
// end of network.c