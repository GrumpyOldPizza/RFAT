/*
 * Copyright (c) 2014 Thomas Roell.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimers.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimers in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of Thomas Roell, nor the names of its contributors
 *     may be used to endorse or promote products derived from this Software
 *     without specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * WITH THE SOFTWARE.
 */

#if !defined(_RFAT_DISK_H)
#define _RFAT_DISK_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "rfat.h"

typedef struct _rfat_disk_t rfat_disk_t;

#define RFAT_BLK_SIZE                     512
#define RFAT_BLK_MASK                     511
#define RFAT_BLK_SHIFT                    9

#define RFAT_DISK_STATE_NONE              0
#define RFAT_DISK_STATE_INIT              1
#define RFAT_DISK_STATE_RESET             2
#define RFAT_DISK_STATE_READY             3
#define RFAT_DISK_STATE_READ_SEQUENTIAL   4
#define RFAT_DISK_STATE_WRITE_SEQUENTIAL  5

#define RFAT_DISK_TYPE_NONE               0
#define RFAT_DISK_TYPE_SDSC               1
#define RFAT_DISK_TYPE_SDHC               2

#define RFAT_DISK_FLAG_COMMAND_SUBSEQUENT 0x01

#define RFAT_DISK_MODE_NONE               0
#define RFAT_DISK_MODE_IDENTIFY           1
#define RFAT_DISK_MODE_DATA_TRANSFER      2

struct _rfat_disk_t {
    uint8_t                 state;
    uint8_t                 type;
    uint8_t                 flags;
    uint8_t                 shift;
    uint32_t                speed;
    uint8_t                 response[8];
    uint32_t                address;
    uint32_t                count;
    volatile uint8_t        *p_status;

#if (RFAT_CONFIG_DISK_SIMULATE == 1)
    uint8_t                 *image;
#endif /* (RFAT_CONFIG_DISK_SIMULATE == 1) */

#if (RFAT_CONFIG_STATISTICS == 1)
    struct {
        uint32_t                disk_reset;
        uint32_t                disk_send_command;
        uint32_t                disk_send_command_retry;
        uint32_t                disk_send_command_fail;
        uint32_t                disk_send_data;
        uint32_t                disk_send_data_retry;
        uint32_t                disk_send_data_fail;
        uint32_t                disk_receive_data;
        uint32_t                disk_receive_data_retry;
        uint32_t                disk_receive_data_fail;
	uint32_t                disk_read_single;
	uint32_t                disk_read_sequential;
	uint32_t                disk_read_coalesce;
	uint32_t                disk_write_single;
	uint32_t                disk_write_sequential;
	uint32_t                disk_write_coalesce;
    }                       statistics;
#endif /* (RFAT_CONFIG_STATISTICS == 1) */
};

#if (RFAT_CONFIG_DISK_CRC == 1)

extern const uint8_t rfat_crc7_table[256];
extern const uint16_t rfat_crc16_table[256];

extern uint8_t rfat_compute_crc7(const uint8_t *data, uint32_t count);
extern uint16_t rfat_compute_crc16(const uint8_t *data, uint32_t count);

#define RFAT_UPDATE_CRC7(_crc7, _data)    { (_crc7)  = rfat_crc7_table[((uint8_t)(_crc7) << 1) ^ (uint8_t)(_data)]; }
#define RFAT_UPDATE_CRC16(_crc16, _data)  { (_crc16) = (uint16_t)((_crc16) << 8) ^ rfat_crc16_table[(uint8_t)((_crc16) >> 8) ^ (uint8_t)(_data)]; }

#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

#if (RFAT_CONFIG_STATISTICS == 1)

#define RFAT_DISK_STATISTICS_COUNT(_name)         { disk->statistics._name += 1; }
#define RFAT_DISK_STATISTICS_COUNT_N(_name,_n)    { disk->statistics._name += (_n); }

#else /* (RFAT_CONFIG_STATISTICS == 1) */

#define RFAT_DISK_STATISTICS_COUNT(_name)         /**/
#define RFAT_DISK_STATISTICS_COUNT_N(_name,_n)    /**/

#endif /* (RFAT_CONFIG_STATISTICS == 1) */

extern rfat_disk_t * rfat_disk_acquire(void);
extern int rfat_disk_release(rfat_disk_t *disk);
extern int rfat_disk_info(rfat_disk_t *disk, bool *p_write_protected, uint32_t *p_block_count, uint32_t *p_au_size, uint32_t *p_serial);
extern int rfat_disk_read(rfat_disk_t *disk, uint32_t address, uint8_t *data);
extern int rfat_disk_read_sequential(rfat_disk_t *disk, uint32_t address, uint32_t length, uint8_t *data);
extern int rfat_disk_write(rfat_disk_t *disk, uint32_t address, const uint8_t *data);
extern int rfat_disk_write_sequential(rfat_disk_t *disk, uint32_t address, uint32_t length, const uint8_t *data, volatile uint8_t *p_status);
extern int rfat_disk_sync(rfat_disk_t *disk, volatile uint8_t *p_status);

#endif /*_RFAT_DISK_H */
