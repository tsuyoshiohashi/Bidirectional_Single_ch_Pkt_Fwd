/*
 *  bidirection.c
* Copyright (c) 2019 Tsuyoshi Ohashi
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html

 */
/*******************************************************************************
 *
 * Copyright (c) 2015 Thomas Telkamp
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 *******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "config.h"
#include "bidirection.h"
#include "base64.h"
#include "radio.h"
#include "network.h"
#include "timer.h"
#include "queue.h"
#include "jsonjob.h"

/* data buffers */
extern char status_report[STATUS_SIZE]; /* status report as a JSON object */
extern uint8_t buf_down[DOWNSTRM_SIZE]; /* buffer to receive downstream packets */
extern uint8_t buf_req[HEADER_SND_LEN]; /* buffer to compose pull requests */
extern uint8_t buff_req[1024];
extern char buff_up[SND_BUFF_SIZE]; /* buffer to compose the upstream packet */
extern int buff_index;

extern struct ifreq ifr;
extern socklen_t slen;
extern struct sockaddr_in d_addr;
extern int sock_down;

extern enum sf_t sf;
extern uint32_t freq;

extern que_t rxq, txq;
extern buf_t tbuf;
extern rf_txpkt_s txpkt;

// valiables
uint32_t cp_nb_rx_rcv;
uint32_t cp_nb_rx_ok;
uint32_t cp_nb_rx_bad;
uint32_t cp_nb_rx_nocrc;
uint32_t cp_up_pkt_fwd;
//
char stat_timestamp[24];
time_t t;

