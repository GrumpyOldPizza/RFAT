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

#include "rfat_disk.h"
#include "rfat_port.h"

static rfat_disk_t rfat_disk;

#if (RFAT_CONFIG_DISK_SIMULATE == 0)

#define FALSE  0
#define TRUE   1

#if !defined(NULL)
#define NULL   ((void*)0)
#endif

static int rfat_disk_send_command(rfat_disk_t *disk, uint8_t index, const uint32_t argument, uint32_t count);
static int rfat_disk_send_data(rfat_disk_t *disk, uint8_t token, const uint8_t *data, uint32_t count, unsigned int *p_retries);
static int rfat_disk_receive_data(rfat_disk_t *disk, uint8_t *data, uint32_t count, unsigned int *p_retries);
static int rfat_disk_reset(rfat_disk_t *disk);
static int rfat_disk_lock(rfat_disk_t *disk, int state, uint32_t address);
static int rfat_disk_unlock(rfat_disk_t *disk, int status);


#define SD_CMD_GO_IDLE_STATE           (0)
#define SD_CMD_SEND_OP_COND            (1)
#define SD_CMD_SEND_IF_COND            (8)
#define SD_CMD_SEND_CSD                (9)
#define SD_CMD_SEND_CID                (10)
#define SD_CMD_STOP_TRANSMISSION       (12)
#define SD_CMD_SEND_STATUS             (13)
#define SD_CMD_SET_BLOCKLEN            (16)
#define SD_CMD_READ_SINGLE_BLOCK       (17)
#define SD_CMD_READ_MULTIPLE_BLOCK     (18)
#define SD_CMD_WRITE_SINGLE_BLOCK      (24)
#define SD_CMD_WRITE_MULTIPLE_BLOCK    (25)
#define SD_CMD_PROGRAM_CSD             (27)
#define SD_CMD_ERASE_WR_BLK_START_ADDR (32)
#define SD_CMD_ERASE_WR_BLK_END_ADDR   (33)
#define SD_CMD_ERASE                   (38)
#define SD_CMD_APP_CMD                 (55)
#define SD_CMD_READ_OCR                (58)
#define SD_CMD_CRC_ON_OFF              (59)

/* Add a 0x40 to ACMD, so that it's clear what is meant. 
 * In rfat_disk_send_command() a 0x40 will set anyway.
 */
#define SD_ACMD_SD_STATUS              (0x40+13)
#define SD_ACMD_SEND_NUM_WR_BLOCKS     (0x40+22)
#define SD_ACMD_SET_WR_BLK_ERASE_COUNT (0x40+23)
#define SD_ACMD_SD_SEND_OP_COND        (0x40+41)
#define SD_ACMD_SET_CLR_CARD_DETECT    (0x40+42)
#define SD_ACMD_SEND_SCR               (0x40+51)

#define SD_R1_VALID_MASK               0x80
#define SD_R1_VALID_DATA               0x00
#define SD_R1_IN_IDLE_STATE            0x01
#define SD_R1_ERASE_RESET              0x02
#define SD_R1_ILLEGAL_COMMAND          0x04
#define SD_R1_COM_CRC_ERROR            0x08
#define SD_R1_ERASE_SEQUENCE_ERROR     0x10
#define SD_R1_ADDRESS_ERROR            0x20
#define SD_R1_PARAMETER_ERROR          0x40

#define SD_R2_CARD_IS_LOCKED           0x01
#define SD_R2_WP_ERASE_SKIP            0x02
#define SD_R2_LOCK_UNLOCK_FAILED       0x02
#define SD_R2_EXECUTION_ERROR          0x04
#define SD_R2_CC_ERROR                 0x08
#define SD_R2_CARD_ECC_FAILED          0x10
#define SD_R2_WP_VIOLATION             0x20
#define SD_R2_ERASE_PARAM              0x40
#define SD_R2_OUT_OF_RANGE             0x80
#define SD_R2_CSD_OVERWRITE            0x80

#define SD_READY_TOKEN                 0xff    /* host -> card, card -> host */
#define SD_START_READ_TOKEN            0xfe    /* card -> host */
#define SD_START_WRITE_SINGLE_TOKEN    0xfe    /* host -> card */
#define SD_START_WRITE_MULTIPLE_TOKEN  0xfc    /* host -> card */
#define SD_STOP_TRANSMISSION_TOKEN     0xfd    /* host -> card */

/* data response token for WRITE */
#define SD_DATA_RESPONSE_MASK          0x1f
#define SD_DATA_RESPONSE_ACCEPTED      0x05
#define SD_DATA_RESPONSE_CRC_ERROR     0x0b
#define SD_DATA_RESPONSE_WRITE_ERROR   0x0d

/* data error token for READ */
#define SD_DATA_ERROR_TOKEN_VALID_MASK 0xf0
#define SD_DATA_ERROR_TOKEN_VALID_DATA 0x00
#define SD_DATA_ERROR_EXECUTION_ERROR  0x01
#define SD_DATA_ERROR_CC_ERROR         0x02
#define SD_DATA_ERROR_CARD_ECC_FAILED  0x04
#define SD_DATA_ERROR_OUT_OF_RANGE     0x08

#if (RFAT_CONFIG_DISK_CRC == 1)

const uint8_t rfat_crc7_table[256]= {
    0x00, 0x09, 0x12, 0x1b, 0x24, 0x2d, 0x36, 0x3f,
    0x48, 0x41, 0x5a, 0x53, 0x6c, 0x65, 0x7e, 0x77,
    0x19, 0x10, 0x0b, 0x02, 0x3d, 0x34, 0x2f, 0x26,
    0x51, 0x58, 0x43, 0x4a, 0x75, 0x7c, 0x67, 0x6e,
    0x32, 0x3b, 0x20, 0x29, 0x16, 0x1f, 0x04, 0x0d,
    0x7a, 0x73, 0x68, 0x61, 0x5e, 0x57, 0x4c, 0x45,
    0x2b, 0x22, 0x39, 0x30, 0x0f, 0x06, 0x1d, 0x14,
    0x63, 0x6a, 0x71, 0x78, 0x47, 0x4e, 0x55, 0x5c,
    0x64, 0x6d, 0x76, 0x7f, 0x40, 0x49, 0x52, 0x5b,
    0x2c, 0x25, 0x3e, 0x37, 0x08, 0x01, 0x1a, 0x13,
    0x7d, 0x74, 0x6f, 0x66, 0x59, 0x50, 0x4b, 0x42,
    0x35, 0x3c, 0x27, 0x2e, 0x11, 0x18, 0x03, 0x0a,
    0x56, 0x5f, 0x44, 0x4d, 0x72, 0x7b, 0x60, 0x69,
    0x1e, 0x17, 0x0c, 0x05, 0x3a, 0x33, 0x28, 0x21,
    0x4f, 0x46, 0x5d, 0x54, 0x6b, 0x62, 0x79, 0x70,
    0x07, 0x0e, 0x15, 0x1c, 0x23, 0x2a, 0x31, 0x38,
    0x41, 0x48, 0x53, 0x5a, 0x65, 0x6c, 0x77, 0x7e,
    0x09, 0x00, 0x1b, 0x12, 0x2d, 0x24, 0x3f, 0x36,
    0x58, 0x51, 0x4a, 0x43, 0x7c, 0x75, 0x6e, 0x67,
    0x10, 0x19, 0x02, 0x0b, 0x34, 0x3d, 0x26, 0x2f,
    0x73, 0x7a, 0x61, 0x68, 0x57, 0x5e, 0x45, 0x4c,
    0x3b, 0x32, 0x29, 0x20, 0x1f, 0x16, 0x0d, 0x04,
    0x6a, 0x63, 0x78, 0x71, 0x4e, 0x47, 0x5c, 0x55,
    0x22, 0x2b, 0x30, 0x39, 0x06, 0x0f, 0x14, 0x1d,
    0x25, 0x2c, 0x37, 0x3e, 0x01, 0x08, 0x13, 0x1a,
    0x6d, 0x64, 0x7f, 0x76, 0x49, 0x40, 0x5b, 0x52,
    0x3c, 0x35, 0x2e, 0x27, 0x18, 0x11, 0x0a, 0x03,
    0x74, 0x7d, 0x66, 0x6f, 0x50, 0x59, 0x42, 0x4b,
    0x17, 0x1e, 0x05, 0x0c, 0x33, 0x3a, 0x21, 0x28,
    0x5f, 0x56, 0x4d, 0x44, 0x7b, 0x72, 0x69, 0x60,
    0x0e, 0x07, 0x1c, 0x15, 0x2a, 0x23, 0x38, 0x31,
    0x46, 0x4f, 0x54, 0x5d, 0x62, 0x6b, 0x70, 0x79
};

