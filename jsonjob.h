/*
* jsonjob.h
*/
#ifndef _JSONJOB_H_
#define _JSONJOB_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct  {
    uint32_t    freq_hz;        /*!> center frequency of TX */
    uint8_t     tx_mode;        /*!> select on what event/time the TX is triggered */
    uint32_t    count_us;       /*!> timestamp or delay in microseconds for TX trigger */
    int8_t      rf_power;       /*!> TX power, in dBm */
    uint8_t     bandwidth;      /*!> modulation bandwidth (LoRa only) */
    uint32_t    datarate;       /*!> TX datarate (baudrate for FSK, SF for LoRa) */
    uint8_t     coderate;       /*!> error-correcting code of the packet (LoRa only) */
    bool        invert_pol;     /*!> invert signal polarity, for orthogonal downlinks (LoRa only) */
    uint16_t    preamble;       /*!> set the preamble length, 0 for default */
    bool        no_crc;         /*!> if true, do not send a CRC in the packet */
    int     size;           /*!> payload size in bytes */
    uint8_t     payload[256];   /*!> buffer containing the payload */
} rf_txpkt_s;

// proto type declaration
void cut_rftxdata(uint8_t* );

#endif // _JSONJOB_H_