uint8_t token_h = 0; /* random token for acknowledgement matching */
uint8_t token_l = 0;
/////////
/* display error message and exit */
void die(const char *s){
    perror(s);
    exit(1);
}
/* generate random token */
void gen_token(){
    token_h = (uint8_t)rand();
    token_l = (uint8_t)rand();
}
/* display time and event */
void show_tm(void){
    struct timespec tvToday; // for msec
    struct tm *ptm; // for date and time

    clock_gettime(CLOCK_REALTIME_COARSE, &tvToday);
    ptm = localtime(&tvToday.tv_sec);
    // time
    printf("%02d:%02d:%02d.",ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    // msec
    printf("%03d ", (uint16_t)(tvToday.tv_nsec / 1000000));
}
void show_time(const char *mess){
    show_tm();
    printf(": %s\n", mess);
    fflush(stdout);
}
void show_time2(const char *mess, int val){
    show_tm();
    printf(": %s %d\n", mess, val);
    fflush(stdout);
}
void show_timeh(const char *mess, int hex_val){
    show_tm();
    printf(": %s %#x\n", mess, hex_val);
    fflush(stdout);
}
/* 
* bidirectional single channel packet forwarder
*/
int main(void){

    //// Setup ////
    const char date[] = __DATE__ ;
    const char time[] = __TIME__ ;
    int i;      /* loop */
    int rxb_len=0;
    int msg_len;
    
    int keepalive_time = DEFAULT_KEEPALIVE; /* send a PULL_DATA request every X seconds, negative = disabled */
    int stat_times = DEFAULT_STAT;
    int rx1_time = DEFAULT_RX1;
    int rx1_time_n = 0; //50000000; //1000000000
    int rx2_time = DEFAULT_RX2;
    int tfd_keepalive;
    int tfd_rx1;
    int tfd_rx2;

    uint64_t exp;
    struct itimerspec katime;
    struct itimerspec rx1time;
    struct itimerspec rx2time;
    ssize_t rtn;    // read timer status 
    int pull_pkt_cnt = stat_times;
    boolean rf_free = FALSE;

    init_que(&rxq);
    init_que(&txq);

    //// Setup Network ////
    //initport();
    initportd();
    initports();
    /* pre-fill the data buffer with fixed fields */
    init_buf_req();
    init_status_report();
    init_buff_up();

    //// Setup LoRa Hardware ////
    wiringPiSetup () ;
    initpinmode();
    SetupwiringPiSPI();
    SetupLoRa();

    //// Setup Timers ////
    tfd_keepalive = init_timerfd(keepalive_time, 0, keepalive_time, &katime);
    tfd_rx1 = init_timerfd(rx1_time, rx1_time_n, 0, &rx1time);
    tfd_rx2 = init_timerfd(rx2_time, 0, 0, &rx2time);
    //// Start TimerKeepAlive
    timerfd_settime( tfd_keepalive, 0, &katime, NULL );

    /* display result */
    printf("Build Date: %s,%s\n", date,time);
    printf("Gateway ID: %.2x:%.2x:%.2x:ff:ff:%.2x:%.2x:%.2x\n",
           (unsigned char)ifr.ifr_hwaddr.sa_data[0],
           (unsigned char)ifr.ifr_hwaddr.sa_data[1],
           (unsigned char)ifr.ifr_hwaddr.sa_data[2],
           (unsigned char)ifr.ifr_hwaddr.sa_data[3],
           (unsigned char)ifr.ifr_hwaddr.sa_data[4],
           (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
    printf("Listening at SF%i on %.6lf Mhz.\n", sf,(double)freq/1000000);
    printf("Send pull pkt every %d seconds.\n", keepalive_time);
    printf("Send stat pkt every %d packets.\n", stat_times);
    printf("Server address & port are %s & %d.\n", inet_ntoa(d_addr.sin_addr), ntohs(d_addr.sin_port));
    printf("------------------\n");

    //// Watch event and do job ////
    while(1){
        //// check radio packet ////
        //// check RxCont mode ////
        if( readRegister(REG_OPMODE) == SX72_MODE_RX_CONTINUOS){
            rxb_len = rf_receivepacket();
            if(rxb_len > 1 ){
                show_time("rx radio packet");
                printf("rxpk update: %s\n", (char *)(buff_up + 12));
                tbuf.bytes = rxb_len;
                memcpy(tbuf.buf, buff_up, rxb_len);
                en_que(&rxq, tbuf);
                if(isempty_que(txq) == 0){      // data to be transmitted
                    show_time("start RX1 timer");
                    timerfd_settime( tfd_rx1, 0, &rx1time, NULL );
                }
            }else if (rxb_len == 1){
                show_time("rx PROPRIETARY radio packet, Tx if data.");
                if(isempty_que(txq) == 0){      // data to be transmitted
                    show_time("Data exists,start RX1 timer");
                    timerfd_settime( tfd_rx1, 0, &rx1time, NULL );
                }
            }
        }   
        //// check timer RX1 ////
        rtn = read( tfd_rx1, &exp, sizeof(uint64_t));
        if ( rtn < 0){
            if( errno != EINTR && errno != EAGAIN){
                die("tfd_rx1");
            }
        }else{
            show_time("RX1 timer expired");
            rf_free = is_channel_free();
            if(rf_free){
                // transmit rf_pkt
                de_que(&txq, &tbuf);
                if(sf != DR_RX1){
                    sf = DR_RX1;
                    set_DataRate(sf);
                }
                rf_transmitpacket(&tbuf);
                show_time( "Transmit rf pkt in RX1");
            }else{
                // start RX2
                timerfd_settime( tfd_rx2, 0, &rx2time, NULL );
                show_time("Ch busy, wait RX2.");
            }
        }
        //// check timer RX2
        rtn = read( tfd_rx2, &exp, sizeof(uint64_t));
        if ( rtn < 0){
            if( errno != EINTR && errno != EAGAIN){
                die("tfd_rx2");
            }
        }else{
            show_time("RX2 timer expired");
            set_Ch2();
            rf_free = is_channel_free();
            if(rf_free){
                // transmit rf_pkt
                de_que(&txq, &tbuf);
                sf = DR_RX2;
                set_DataRate(sf);
                rf_transmitpacket(&tbuf);
                show_time( "Transmit rf pkt in RX2" );
            }else{
                show_time("Sorry, BUSY RX2.");
            }
            set_Channel(freq);
        }
        //// Check STDBY(TxDone) and Resume RxCont
        if( readRegister(REG_OPMODE) == SX72_MODE_STDBY){
            show_time("Txdone, Restart RX_CONT.");
            // clear radio IRQ flags
            writeRegister(REG_IRQ_FLAGS, 0xFF);
            //mask all radio IRQ flags but RxDone 
            writeRegister(REG_IRQ_FLAGS_MASK,  ~SX1276_IRQ_RXDONE_MASK);
            // Set DIO0 RxDone 
            writeRegister(REG_DIO_MAPPING_1, 0x00);
            // Set DevMode TXDone(STDBY) -> RX_Cont
            writeRegister(REG_OPMODE, SX72_MODE_RX_CONTINUOS);
        }
        //// check server packet ////
        msg_len = recvfrom(sock_down, buf_down, sizeof( buf_down)-1, 0 , (struct sockaddr *) &d_addr, &slen);     
        if (msg_len < 1) {
            if (errno != EAGAIN) {
                die("recv error");
	        }
        } else {
            show_time("recv from server");
            printf("\tRecv IP:%s, Port:%d. ", inet_ntoa(d_addr.sin_addr), ntohs(d_addr.sin_port) );
            if ((buf_down[1] == token_h) && (buf_down[2] == token_l)) {
                buf_down[msg_len] = 0; //add string terminator, just to be safe
                printf("recv token Ok. TYPE:%x\n", buf_down[3]);
                //printf("\nJSON down: %s\n", (char *)(buf_down + 4)); ///DEBUG: display JSON payload
            }else{
                //NEED TO CHECK PAKET TYPE
                printf("recved NEW token: %02x:%02x: TYPE:%x\n", buf_down[1], buf_down[2], buf_down[3]);
                if(buf_down[3] == PKT_PULL_RESP){
                    // Data from Server
                    show_time("Rcvd PULL_RESP, EnQue & Send TX_ACK");
                    // send TX_ACK w/ new token
                    send2serv(buf_down[1], buf_down[2], PKT_TX_ACK);                        // tx data to node
                    // find rf tx data JSON part
                    cut_rftxdata(buf_down);
                    // enque rf tx data
                    tbuf.bytes = txpkt.size;
                    memcpy(tbuf.buf, txpkt.payload, txpkt.size);
                    en_que(&txq, tbuf);
                    // will send when RX1 expired
                }else{
                    //???
                    printf("Token or Packet type error:\n");
                }
                printf("\t%d bytes received. ", msg_len);
                for(i=0;i<msg_len;i++){
                    printf("%02x:",buf_down[i]);
                }   
                printf("\n");
                if(msg_len > HEADER_RCV_LEN){
                    printf("rcvd stat: %s\n", (char *)(buf_down + HEADER_RCV_LEN)); 
                }
            }
        }    
        //// check timer keepalive ////            
        rtn = read( tfd_keepalive, &exp, sizeof(uint64_t) );
        if ( rtn < 0){
            if( errno != EINTR && errno != EAGAIN){
                die("tfd_keepalive");
            }
        }else{
            show_time("Keepalive timer expired");
            gen_token();
            // check rx pkt
            if(isempty_que(rxq) == 0){      // data 
                show_time("Send PUSH_DATA rxpk");
                de_que(&rxq, &tbuf);
                // send PUSH_DATA rxpk
                send2serv_que(token_h, token_l, &tbuf);
            }else if(--pull_pkt_cnt == 0){
                show_time("Send PUSH_DATA stat");
                pull_pkt_cnt = stat_times;
                sendstat(token_h, token_l);
            }else{
                show_time("Send PULL_DATA");
                // send PULL_DATA 
                send2serv(token_h,token_l, PKT_PULL_DATA);
                cp_nb_rx_rcv = 0;
                cp_nb_rx_ok = 0;
                cp_up_pkt_fwd = 0;
            }    
        }
    }   // end of while
    return (0);     
}