const uint16_t rfat_crc16_table[256]= {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823, 
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12, 
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49, 
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70, 
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067, 
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c, 
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634, 
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab, 
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3, 
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

uint8_t rfat_compute_crc7(const uint8_t *data, uint32_t count)
{
    unsigned int n;
    uint8_t crc7 = 0;

    for (n = 0; n < count; n++)
    {
	RFAT_UPDATE_CRC7(crc7, data[n]);
    }
    
    return crc7;
}

uint16_t rfat_compute_crc16(const uint8_t *data, uint32_t count)
{
    unsigned int n;
    uint16_t crc16 = 0;

    for (n = 0; n < count; n++)
    {
	RFAT_UPDATE_CRC16(crc16, data[n]);
    }
    
    return crc16;
}

#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

/*
 * rfat_disk_wait_ready(rfat_disk_t *disk)
 *
 * Wait till DO transitions from BUSY (0x00) to READY (0xff).
 */

static int rfat_disk_wait_ready(rfat_disk_t *disk)
{
    int status = F_NO_ERROR;
    unsigned int n;
    uint8_t response;

    /* While waiting for non busy (not 0x00) the host can
     * release the CS line to let somebody else access the
     * bus. However there needs to be an extra clock
     * cycle after driving CS to H for the card to release DO,
     * as well as one extra clock cycle after driving CS to L
     * before the data is valid again.
     */

#if defined(RFAT_PORT_DISK_TIME_START)

    /* If there is a timer available, do a quick loop to avoid
     * having to query the possibly expensive timer. 
     */
    for (n = 0; n < 64; n++)
    {
        response = RFAT_PORT_DISK_SPI_RECEIVE();

        if (response == SD_READY_TOKEN)
        {
            break;
        }
    }

    if (response != SD_READY_TOKEN)
    {
        RFAT_PORT_DISK_TIME_START();

	do
	{
	    response = RFAT_PORT_DISK_SPI_RECEIVE();
	
	    if (response != SD_READY_TOKEN)
	    {
		if (RFAT_PORT_DISK_TIME_ELAPSED(250))
		{
		    break;
		}

#if defined(RFAT_PORT_DISK_YIELD)
		RFAT_PORT_DISK_SPI_DESELECT();

		RFAT_PORT_DISK_UNLOCK();

		RFAT_PORT_DISK_YIELD();

		status = RFAT_PORT_DISK_LOCK();

		if (status == F_NO_ERROR)
		{
		    RFAT_PORT_DISK_SPI_SELECT();
		}
#endif /* RFAT_PORT_DISK_SPI_YIELD */
	    }
	}
	while ((status == F_NO_ERROR) && (response != SD_READY_TOKEN));
    }

#else /* RFAT_PORT_DISK_TIME_START */

    /* If there is no time available, loop in 256 transfer chunks.
     * Each transfer takes up 8 cycles on the bus. Hence the loop will
     * consume at least 2048 clock cycles. Since the speed of the bus is
     * known, one can derive a timeout from there.
     */

    uint32_t cycles = 0;

    do
    {
	for (n = 0; n < 256; n++)
	{
	    response = RFAT_PORT_DISK_SPI_RECEIVE();
	    
	    if (response == SD_READY_TOKEN)
	    {
		break;
	    }
	}

	if (response != SD_READY_TOKEN)
	{
	    cycles += (2048 * (1000 / 250));
	}
    }
    while ((response != SD_READY_TOKEN) && (cycles < disk->speed));

#endif /* RFAT_PORT_DISK_TIME_START */

    disk->flags &= ~RFAT_DISK_FLAG_COMMAND_SUBSEQUENT;

    if (response != SD_READY_TOKEN)
    {
	status = F_ERR_ONDRIVE;
    }

    return status;
}

static int rfat_disk_send_command(rfat_disk_t *disk, uint8_t index, const uint32_t argument, uint32_t count)
{
    int status = F_NO_ERROR;
    unsigned int n;
    uint8_t data[5], response, crc7;
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_COMMAND_RETRIES != 0)
    unsigned int retries = RFAT_CONFIG_DISK_COMMAND_RETRIES +1;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_COMMAND_RETRIES != 0) */
    unsigned int retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_COMMAND_RETRIES != 0) */

    RFAT_DISK_STATISTICS_COUNT(disk_send_command);

    data[0] = 0x40 | index;
    data[1] = argument >> 24;
    data[2] = argument >> 16;
    data[3] = argument >> 8;
    data[4] = argument >> 0;

#if (RFAT_CONFIG_DISK_CRC == 1)
    crc7 = (rfat_compute_crc7(data, 5) << 1) | 0x01;
#else /* (RFAT_CONFIG_DISK_CRC == 1) */
    if (index == SD_CMD_GO_IDLE_STATE)
    {
        crc7 = 0x95;
    }
    else if (index == SD_CMD_SEND_IF_COND)
    {
        crc7 = 0x87;
    } 
    else
    {
        crc7 = 0x01;
    }
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

    do
    {
	/* A command needs at least one back to back idle cycle. Unless of 
	 * course it's known to have this extra byte seen already.
	 */
	
	if (disk->flags & RFAT_DISK_FLAG_COMMAND_SUBSEQUENT)
	{
	    RFAT_PORT_DISK_SPI_RECEIVE();
	}

	disk->flags |= RFAT_DISK_FLAG_COMMAND_SUBSEQUENT;
	
	RFAT_PORT_DISK_SPI_SEND(data[0]);
	RFAT_PORT_DISK_SPI_SEND(data[1]);
	RFAT_PORT_DISK_SPI_SEND(data[2]);
	RFAT_PORT_DISK_SPI_SEND(data[3]);
	RFAT_PORT_DISK_SPI_SEND(data[4]);
	RFAT_PORT_DISK_SPI_SEND(crc7);

	/* NCR is 1..8 bytes, so simply always discard the first byte,
	 * and then read up to 8 bytes or till a vaild response
	 * was seen. N.b that STOP_TRANSMISSION specifies that
	 * the first byte on DataOut after reception of the command
	 * is a stuffing byte that has to be ignored. The discard
	 * takes care of that here.
	 */

	RFAT_PORT_DISK_SPI_RECEIVE();

	for (n = 0; n < 8; n++)
	{
	    response = RFAT_PORT_DISK_SPI_RECEIVE();
        
	    if (!(response & 0x80))
	    {
		/*
		 * A STOP_TRANSMISSION can be issued before the card
		 * had send a "Data Error Token" for that last block
		 * that we are not really intrested in. This could result
		 * in a "Parameter Error" if the next block is out of bounds.
		 * Due to the way CMD_STOP_TRANSMISSION gets send there is
		 * also the chance it gets an "Illegal Command" error ...
		 */
		if (index == SD_CMD_STOP_TRANSMISSION)
		{
		    response &= ~(SD_R1_ILLEGAL_COMMAND | SD_R1_PARAMETER_ERROR);
		}
		break;
	    }
	}

	disk->response[0] = response;
	
	/* This below is somewhat tricky. There can be multiple communication
	 * failures:
	 *
	 * - card not responding
	 * - noise on the DATA line
	 * - noise on the CLK line
	 *
	 * If the card does not respond there should be 1xxxxxxxb as a response.
	 * If there is noise on the DATA line a CRC error is detect with a
	 * 0xxx1xxxx response.
	 *
	 * If there is noise on the CLK line, bits will be missing. The loop
	 * up there will throw out anything with a 1xxxxxxxb pattern. Hence if
	 * bits are missing the possible replies are (n.b. that the CRC error
	 * bit will be set, and the missing bits will be filled in as ones
	 * from the right):
	 *
	 * 00010001
	 * 00100011
	 * 01000111
	 * 00111111
	 * 01111111
	 *
	 * So in a nutshell, bit 0 will be always set, and at least one other bit
	 * will be set. This means 0x01 is a legal pattern. N.b. that without
	 * CRC only the "card not responding" case can be addressed.
	 *
	 * If such errors occur, a retry process is used, whereby the CS line
	 * is toggled to allow the card to resync.
	 */

	if (
#if (RFAT_CONFIG_DISK_CRC == 1)
	    ((response != 0x01) && (response & 0x89)) 
#else /* (RFAT_CONFIG_DISK_CRC == 1) */
	    (response & 0x80)
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */
	    )
	{
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_COMMAND_RETRIES != 0)
	    if (retries > 1)
	    {
		RFAT_PORT_DISK_SPI_DESELECT();

		RFAT_PORT_DISK_SPI_SELECT();

		disk->flags &= ~RFAT_DISK_FLAG_COMMAND_SUBSEQUENT;

		RFAT_DISK_STATISTICS_COUNT(disk_send_command_retry);
		    
		retries--;
	    }
	    else
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_COMMAND_RETRIES != 0) */
	    {
		status = F_ERR_ONDRIVE;

		retries = 0;
	    }
	}
	else
	{
	    /* Always read the rest of the data to avoid synchronization
	     * issues. Worst case the data read is random.
	     */
	    for (n = 1; n <= count; n++)
	    {
		disk->response[n] = RFAT_PORT_DISK_SPI_RECEIVE();
	    } 

	    if (response != 0x00)
	    {
		if (disk->state != RFAT_DISK_STATE_RESET)
		{
		    status = F_ERR_ONDRIVE;
		}
	    }

	    retries = 0;
	}
    }
    while (retries != 0);

    if (status != F_NO_ERROR)
    {
	RFAT_DISK_STATISTICS_COUNT(disk_send_command_fail);
    }

    return status;
}

static int rfat_disk_send_data(rfat_disk_t *disk, uint8_t token, const uint8_t *data, uint32_t count, unsigned int *p_retries)
{
    int status = F_NO_ERROR;
    unsigned int retries = *p_retries;
    uint8_t response;
#if !defined(RFAT_PORT_DISK_SPI_SEND_BLOCK)
    unsigned int n;
    uint32_t crc16
#endif /* !RFAT_PORT_DISK_SPI_SEND_BLOCK */

    RFAT_DISK_STATISTICS_COUNT(disk_send_data);

    RFAT_PORT_DISK_SPI_SEND(token);

#if defined(RFAT_PORT_DISK_SPI_SEND_BLOCK)

    RFAT_PORT_DISK_SPI_SEND_BLOCK(data);

#else /* RFAT_PORT_DISK_SPI_SEND_BLOCK */

    crc16 = 0;

    for (n = 0; n < count; n++)
    {
#if (RFAT_CONFIG_DISK_CRC == 1)
	RFAT_UPDATE_CRC16(crc16, data[n]);
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

	RFAT_PORT_DISK_SPI_SEND(data[n]);
    } 

    RFAT_PORT_DISK_SPI_SEND(crc16 >> 8);
    RFAT_PORT_DISK_SPI_SEND(crc16);
    
#endif /* RFAT_PORT_DISK_SPI_SEND_BLOCK */

    /* At last read back the "Data Response Token":
     *
     * 0x05 No Error
     * 0x0b CRC Error
     * 0x0d Write Error
     */

    response = RFAT_PORT_DISK_SPI_RECEIVE() & SD_DATA_RESPONSE_MASK;

    if (response == SD_DATA_RESPONSE_ACCEPTED)
    {
	retries = 0;
    }
    else
    {
#if (RFAT_CONFIG_DISK_CRC == 1)
	/* But then it get's interesting. It's possible that the start token
	 * morphed into a STOP_TRANSMISSION token, or got lost all together.
	 * In that case there would be a BUSY state. So one had to wait till
	 * that was done before issuing the CMD_STOP_TRANSMISSION later on.
	 * The other case is where clock pulses were lost. In that cases the
	 * toggle of CS line will resync the clock, and we'd end up in
	 * a READY state after one dummy byte. But there might be still data 
	 * left over. This left over data will predictably end up in a 
	 * DATA_RESPONSE_CRC_ERROR, which shares R1_COM_CRC_ERROR with a normal
	 * command response. Hence the subsequent CMDP_STOP_TRANSMISSION will
	 * consume that left over data as 0xff or 0x0b ... and due to the retry
	 * mechanism resync.
	 */
	RFAT_PORT_DISK_SPI_DESELECT();
	
	RFAT_PORT_DISK_SPI_SELECT();
	
	status = rfat_disk_wait_ready(disk);

	if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */
	{
	    /* A non-accepted "Data Response Token" in a write multiple block
	     * operation shall be followed by a CMD_STOP_TRANSMISSION
	     * (not a "Stop Token").
	     */
	    
	    status = rfat_disk_send_command(disk, SD_CMD_STOP_TRANSMISSION, 0, 0);
	    
	    if (status == F_NO_ERROR)
	    {
		status = rfat_disk_wait_ready(disk);
	    
		if (status == F_NO_ERROR)
		{
		    disk->state = RFAT_DISK_STATE_READY;

		    status = rfat_disk_send_command(disk, SD_CMD_SEND_STATUS, 0, 1);

		    if (status == F_NO_ERROR)
		    {
			if (disk->response[0] != 0x00)
			{
			    /* Here the complete error response is available, so one can find out the
			     * true cause for the write error.
			     */
			
			    if (disk->response[1] & (SD_R2_CARD_IS_LOCKED | SD_R2_WP_ERASE_SKIP | SD_R2_EXECUTION_ERROR | SD_R2_CC_ERROR | SD_R2_ERASE_PARAM | SD_R2_OUT_OF_RANGE))
			    {
				status = F_ERR_ONDRIVE;
			    }
			    else if (disk->response[1] & SD_R2_WP_VIOLATION)
			    {
				status = F_ERR_WRITEPROTECT;
			    }
			    else
			    {
				/* CARD_ECC_FAILED */
				status = F_ERR_INVALIDSECTOR;
			    }

			    retries = 0;
			}
			else
			{
			    /* If there was not error posted, it must have been a CRC error, so
			     * let's restart the operatio if possible ...
			     */
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
			    if (retries > 1)
			    {
				RFAT_DISK_STATISTICS_COUNT(disk_send_data_retry);
			    
				retries--;
			    }
			    else
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
			    {
				status = F_ERR_WRITE;
			    
				retries = 0;
			    }
			}
		    }
		}
	    }
	}
    }

    if (status != F_NO_ERROR)
    {
	RFAT_DISK_STATISTICS_COUNT(disk_send_data_fail);
    }

#if (RFAT_CONFIG_DISK_DATA_RETRIES == 0)

    /* There are paths throu the code when "status" gets set,
     * but "retries" does not get touched. So if "retries"
     * is known to be 0, make it so.
     */

    retries = 0;

#endif /* (RFAT_CONFIG_DISK_DATA_RETRIES == 0) */

    *p_retries = retries;

    return status;
}

static int rfat_disk_receive_data(rfat_disk_t *disk, uint8_t *data, uint32_t count, unsigned int *p_retries)
{
    int status = F_NO_ERROR;
    unsigned int n;
    unsigned int retries = *p_retries;
    uint8_t token;
#if (RFAT_CONFIG_DISK_CRC == 1)
    uint32_t crc16;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

    RFAT_DISK_STATISTICS_COUNT(disk_receive_data);

    /* Before we waited for a non 0xff token.
     * If a "Start Block Token" (0xfe) zips by then
     * a data block will follow. Otherwise a "Data Error Token"
     * shows up:
     *
     * 0x01 Execution Error
     * 0x02 CC Error
     * 0x04 Card ECC failed
     * 0x08 Out of Range
     *
     * The maximum documented timeout is 100ms for SDHC.
     */

#if defined(RFAT_PORT_DISK_TIME_START)

    /* If there is a timer available, do a quick loop to avoid
     * having to query the expensive timer. 
     */

    for (n = 0; n < 64; n++)
    {
        token = RFAT_PORT_DISK_SPI_RECEIVE();

        if (token != SD_READY_TOKEN)
        {
            break;
        }
    }

    if (token == SD_READY_TOKEN)
    {
        RFAT_PORT_DISK_TIME_START();

	do
	{
	    token = RFAT_PORT_DISK_SPI_RECEIVE();
	    
	    if (token == SD_READY_TOKEN)
	    {
		if (RFAT_PORT_DISK_TIME_ELAPSED(100))
		{
		    break;
		}
	    }
	}
	while (token == SD_READY_TOKEN);
    }

#else /* RFAT_PORT_DISK_TIME_START */

    /* If there is no time available, loop in 256 transfer chunks.
     * Each transfer take up 8 cycles on the bus. Hence the loop will
     * consume at least 2048 clock cycles. Since the speed of the bus is
     * known, one can derive a timeout from there.
     */

    uint32_t cycles = 0;

    do
    {
	for (n = 0; n < 256; n++)
	{
	    token = RFAT_PORT_DISK_SPI_RECEIVE();
	    
	    if (token != SD_READY_TOKEN)
	    {
		break;
	    }
	}

	if (token == SD_READY_TOKEN)
	{
	    cycles += (2048 * 10); /* 100ms */
	} 
    }
    while ((token == SD_READY_TOKEN) && (cycles < disk->speed));

#endif /* RFAT_PORT_DISK_TIME_START */

    if (token != SD_START_READ_TOKEN)
    {
	/* On an invalid token a toggle of the CS signal
	 * will resync the clock. The stop is required,
	 * as it's unclear as to whether the transfer is 
	 * under way of not.
	 */
	
	RFAT_PORT_DISK_SPI_DESELECT();
	
	RFAT_PORT_DISK_SPI_SELECT();
	
	disk->flags &= ~RFAT_DISK_FLAG_COMMAND_SUBSEQUENT;
	
	status = rfat_disk_send_command(disk, SD_CMD_STOP_TRANSMISSION, 0, 0);

	if (status == F_NO_ERROR)
	{
	    disk->state = RFAT_DISK_STATE_READY;
	    
	    if ((token & SD_DATA_ERROR_TOKEN_VALID_MASK) != SD_DATA_ERROR_TOKEN_VALID_DATA)
	    {
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
		if (retries > 1)
		{
		    RFAT_DISK_STATISTICS_COUNT(disk_receive_data_retry);
		
		    retries--;
		}
		else
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
		{
		    status = F_ERR_READ;
		    
		    retries = 0;
		}
	    }
	    else
	    {
		if (token & (SD_DATA_ERROR_EXECUTION_ERROR | SD_DATA_ERROR_CC_ERROR | SD_DATA_ERROR_OUT_OF_RANGE))
		{
		    status = F_ERR_ONDRIVE;
		}
		else
		{
		    /* CARD_ECC_FAILED */
		    status = F_ERR_INVALIDSECTOR;
		}
		
		retries = 0;
	    }
	}
	else 
	{ 
	    retries = 0;
	}
    }
    else
    {
#if (RFAT_CONFIG_DISK_CRC == 1)

#if defined(RFAT_PORT_DISK_SPI_RECEIVE_BLOCK)
	if (count == RFAT_BLK_SIZE)
	{
	    crc16 = RFAT_PORT_DISK_SPI_RECEIVE_BLOCK(data);
	}
	else
#endif /* RFAT_PORT_DISK_SPI_RECEIVE_BLOCK */
	{
	    crc16 = 0;

	    for (n = 0; n < count; n++)
	    {
		data[n] = RFAT_PORT_DISK_SPI_RECEIVE();
		
		RFAT_UPDATE_CRC16(crc16, data[n]);
	    } 

	    crc16 ^= (RFAT_PORT_DISK_SPI_RECEIVE() << 8);
	    crc16 ^= RFAT_PORT_DISK_SPI_RECEIVE();
	}

	if (crc16 != 0)
	{
	    /* On an invalid token a toggle of the CS signal
	     * will resync the clock. The stop is required,
	     * as it's unclear as to whether the tranfer is 
	     * under way of not.
	     */
	
	    RFAT_PORT_DISK_SPI_DESELECT();

	    RFAT_PORT_DISK_SPI_SELECT();

	    disk->flags &= ~RFAT_DISK_FLAG_COMMAND_SUBSEQUENT;
	
	    status = rfat_disk_send_command(disk, SD_CMD_STOP_TRANSMISSION, 0, 0);
	    
	    if (status == F_NO_ERROR)
	    {
		disk->state = RFAT_DISK_STATE_READY;
	    
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
		if (retries > 1)
		{
		    RFAT_DISK_STATISTICS_COUNT(disk_receive_data_retry);

		    retries--;
		}
		else
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
		{
		    status = F_ERR_READ;
		    
		    retries = 0;
		}
	    }
	    else
	    {
		retries = 0;
	    }
	}
	else
	{
	    retries = 0;
	}

#else /* (RFAT_CONFIG_DISK_CRC == 1) */

#if defined(RFAT_PORT_DISK_SPI_RECEIVE_BLOCK)
	if (count == RFAT_BLK_SIZE)
	{
	    RFAT_PORT_DISK_SPI_RECEIVE_BLOCK(data);
	}
	else
#endif /* RFAT_PORT_DISK_SPI_RECEIVE_BLOCK */
	{
	    for (n = 0; n < count; n++)
	    {
		data[n] = RFAT_PORT_DISK_SPI_RECEIVE();
	    } 
    
	    RFAT_PORT_DISK_SPI_RECEIVE();
	    RFAT_PORT_DISK_SPI_RECEIVE();
	}

	retries = 0;
    
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */
    }

    if (status != F_NO_ERROR)
    {
	RFAT_DISK_STATISTICS_COUNT(disk_receive_data_fail);
    }

#if (RFAT_CONFIG_DISK_DATA_RETRIES == 0)

    /* There are paths throu the code when "status" gets set,
     * but "retries" does not get touched. So if "retries"
     * is known to be 0, make it so.
     */

    retries = 0;

#endif /* (RFAT_CONFIG_DISK_DATA_RETRIES == 0) */

    *p_retries = retries;
    
    return status;
}

static int rfat_disk_reset(rfat_disk_t *disk)
{
    int status = F_NO_ERROR;
    unsigned int n, type;

    RFAT_DISK_STATISTICS_COUNT(disk_reset);

    type = RFAT_DISK_TYPE_NONE;
    
    disk->speed = RFAT_PORT_DISK_SPI_MODE(RFAT_DISK_MODE_IDENTIFY);

    /* Apply an initial CMD_GO_IDLE_STATE, so that the card is out of
     * data read/write mode, and can properly respond.
     */

    rfat_disk_send_command(disk, SD_CMD_GO_IDLE_STATE, 0, 0);

    if (disk->response[0] != 0x01)
    {
	/* There could be 2 error scenarios. One is that the
	 * CMD_GO_IDLE_STATE was send while the card was waiting
	 * for the next "Start Block" / "Stop Transmission" token.
	 * The card will answer that normally by signaling BUSY,
	 * when means there would be a 0x00 response.
	 * The other case is that there was a reset request while
	 * being in the middle of a write transaction. As a result
	 * the card will send back a stream of non-zero values
	 * (READY or a "Data Response Token"), which can be handled
	 * by simple flushing the transaction (CRC will take care of
	 * rejecting the data ... without CRC the last write will be
	 * garbage.
	 *
	 * Hence first drain a possible write command, and then
	 * rety CMD_GO_IDLE_STATE after a rfat_disk_wait_ready().
	 * If the card is still in BUSY mode, then retry the
	 * CMD_GO_IDLE_STATE again.
	 */

	if (disk->response[0] != 0x00)
	{
	    for (n = 0; n < 1024; n++)
	    {
		RFAT_PORT_DISK_SPI_RECEIVE();
	    }

	    rfat_disk_send_command(disk, SD_CMD_GO_IDLE_STATE, 0, 0);
	}

	rfat_disk_wait_ready(disk);
	    
	rfat_disk_send_command(disk, SD_CMD_GO_IDLE_STATE, 0, 0);

	if (disk->response[0] == 0x00)
	{
	    rfat_disk_wait_ready(disk);
	    
	    rfat_disk_send_command(disk, SD_CMD_GO_IDLE_STATE, 0, 0);
	}
    }

    if (disk->response[0] == 0x01)
    {
	rfat_disk_send_command(disk, SD_CMD_SEND_IF_COND, 0x000001aa, 4);

	if (disk->response[0] == 0x01)
	{
	    if ((disk->response[1] == 0x00) &&
		(disk->response[2] == 0x00) &&
		(disk->response[3] == 0x01) &&
		(disk->response[4] == 0xaa))
	    {
		type = RFAT_DISK_TYPE_SDHC;
	    }
	    else
	    {
		type = RFAT_DISK_TYPE_NONE;
	    }
	}
	else
	{
	    type = RFAT_DISK_TYPE_SDSC;
	}
		
	if (type != RFAT_DISK_TYPE_NONE)
	{
	    /* Send ACMD_SD_SEND_OP_COND, and wait till the initialization process is done. The SDCARD has 1000ms to
	     * respond to that with 0x00.
	     */
	    
#if defined(RFAT_PORT_DISK_TIME_START)
	    
	    RFAT_PORT_DISK_TIME_START();

	    do
	    {
		rfat_disk_send_command(disk, SD_CMD_APP_CMD, 0, 0);
		
		if (disk->response[0] > 0x01)
		{
		    type = RFAT_DISK_TYPE_NONE;
		}
		else
		{
		    rfat_disk_send_command(disk, SD_ACMD_SD_SEND_OP_COND, ((type == RFAT_DISK_TYPE_SDHC) ? 0x40000000 : 0x00000000), 0);

		    if (disk->response[0] > 0x01)
		    {
			type = RFAT_DISK_TYPE_NONE;
		    }
		}
	    }
	    while ((type != RFAT_DISK_TYPE_NONE) && (disk->response[0] == 0x01) && !RFAT_PORT_DISK_TIME_ELAPSED(1000));

#else /* RFAT_PORT_DISK_TIME_START */

	    /* If there is no time available, things get tricky. A command
	     * takes up minimally 9 bus transfers. So the idea is to consume
	     * 64 bus transfers per iteration, which is at least 512 bus cycles.
	     * Since the speed of the bus is known, one can derive a timeout from there.
	     */
	    
	    uint32_t cycles = 0;
	    
	    do
	    {
		rfat_disk_send_command(disk, SD_CMD_APP_CMD, 0, 0);
		
		if (disk->response[0] > 0x01)
		{
		    type = RFAT_DISK_TYPE_NONE;
		}
		else
		{
		    rfat_disk_send_command(disk, SD_ACMD_SD_SEND_OP_COND, ((type == RFAT_DISK_TYPE_SDHC) ? 0x40000000 : 0x00000000), 0);

		    if (disk->response[0] > 0x01)
		    {
			type = RFAT_DISK_TYPE_NONE;
		    }
		    else
		    {
			if (disk->response[0] == 0x01)
			{
			    for (n = 0; n < (64 - 2 * 9); n++)
			    {
				RFAT_PORT_DISK_SPI_RECEIVE();
			    }
			    
			    cycles += 512;
			}
		    }
		}
	    }
	    while ((type != RFAT_DISK_TYPE_NONE) && (disk->response[0] == 0x01) && (cycles < disk->speed));

#endif /* RFAT_PORT_DISK_TIME_START */
	}

	if (disk->response[0] != 0x00)
	{
	    /* The card did not respond, so it's not an SDCARD.
	     */
	    type = RFAT_DISK_TYPE_NONE;
	}
	else
	{
	    /* Here there is a SDSC or SDHC card. Hence switch the state to RFAT_DISK_STATE_READY,
	     * so that rfat_disk_send_command() does the full error checking.
	     */
	    
	    disk->state = RFAT_DISK_STATE_READY;
	    
	    if (type == RFAT_DISK_TYPE_SDHC)
	    {
		/* Now it's time to find out whether we really have a SDHC, or a SDSC supporting V2.
		 */
		status = rfat_disk_send_command(disk, SD_CMD_READ_OCR, 0x00000000, 4);
			    
		if (status == F_NO_ERROR)
		{
		    if (!(disk->response[1] & 0x40))
		    {
			type = RFAT_DISK_TYPE_SDSC;
		    }
		}
	    }

#if (RFAT_CONFIG_DISK_CRC == 1)
	    if (status == F_NO_ERROR)
	    {
		status  = rfat_disk_send_command(disk, SD_CMD_CRC_ON_OFF, 1, 0);
	    }
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */
			
	    if (status == F_NO_ERROR)
	    {
		status = rfat_disk_send_command(disk, SD_CMD_SET_BLOCKLEN, 512, 0);
	    }

	    /* If there was no error up to here the "type" is valid and the card is
	     * ready for use.
	     */
	    if (status == F_NO_ERROR)
	    {
		disk->type = type;
		disk->flags = 0;
		disk->shift = (type == RFAT_DISK_TYPE_SDHC) ? 0 : 9;
		disk->speed = RFAT_PORT_DISK_SPI_MODE(RFAT_DISK_MODE_DATA_TRANSFER);
	    }
	}

	if (type == RFAT_DISK_TYPE_NONE)
	{
	    if (status == F_NO_ERROR)
	    {
		status = F_ERR_INVALIDMEDIA;
	    }
	}
    }
    else
    {
	status = F_ERR_CARDREMOVED;
    }

    if (status != F_NO_ERROR)
    {
	disk->state = RFAT_DISK_STATE_RESET;
	disk->speed = RFAT_PORT_DISK_SPI_MODE(RFAT_DISK_MODE_NONE);
    }
    
    return status;
}

static int rfat_disk_write_stop(rfat_disk_t *disk)
{
    int status = F_NO_ERROR;
    uint8_t response;

    status = rfat_disk_wait_ready(disk);
    
    if (status == F_NO_ERROR)
    {
	/* There need to be 8 clocks before the "Stop Transfer Token",
	 * and 8 clocks after it, before the card signals busy properly.
	 * The 8 clocks before are covered by rfat_disk_wait_ready().
	 */
	
	RFAT_PORT_DISK_SPI_SEND(SD_STOP_TRANSMISSION_TOKEN);
	
	status = rfat_disk_wait_ready(disk);

	if (status == F_NO_ERROR)
	{
	    /* Here it's getting a tad interesting. The spec says that
	     * an error that occured after the STOP_TRANSMISSION_TOKEN
	     * occured will be forwarded to the next command. So here
	     * one really has to send a CMD_SEND_STATUS to find out.
	     *
	     * But there is the chance that the STOP_TRANSMISSION_TOKEN
	     * was lost. If so, the CMD_SEND_STATUS will transition to BUSY
	     * state. When this is detected, one has to wait for READY and
	     * then reissue CMD_SEND_STATUS.
	     */
	    
	    status = rfat_disk_send_command(disk, SD_CMD_SEND_STATUS, 0, 1);
	    
	    if (status == F_NO_ERROR)
	    {
		response = RFAT_PORT_DISK_SPI_RECEIVE();
		
		disk->flags &= ~RFAT_DISK_FLAG_COMMAND_SUBSEQUENT;
		
		if (response != SD_READY_TOKEN)
		{
		    status = rfat_disk_wait_ready(disk);
		    
		    if (status == F_NO_ERROR)
		    {
			status = rfat_disk_send_command(disk, SD_CMD_SEND_STATUS, 0, 1);
		    }
		}
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		disk->state = RFAT_DISK_STATE_READY;

		if (disk->response[1] != 0x00)
		{
		    if (disk->response[1] & (SD_R2_CARD_IS_LOCKED | SD_R2_WP_ERASE_SKIP | SD_R2_CC_ERROR | SD_R2_ERASE_PARAM))
		    {
			status = F_ERR_ONDRIVE;
		    }
		    else
		    {
			if (disk->response[1] & (SD_R2_EXECUTION_ERROR | SD_R2_OUT_OF_RANGE))
			{
			    status = F_ERR_ONDRIVE;
			}
			else if (disk->response[1] & SD_R2_WP_VIOLATION)
			{
			    status = F_ERR_WRITEPROTECT;
			}
			else
			{
			    /* CARD_ECC_FAILED */
			    status = F_ERR_INVALIDSECTOR;
			}
			
			if (disk->p_status != NULL)
			{
			    if (*disk->p_status == F_NO_ERROR)
			    {
				*disk->p_status = status;
			    }
			    
			    status = F_NO_ERROR;
			}
		    }
		}
	    }
	}
    }

    disk->p_status = NULL;
				
    return status;
}

/* 
 * int rfat_disk_lock(rfat_disk_t *disk, int state, uint32_t address)
 *
 * Lock the disk device, pull CS low, and process "state" (READY, READ, WRITE).
 * Unless there is an error, the disk will be in READY state, locked and selected.
 */

static int rfat_disk_lock(rfat_disk_t *disk, int state, uint32_t address)
{
    int status = F_NO_ERROR;

#if defined(RFAT_PORT_DISK_LOCK)
    status = RFAT_PORT_DISK_LOCK();

    if (status == F_NO_ERROR)
#endif /* RFAT_PORT_DISK_LOCK */
    {
	disk->flags &= ~RFAT_DISK_FLAG_COMMAND_SUBSEQUENT;
	    
	if (disk->state == RFAT_DISK_STATE_RESET)
	{
	    if (!RFAT_PORT_DISK_SPI_PRESENT())
	    {
		status = F_ERR_CARDREMOVED;
	    }
	    else
	    {
		status = rfat_disk_reset(disk);
	    }
	}
	else
	{
	    RFAT_PORT_DISK_SPI_SELECT();

	    if (status == F_NO_ERROR)
	    {
		if ((disk->state == state) && (disk->address == address))
		{
		    /* Continuation of a previous read/write ... */
		}
		else
		{
		    /* No special case, move the disk to RFAT_DISK_STATE_READY.
		     */
		
		    if (disk->state == RFAT_DISK_STATE_READ_SEQUENTIAL)
		    {
			status = rfat_disk_send_command(disk, SD_CMD_STOP_TRANSMISSION, 0, 0);
		    
			if (status == F_NO_ERROR)
			{
			    disk->state = RFAT_DISK_STATE_READY;
			}
		    }

		    if (disk->state == RFAT_DISK_STATE_WRITE_SEQUENTIAL)
		    {
			status = rfat_disk_write_stop(disk);
		    }
		}
	    }
	}
    }
    
    if (status != F_NO_ERROR)
    {
	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

static int rfat_disk_unlock(rfat_disk_t *disk, int status)
{
    if (disk->state != RFAT_DISK_STATE_RESET)
    {
	RFAT_PORT_DISK_SPI_DESELECT();
    }
    
    if (status == F_ERR_ONDRIVE)
    {
	disk->state = RFAT_DISK_STATE_RESET;
	disk->speed = RFAT_PORT_DISK_SPI_MODE(RFAT_DISK_MODE_NONE);
    }

#if defined(RFAT_PORT_DISK_UNLOCK)
    if (status != F_ERR_BUSY)
    {
	RFAT_PORT_DISK_UNLOCK();
    }
#endif /* RFAT_PORT_DISK_UNLOCK */

    return status;
}

rfat_disk_t * rfat_disk_acquire(void)
{
    rfat_disk_t *disk = &rfat_disk;

    if (disk->state == RFAT_DISK_STATE_NONE)
    {
#if defined(RFAT_PORT_DISK_INIT)
	if (!RFAT_PORT_DISK_INIT())
	{
	    disk = NULL;
	}
	else
#endif /* RFAT_PORT_DISK_INIT */
	{
	    disk->state = RFAT_DISK_STATE_INIT;
	}
    }

    if (disk && (disk->state == RFAT_DISK_STATE_INIT))
    {
	disk->state = RFAT_DISK_STATE_RESET;
    }
    else
    {
	disk = NULL;
    }
   
    return disk;
}

int rfat_disk_release(rfat_disk_t *disk)
{
    int status = F_NO_ERROR;

    if (disk->state != RFAT_DISK_STATE_RESET)
    {
	status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);
	
	if (status == F_NO_ERROR)
	{

	    status = rfat_disk_unlock(disk, status);
	}
    }

    disk->state = RFAT_DISK_STATE_INIT;

    return status;
}

int rfat_disk_info(rfat_disk_t *disk, bool *p_write_protected, uint32_t *p_block_count, uint32_t *p_au_size, uint32_t *p_serial)
{
    int status = F_NO_ERROR;
    unsigned int retries;
    uint32_t c_size, c_size_mult, read_bl_len, au_size;
    uint8_t data[64];

    static uint32_t rfat_au_size_table[16] = {
	0,
	32,      /* 16KB  */
	64,      /* 32KB  */
	128,     /* 64KB  */
	256,     /* 128KB */
	512,     /* 256KB */
	1024,    /* 512KB */
	2048,    /* 1MB   */
	4096,    /* 2MB   */
	8192,    /* 4MB   */
	16384,   /* 8MB   */
	24576,   /* 12MB  */
	32768,   /* 16MB  */
	49152,   /* 24MB  */
	65536,   /* 32MB  */
	131072,  /* 64MB  */
    };

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

    if (status == F_NO_ERROR)
    {
	*p_write_protected = false;

#if defined(RFAT_PORT_DISK_SPI_WRITE_PROTECTED)
	if (RFAT_PORT_DISK_SPI_WRITE_PROTECTED())
	{
	    *p_write_protected = true;
	}
#endif /* RFAT_PORT_DISK_SPI_WRITE_PROTECTED */

#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
	retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
	retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
	
	do
	{
	    status = rfat_disk_send_command(disk, SD_CMD_SEND_CSD, 0, 0);

	    if (status == F_NO_ERROR)
	    {
		status = rfat_disk_receive_data(disk, data, 16, &retries);

		if ((status == F_NO_ERROR) && !retries)
		{
		    /* PERM_WRITE_PROTECT.
		     * TMP_WRITE_PROTECT
		     */
		    if (data[14] & 0x30)
		    {
			*p_write_protected = true;
		    }

		    if ((data[0] & 0xc0) == 0)
		    {
			/* SDSC */
			
			read_bl_len = (uint32_t)(data[5] & 0x0f);
			c_size = ((uint32_t)(data[6] & 0x03) << 10) | ((uint32_t)data[7] << 2) | ((uint32_t)data[8] >> 6);
			c_size_mult = ((uint32_t)(data[9] & 0x03) << 1) | ((uint32_t)(data[10] & 0x80) >> 7);
			
			*p_block_count = ((c_size + 1) << (c_size_mult + 2)) << (read_bl_len - 9);
		    }
		    else
		    {
			/* SDHC */
			
			c_size = ((uint32_t)(data[7] & 0x3f) << 16) | ((uint32_t)data[8] << 8) | ((uint32_t)data[9]);
			
			*p_block_count = (c_size + 1) << (19 - 9);
		    }
		}
	    }
	} 
	while ((status == F_NO_ERROR) && retries);
	
	if (status == F_NO_ERROR)
	{
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
	    retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
	    retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */

	    do
	    {
		status = rfat_disk_send_command(disk, SD_CMD_APP_CMD, 0, 0);
	  
		if (status == F_NO_ERROR)
		{
		    status = rfat_disk_send_command(disk, SD_ACMD_SD_STATUS, 0, 1);
	      
		    if (status == F_NO_ERROR)
		    {
			status = rfat_disk_receive_data(disk, data, 64, &retries);
		  
			if ((status == F_NO_ERROR) && !retries)
			{
			    /* If there is a UHS_AU_SIZE, use it, otherwise use
			     * the regular AU_SIZE. The issue at hand is that
			     * SDHC can only report up to 4MB for AU_SIZE, but
			     * will report the correct AU size in UHS_AU_SIZE.
			     */
		      
			    au_size = data[14] & 0x0f;
		      
			    if (au_size == 0)
			    {
				au_size = (data[10] & 0xf0) >> 4;
			    }
		      
			    *p_au_size = rfat_au_size_table[au_size];
			}
		    }
		}
	    }
	    while ((status == F_NO_ERROR) && retries);
	}

	if (status == F_NO_ERROR)
	{
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
	    retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
	    retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */

	    do
	    {
		status = rfat_disk_send_command(disk, SD_CMD_SEND_CID, 0, 0);

		if (status == F_NO_ERROR)
		{
		    status = rfat_disk_receive_data(disk, data, 16, &retries);

		    if ((status == F_NO_ERROR) && !retries)
		    {
			*p_serial = (((uint32_t)data[9]  << 24) |
				     ((uint32_t)data[10] << 16) |
				     ((uint32_t)data[11] <<  8) |
				     ((uint32_t)data[12] <<  0));
		    }
		}
	    } 
	    while ((status == F_NO_ERROR) && retries);
	}

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_read(rfat_disk_t *disk, uint32_t address, uint8_t *data)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
    unsigned int retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
    unsigned int retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

    if (status == F_NO_ERROR)
    {
	do
	{
	    status = rfat_disk_send_command(disk, SD_CMD_READ_SINGLE_BLOCK, (address << disk->shift), 0);

	    if (status == F_NO_ERROR)
	    {
		status = rfat_disk_receive_data(disk, data, RFAT_BLK_SIZE, &retries);

		if ((status == F_NO_ERROR) && !retries)
		{
		    RFAT_DISK_STATISTICS_COUNT(disk_read_single);
		}
	    }
	} 
	while ((status == F_NO_ERROR) && retries);

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_read_sequential(rfat_disk_t *disk, uint32_t address, uint32_t length, uint8_t *data)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
    unsigned int retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
    unsigned int retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READ_SEQUENTIAL, address);

    if (status == F_NO_ERROR)
    {
	do
	{
	    if (disk->state != RFAT_DISK_STATE_READ_SEQUENTIAL)
	    {
		status = rfat_disk_send_command(disk, SD_CMD_READ_MULTIPLE_BLOCK, (address << disk->shift), 0);

		if (status == F_NO_ERROR)
		{
		    disk->state = RFAT_DISK_STATE_READ_SEQUENTIAL;
		    disk->address = 0;
		    disk->count = 0;
		}
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		status = rfat_disk_receive_data(disk, data, RFAT_BLK_SIZE, &retries);

		if ((status == F_NO_ERROR) && !retries)
		{
		    RFAT_DISK_STATISTICS_COUNT(disk_read_sequential);

		    if (disk->address == address)
		    {
			RFAT_DISK_STATISTICS_COUNT(disk_read_coalesce);
		    }

		    address++;
		    length--;
			
		    data += RFAT_BLK_SIZE;

		    disk->address = address;
		    disk->count++;

		    /* A block has been sucessfully read, so the retry counter
		     * can be reset.
		     */
		    retries = RFAT_CONFIG_DISK_DATA_RETRIES;
		}
	    }
	} 
	while ((status == F_NO_ERROR) && (length != 0));

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_write(rfat_disk_t *disk, uint32_t address, const uint8_t *data)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
    unsigned int retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
    unsigned int retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

    if (status == F_NO_ERROR)
    {
	do
	{
	    status = rfat_disk_send_command(disk, SD_CMD_WRITE_SINGLE_BLOCK, (address << disk->shift), 0);
	    
	    if (status == F_NO_ERROR)
	    {
		/* After the CMD_WRITE_SINGLE_BLOCK there needs to be 8 clocks before sending
		 * the Start Block Token.
		 */
		RFAT_PORT_DISK_SPI_RECEIVE();
		    
		status = rfat_disk_send_data(disk, SD_START_WRITE_SINGLE_TOKEN, data, RFAT_BLK_SIZE, &retries);
	    }
	}
	while ((status == F_NO_ERROR) && retries);

	if (status == F_NO_ERROR)
	{
	    RFAT_DISK_STATISTICS_COUNT(disk_write_single);

	    status = rfat_disk_wait_ready(disk);
	    
	    if (status == F_NO_ERROR)
	    {
		status = rfat_disk_send_command(disk, SD_CMD_SEND_STATUS, 0, 1);
		
		if (status == F_NO_ERROR)
		{
		    if (disk->response[1] != 0x00)
		    {
			if (disk->response[1] & (SD_R2_CARD_IS_LOCKED | SD_R2_WP_ERASE_SKIP | SD_R2_EXECUTION_ERROR | SD_R2_CC_ERROR | SD_R2_ERASE_PARAM | SD_R2_OUT_OF_RANGE))
			{
			    status = F_ERR_ONDRIVE;
			}
			else if (disk->response[1] & SD_R2_WP_VIOLATION)
			{
			    status = F_ERR_WRITEPROTECT;
			}
			else
			{
			    /* CARD_ECC_FAILED */
			    status = F_ERR_INVALIDSECTOR;
			}
		    }
		}
	    }
	}

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_write_sequential(rfat_disk_t *disk, uint32_t address, uint32_t length, const uint8_t *data, volatile uint8_t *p_status)
{
    int status = F_NO_ERROR;
    int busy;
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
    unsigned int retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
    uint32_t retry_address = address;
#else /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
    unsigned int retries = 0;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_WRITE_SEQUENTIAL, address);

    if (status == F_NO_ERROR)
    {
	disk->p_status = p_status;

	do
	{
	    if (disk->state != RFAT_DISK_STATE_WRITE_SEQUENTIAL)
	    {
		if (status == F_NO_ERROR)
		{
		    status = rfat_disk_send_command(disk, SD_CMD_WRITE_MULTIPLE_BLOCK, (address << disk->shift), 0);

		    if (status == F_NO_ERROR)
		    {
			/* After the CMD_WRITE_MULTIPLE_BLOCK there needs to be 8 clocks before sending
			 * the Start Block Token.
			 */
			RFAT_PORT_DISK_SPI_RECEIVE();
			
			disk->state = RFAT_DISK_STATE_WRITE_SEQUENTIAL;
			disk->address = 0;
			disk->count = 0;
			
			busy = FALSE;
		    }
		}
	    }
	    else
	    {
		busy = TRUE;
	    }
		    
	    if (status == F_NO_ERROR)
	    {
		if (busy)
		{
		    status = rfat_disk_wait_ready(disk);
		}
			
		if (status == F_NO_ERROR)
		{
		    status = rfat_disk_send_data(disk, SD_START_WRITE_MULTIPLE_TOKEN, data, RFAT_BLK_SIZE, &retries);

		    if ((status == F_NO_ERROR) && !retries)
		    {
			RFAT_DISK_STATISTICS_COUNT(disk_write_sequential);
			    
			if (disk->address == address)
			{
			    RFAT_DISK_STATISTICS_COUNT(disk_write_coalesce);
			}
			    
			address++;
			length--;
			    
			data += RFAT_BLK_SIZE;
			    
			disk->address = address;
			disk->count++;

#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
			if (address >= retry_address)
			{
			    /* If this write is post the previous retry address,
			     * then reset the retry counter.
			     */
			    retries = RFAT_CONFIG_DISK_DATA_RETRIES +1;
			}
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
		    }
#if (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0)
		    else
		    {
			if (retries)
			{
			    /* CRC errors are retried. Hence one needs to get the number of sucessfully
			     * written block since the last CMD_WRITE_MULITPLE_BLOCK was issued.
			     */

			    uint8_t response[4];
			    uint32_t delta;
			    unsigned int retries2 = RFAT_CONFIG_DISK_DATA_RETRIES +1;

			    do
			    {
				status = rfat_disk_send_command(disk, SD_CMD_APP_CMD, 0, 0);
		
				if (status == F_NO_ERROR)
				{
				    status = rfat_disk_send_command(disk, SD_ACMD_SEND_NUM_WR_BLOCKS, 0, 0);
					
				    if (status == F_NO_ERROR)
				    {
					status = rfat_disk_receive_data(disk, response, 4, &retries2);
				    }
				}
			    }
			    while ((status == F_NO_ERROR) && retries2);
					    
			    if (status == F_NO_ERROR)
			    {
				delta = 
				    (disk->count -
				     (((uint32_t)response[0] << 24) |
				      ((uint32_t)response[1] << 16) |
				      ((uint32_t)response[2] <<  8) |
				      ((uint32_t)response[3] <<  0)));
						
				if ((address - delta) < retry_address)
				{
				    /* This here enforces that we make forward progress, and do
				     * not try to write data that is not available.
				     */

				    status = F_ERR_WRITE;
				}
				else
				{
				    address -= delta;
				    length += delta;
					    
				    data -= (delta << RFAT_BLK_SHIFT);
					    
				    retry_address = address;
				}
			    }
			}
		    }
#endif /* (RFAT_CONFIG_DISK_CRC == 1) && (RFAT_CONFIG_DISK_DATA_RETRIES != 0) */
		}
	    }
	}
	while ((status == F_NO_ERROR) && (length != 0));

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_sync(rfat_disk_t *disk, volatile uint8_t *p_status)
{
    int status = F_NO_ERROR;

    if ((disk->state == RFAT_DISK_STATE_WRITE_SEQUENTIAL) && ((p_status == NULL) || (p_status == disk->p_status)))
    {
	status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

	if (status == F_NO_ERROR)
	{
	    status = rfat_disk_unlock(disk, status);
	}
    }

    return status;
}

#else /* (RFAT_CONFIG_DISK_SIMULATE == 0) */

/********************************************************************************************************************************************/

#include <malloc.h>
#include <stdio.h>

static int rfat_disk_reset(rfat_disk_t *disk)
{
    int status = F_NO_ERROR;
    unsigned int type;

    type = (RFAT_CONFIG_DISK_SIMULATE_BLKCNT < 4209984) ? RFAT_DISK_TYPE_SDSC : RFAT_DISK_TYPE_SDHC;

    disk->state = RFAT_DISK_STATE_RESET;
    disk->type = type;
    disk->flags = 0;
    disk->shift = (type == RFAT_DISK_TYPE_SDHC) ? 0 : 9;
    disk->speed = 25000000;

    if (disk->image == NULL)
    {
	disk->image = (uint8_t*)malloc(RFAT_CONFIG_DISK_SIMULATE_BLKCNT * RFAT_BLK_SIZE);

	if (disk->image == NULL)
	{
	    status = F_ERR_INVALIDMEDIA;
	}
	else
	{
	    /* Simulate a compeltely erased device */
	    memset(disk->image, 0xff, RFAT_CONFIG_DISK_SIMULATE_BLKCNT * RFAT_BLK_SIZE);

	    disk->state = RFAT_DISK_STATE_READY;
	}
    }

    return status;
}

static int rfat_disk_lock(rfat_disk_t *disk, int state, uint32_t address)
{
    int status = F_NO_ERROR;

    /* "state" can be READY, READ, WRITE */

    if (disk->state == RFAT_DISK_STATE_RESET)
    {
        status = rfat_disk_reset(disk);
    }

    if (status == F_NO_ERROR)
    {
	if ((disk->state == state) && (disk->address == address))
	{
	    /* Continuation of a previous read/write ... */
	}
	else
	{
	    /* No special case, move the disk to RFAT_DISK_STATE_READY.
	     */
	    
	    if (disk->state == RFAT_DISK_STATE_READ_SEQUENTIAL)
	    {
#if (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1)
		printf("DISK_READ_STOP\n");
#endif /* (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1) */
	    }
	    
	    if (disk->state == RFAT_DISK_STATE_WRITE_SEQUENTIAL)
	    {
#if (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1)
		printf("DISK_WRITE_STOP\n");
#endif /* (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1) */
	    }
	    
	    disk->state = RFAT_DISK_STATE_READY;
	}
    }

    return status;
}

static int rfat_disk_unlock(rfat_disk_t *disk, int status)
{
    return status;
}

rfat_disk_t * rfat_disk_acquire(void)
{
    rfat_disk_t *disk = &rfat_disk;

    if (disk->state == RFAT_DISK_STATE_NONE)
    {
	disk->state = RFAT_DISK_STATE_RESET;

#if (RFAT_CONFIG_STATISTICS == 1)
	memset(&disk->statistics, 0, sizeof(disk->statistics));
#endif /* (RFAT_CONFIG_STATISTICS == 1) */
    }
    else
    {
	disk = NULL;
    }

    return disk;
}

int rfat_disk_release(rfat_disk_t *disk)
{
    int status = F_NO_ERROR;

    if (disk->state != RFAT_DISK_STATE_RESET)
    {
	status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

	if (status == F_NO_ERROR)
	{
	    status = rfat_disk_unlock(disk, status);
	}
    }

    disk->state = RFAT_DISK_STATE_NONE;

    return status;
}

int rfat_disk_info(rfat_disk_t *disk, bool *p_write_protected, uint32_t *p_block_count, uint32_t *p_au_size, uint32_t *p_serial)
{
    int status = F_NO_ERROR;

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

    if (status == F_NO_ERROR)
    {
	*p_write_protected = false;
	*p_block_count = RFAT_CONFIG_DISK_SIMULATE_BLKCNT;
	*p_au_size = 0;
	*p_serial = 0;

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_read(rfat_disk_t *disk, uint32_t address, uint8_t *data)
{
    int status = F_NO_ERROR;

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

    if (status == F_NO_ERROR)
    {
#if (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1)
        printf("DISK_READ %08x\n", address);
#endif /* (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1) */
	
	memcpy(data, disk->image + (RFAT_BLK_SIZE * address), RFAT_BLK_SIZE);

	RFAT_DISK_STATISTICS_COUNT(disk_read_single);

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_read_sequential(rfat_disk_t *disk, uint32_t address, uint32_t length, uint8_t *data)
{
    int status = F_NO_ERROR;

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READ_SEQUENTIAL, address);

    if (status == F_NO_ERROR)
    {
	if (disk->state != RFAT_DISK_STATE_READ_SEQUENTIAL)
	{
	    disk->state = RFAT_DISK_STATE_READ_SEQUENTIAL;
	    disk->address = 0;
	    disk->count = 0;
	}

#if (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1)
	printf("DISK_READ_SEQUENTIAL %08x, %d\n", address, length);
#endif /* (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1) */
	
	memcpy(data, disk->image + (RFAT_BLK_SIZE * address), (RFAT_BLK_SIZE * length));

	RFAT_DISK_STATISTICS_COUNT_N(disk_read_sequential, length);
	RFAT_DISK_STATISTICS_COUNT_N(disk_read_coalesce, ((disk->address == address) ? length : (length -1)));

	address += length;

	disk->address = address;
	disk->count += length;

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_write(rfat_disk_t *disk, uint32_t address, const uint8_t *data)
{
    int status = F_NO_ERROR;

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

    if (status == F_NO_ERROR)
    {
#if (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1)
	printf("DISK_WRITE %08x\n", address);
#endif /* (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1) */
	
	memcpy(disk->image + (RFAT_BLK_SIZE * address), data, RFAT_BLK_SIZE);
	
	RFAT_DISK_STATISTICS_COUNT(disk_write_single);

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_write_sequential(rfat_disk_t *disk, uint32_t address, uint32_t length, const uint8_t *data, volatile uint8_t *p_status)
{
    int status = F_NO_ERROR;

    status = rfat_disk_lock(disk, RFAT_DISK_STATE_WRITE_SEQUENTIAL, address);

    if (status == F_NO_ERROR)
    {
	if  (disk->state != RFAT_DISK_STATE_WRITE_SEQUENTIAL)
	{
	    disk->state = RFAT_DISK_STATE_WRITE_SEQUENTIAL;
	    disk->address = 0;
	    disk->count = 0;
	}

#if (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1)
	printf("DISK_WRITE_SEQUENTIAL %08x, %d\n", address, length);
#endif /* (RFAT_CONFIG_DISK_SIMULATE_TRACE == 1) */

	memcpy(disk->image + (RFAT_BLK_SIZE * address), data, (RFAT_BLK_SIZE * length));
	
	RFAT_DISK_STATISTICS_COUNT_N(disk_write_sequential, length);
	RFAT_DISK_STATISTICS_COUNT_N(disk_write_coalesce, ((disk->address == address) ? length : (length -1)));

	address += length;

	disk->address = address;
	disk->count += length;

	status = rfat_disk_unlock(disk, status);
    }

    return status;
}

int rfat_disk_sync(rfat_disk_t *disk, volatile uint8_t *p_status)
{
    int status = F_NO_ERROR;

    if ((disk->state == RFAT_DISK_STATE_WRITE_SEQUENTIAL) && ((p_status == NULL) || (p_status == disk->p_status)))
    {
	status = rfat_disk_lock(disk, RFAT_DISK_STATE_READY, 0);

	if (status == F_NO_ERROR)
	{
	    status = rfat_disk_unlock(disk, status);
	}
    }

    return status;
}

#endif /* (RFAT_CONFIG_DISK_SIMULATE == 0) */
