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

#include "TM4C123.h"

#define TARGET_IS_BLIZZARD_RA3
#define PART_TM4C123GH6PM

#include "BSP/TI/inc/hw_types.h"
#include "BSP/TI/inc/tm4c123gh6pm.h"
#include "BSP/TI/driverlib/rom.h"
#include "BSP/TI/driverlib/gpio.h"
#include "BSP/TI/driverlib/pin_map.h"
#include "BSP/TI/driverlib/ssi.h"
#include "BSP/TI/driverlib/sysctl.h"


#define TM4C123_SSI_FIFO_COUNT           8

#define TM4C123_SSI                      ((SSI0_Type*)(TM4C123_SSI_BASE))

#define TM4C123_SSI_MOSI_GPIO           ((GPIOA_Type*)TM4C123_SSI_MOSI_GPIO_BASE)
#define TM4C123_SSI_MOSI_READ()         ((TM4C123_SSI_MOSI_GPIO->DATA & TM4C123_SSI_MOSI_GPIO_PIN) == TM4C123_SSI_MOSI_GPIO_PIN)
#define TM4C123_SSI_MOSI_WRITE(_bit)    (((volatile uint32_t*)(TM4C123_SSI_MOSI_GPIO_BASE))[TM4C123_SSI_MOSI_GPIO_PIN] = ((_bit) ? 0xff : 0x00))

#define TM4C123_SDCARD_CS_GPIO           ((GPIOA_Type*)TM4C123_SDCARD_CS_GPIO_BASE)
#define TM4C123_SDCARD_CS_READ()         ((TM4C123_SDCARD_CS_GPIO->DATA & TM4C123_SDCARD_CS_GPIO_PIN) == TM4C123_SDCARD_CS_GPIO_PIN)
#define TM4C123_SDCARD_CS_WRITE(_bit)    (((volatile uint32_t*)(TM4C123_SDCARD_CS_GPIO_BASE))[TM4C123_SDCARD_CS_GPIO_PIN] = ((_bit) ? 0xff : 0x00))

#define TM4C123_SDCARD_CD_GPIO           ((GPIOA_Type*)TM4C123_SDCARD_CD_GPIO_BASE)
#define TM4C123_SDCARD_CD_READ()         ((TM4C123_SDCARD_CD_GPIO->DATA & TM4C123_SDCARD_CD_GPIO_PIN) == TM4C123_SDCARD_CD_GPIO_PIN)

#define TM4C123_SDCARD_WP_GPIO           ((GPIOA_Type*)TM4C123_SDCARD_WP_GPIO_BASE)
#define TM4C123_SDCARD_WP_READ()         ((TM4C123_SDCARD_WP_GPIO->DATA & TM4C123_SDCARD_WP_GPIO_PIN) == TM4C123_SDCARD_WP_GPIO_PIN)

#define TM4C123_DISK_LED_GPIO            ((GPIOA_Type*)TM4C123_DISK_LED_GPIO_BASE)
#define TM4C123_DISK_LED_WRITE(_bit)     (((volatile uint32_t*)(TM4C123_DISK_LED_GPIO_BASE))[TM4C123_DISK_LED_GPIO_PIN] = ((_bit) ? 0xff : 0x00))


static uint32_t tm4c123_disk_ssi_cr0;
static uint32_t tm4c123_disk_ssi_cpsr;

static uint32_t tm4c123_disk_time_stamp;
static uint32_t tm4c123_disk_time_scale;

void tm4c123_spi_initialize(void)
{
    ROM_SysCtlPeripheralEnable(TM4C123_SSI_PERIPH);

    ROM_SysCtlPeripheralEnable(TM4C123_SSI_SCLK_GPIO_PERIPH);
    ROM_GPIOPinTypeSSI(TM4C123_SSI_SCLK_GPIO_BASE, TM4C123_SSI_SCLK_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_SSI_SCLK_GPIO_BASE, TM4C123_SSI_SCLK_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    ROM_GPIOPinConfigure(TM4C123_SSI_SCLK_GPIO_CONFIG);

    ROM_SysCtlPeripheralEnable(TM4C123_SSI_MISO_GPIO_PERIPH);
    ROM_GPIOPinTypeSSI(TM4C123_SSI_MISO_GPIO_BASE, TM4C123_SSI_MISO_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_SSI_MISO_GPIO_BASE,  TM4C123_SSI_MISO_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU);
    ROM_GPIOPinConfigure(TM4C123_SSI_MISO_GPIO_CONFIG);

    ROM_SysCtlPeripheralEnable(TM4C123_SSI_MOSI_GPIO_PERIPH);
    ROM_GPIOPinTypeSSI(TM4C123_SSI_MOSI_GPIO_BASE, TM4C123_SSI_MOSI_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_SSI_MOSI_GPIO_BASE, TM4C123_SSI_MOSI_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    ROM_GPIOPinConfigure(TM4C123_SSI_MOSI_GPIO_CONFIG);

#if !defined(TM4C123_SDCARD_CD_GPIO_PERIPH)
    /* If there is no CD pin, the pullup on the CS pin is used to 
     * detect a SDCARD.
     */
    ROM_SysCtlPeripheralEnable(TM4C123_SDCARD_CS_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOInput(TM4C123_SDCARD_CS_GPIO_BASE, TM4C123_SDCARD_CS_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_SDCARD_CS_GPIO_BASE, TM4C123_SDCARD_CS_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
#else /* !TM4C123_SDCARD_CD_GPIO_PERIPH */
    /* If there is a CD pin, then CS is simply an output, while CD is a input
     * with a pullup. When a card is present, CD is shorted to GND, otherwise
     * it's left open, which means pulled high.
     */
    ROM_SysCtlPeripheralEnable(TM4C123_SDCARD_CS_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOOutput(TM4C123_SDCARD_CS_GPIO_BASE, TM4C123_SDCARD_CS_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_SDCARD_CS_GPIO_BASE, TM4C123_SDCARD_CS_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);

    ROM_SysCtlPeripheralEnable(TM4C123_SDCARD_CD_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOInput(TM4C123_SDCARD_CD_GPIO_BASE, TM4C123_SDCARD_CD_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_SDCARD_CD_GPIO_BASE, TM4C123_SDCARD_CD_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU);
#endif /* !TM4C123_SDCARD_CD_GPIO_PERIPH */

#if defined(TM4C123_SDCARD_WP_GPIO_PERIPH)
    /* If a card is write protected (or locked), then the WP pin gets disconnected
     * from GND.
     */
    ROM_SysCtlPeripheralEnable(TM4C123_SDCARD_WP_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOInput(TM4C123_SDCARD_WP_GPIO_BASE, TM4C123_SDCARD_WP_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_SDCARD_WP_GPIO_BASE, TM4C123_SDCARD_WP_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU);
#endif /* TM4C123_SDCARD_WP_GPIO_PERIPH */

#if defined(TM4C123_DISK_LED_GPIO_PERIPH)
    ROM_SysCtlPeripheralEnable(TM4C123_DISK_LED_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOOutput(TM4C123_DISK_LED_GPIO_BASE, TM4C123_DISK_LED_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_DISK_LED_GPIO_BASE, TM4C123_DISK_LED_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    ROM_GPIOPinWrite(TM4C123_DISK_LED_GPIO_BASE, TM4C123_DISK_LED_GPIO_PIN, 0);
#endif /* TM4C123_DISK_LED_GPIO_PERIPH */

#if defined(TM4C123_EEPROM_CS_GPIO_PERIPH)
    ROM_SysCtlPeripheralEnable(TM4C123_EEPROM_CS_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOOutput(TM4C123_EEPROM_CS_GPIO_BASE, TM4C123_EEPROM_CS_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_EEPROM_CS_GPIO_BASE, TM4C123_EEPROM_CS_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    ROM_GPIOPinWrite(TM4C123_EEPROM_CS_GPIO_BASE, TM4C123_EEPROM_CS_GPIO_PIN, TM4C123_EEPROM_CS_GPIO_PIN);
#endif /* TM4C123_EEPROM_CS_GPIO_PERIPH */

#if defined(TM4C123_TFT_CS_GPIO_PERIPH)
    ROM_SysCtlPeripheralEnable(TM4C123_TFT_CS_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOOutput(TM4C123_TFT_CS_GPIO_BASE, TM4C123_TFT_CS_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_TFT_CS_GPIO_BASE, TM4C123_TFT_CS_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    ROM_GPIOPinWrite(TM4C123_TFT_CS_GPIO_BASE, TM4C123_TFT_CS_GPIO_PIN, TM4C123_TFT_CS_GPIO_PIN);
#endif /* TM4C123_TFT_CS_GPIO_PERIPH */

#if defined(TM4C123_TFT_DC_GPIO_PERIPH)
    ROM_SysCtlPeripheralEnable(TM4C123_TFT_DC_GPIO_PERIPH);
    ROM_GPIOPinTypeGPIOOutput(TM4C123_TFT_DC_GPIO_BASE, TM4C123_TFT_DC_GPIO_PIN);
    ROM_GPIOPadConfigSet(TM4C123_TFT_DC_GPIO_BASE, TM4C123_TFT_DC_GPIO_PIN, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    ROM_GPIOPinWrite(TM4C123_TFT_DC_GPIO_BASE, TM4C123_TFT_DC_GPIO_PIN, TM4C123_TFT_DC_GPIO_PIN);
#endif /* TM4C123_TFT_CS_GPIO_PERIPH */

#if defined(RFAT_PORT_DISK_TIME_START)
    /* Enable CYCCNT for busy wait loops */

    ITM->TCR |= ITM_TCR_DWTENA_Msk;

    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
    {
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }

    tm4c123_disk_time_scale = SystemCoreClock / 1000;
#endif /* (RFAT_PORT_DISK_TIME_START) */
}


#if defined(RFAT_PORT_DISK_TIME_START)

void tm4c123_disk_time_start(void)
{
    tm4c123_disk_time_stamp = DWT->CYCCNT;
}

bool tm4c123_disk_time_elapsed(uint32_t time)
{
    return (((uint32_t)(DWT->CYCCNT - tm4c123_disk_time_stamp)) > (time * tm4c123_disk_time_scale));
}

#endif /* RFAT_PORT_DISK_TIME_START */


#if defined(TM4C123_DISK_LED_GPIO_PERIPH)

int tm4c123_disk_lock(void)
{
    TM4C123_DISK_LED_WRITE(1);
    
    return F_NO_ERROR;
}


void tm4c123_disk_unlock(void)
{
    TM4C123_DISK_LED_WRITE(0);
}

#endif /* TM4C123_DISK_LED_GPIO_PERIPH */


bool tm4c123_disk_present(void)
{
    bool present;

#if defined(TM4C123_SDCARD_CD_GPIO_PERIPH)
#if (TM4C123_SDCARD_CD_GPIO_INVERTED == 1)
    present = !TM4C123_SDCARD_CD_READ();
#else /* (TM4C123_SDCARD_CD_GPIO_INVERTED == 1) */
    present = TM4C123_SDCARD_CD_READ();
#endif /* (TM4C123_SDCARD_CD_GPIO_INVERTED == 1) */

#else /* TM4C123_SDCARD_CD_GPIO_PERIPH */
    /* CS is pulled high if a SDCARD is present.
     */

    present = TM4C123_SDCARD_CS_READ();
#endif /* TM4C123_SDCARD_CD_GPIO_PERIPH */

    return present;
}


#if defined(TM4C123_SDCARD_WP_GPIO_PERIPH)

bool tm4c123_disk_write_protected(void)
{
    bool write_protected;

#if (TM4C123_SDCARD_WP_GPIO_INVERTED == 1)
    write_protected = !TM4C123_SDCARD_WP_READ();
#else /* (TM4C123_SDCARD_WP_GPIO_INVERTED == 1) */
    write_protected = TM4C123_SDCARD_WP_READ();
#endif /* (TM4C123_SDCARD_WP_GPIO_INVERTED == 1) */

    return write_protected;
}

#endif /* TM4C123_SDCARD_WP_GPIO_PERIPH */


/* 
 * tm4c123_disk_mode(int mode)
 */

uint32_t tm4c123_disk_mode(int mode)
{
    unsigned int n;
    uint32_t ssiclock, speed, cpsdvsr, scr;

    if (mode == RFAT_DISK_MODE_NONE)
    {
	speed = 0;

#if !defined(TM4C123_SDCARD_CD_GPIO_PERIPH)
	/* Switch SDCARD_CS to be input */
	TM4C123_SDCARD_CS_GPIO->DIR &= ~TM4C123_SDCARD_CS_GPIO_PIN;
#endif /* !TM4C123_SDCARD_CD_GPIO_PERIPH */
    }
    else
    {
	if (mode == RFAT_DISK_MODE_IDENTIFY)
	{
	    speed = 400000;

#if !defined(TM4C123_SDCARD_CD_GPIO_PERIPH)
	    /* Switch SDCARD_CS to be output */
	    TM4C123_SDCARD_CS_GPIO->DIR |= TM4C123_SDCARD_CS_GPIO_PIN;
	    TM4C123_SDCARD_CS_WRITE(1);
#endif /* !TM4C123_SDCARD_CD_GPIO_PERIPH */
	}
	else
	{
	    speed = TM4C123_SSI_SPEED_DATA_TRANSFER;

	    tm4c123_disk_deselect();
	}

	/* 
	 * "speed" cannot be above ssiclock / 2 !
	 *
	 *     speed = (ssiclock / (cpsdvsr * (1 + scr));
	 *
	 * The idea is now to compute the minimum "cpsdvsr" that does 
	 * not overflow "scr" (0..255):
	 *
	 *     cpsdvsr * (1 + scr) = ssiclock / speed;
	 *     cpsdvsr = max(2, ((ssiclock / speed) / (1 + scr_max) +1) & ~1)) = max(2, ((ssiclock / speed) / 256 + 1) & ~1);
	 *
	 * With that a "scr" can be computed:
	 *
	 *     (1 + scr) = (ssiclock / speed) / cpsdvsr;
	 *     scr = (ssiclock / speed) / cpsdvsr -1;
	 *
	 * However this is all pretty pointless. Lets assume we have a 50MHz ssiclock:
	 *
	 *     speed  c s
	 *  25000000  2 0
	 *  12500000  2 1
	 *   8333333  2 2
	 *   6250000  2 3
	 *   5000000  2 4
	 *    400000  2 62
	 */

	ssiclock = SystemCoreClock;

	cpsdvsr = ((ssiclock / speed) / 256 +1) & ~1;
	
	if (cpsdvsr == 0)
	{
	    cpsdvsr = 2;
	}
	
	scr = (ssiclock / speed + (cpsdvsr -1)) / cpsdvsr -1;
	
	tm4c123_disk_ssi_cr0  = ((scr << 8) | SSI_CR0_SPH | SSI_CR0_SPO | SSI_CR0_FRF_MOTO | SSI_CR0_DSS_8);
	tm4c123_disk_ssi_cpsr = cpsdvsr;
	
	while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }
	
	TM4C123_SSI->CR1  = 0;
	TM4C123_SSI->CR0  = tm4c123_disk_ssi_cr0;
	TM4C123_SSI->CPSR = tm4c123_disk_ssi_cpsr;
	TM4C123_SSI->CR1  = SSI_CR1_SSE;

	speed = (ssiclock / (cpsdvsr * (1 + scr)));

	if (mode == RFAT_DISK_MODE_IDENTIFY)
	{
	    TM4C123_SSI_MOSI_GPIO->DIR |= TM4C123_SDCARD_CS_GPIO_PIN;
	    TM4C123_SSI_MOSI_WRITE(1);

	    /* Here CS/MOSI are driven both to H.
	     *
	     * Specs says to issue 74 clock cycles in SPI mode while CS/MOSI are H,
	     * so simply send 10 bytes over the clock line.
	     */
    
	    for (n = 0; n < 10; n++)
	    {
		tm4c123_disk_receive();
	    }
    
	    while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }

	    TM4C123_SSI_MOSI_GPIO->DIR &= ~TM4C123_SDCARD_CS_GPIO_PIN;
	}

	tm4c123_disk_select();
    }
    
    return speed;
}


void tm4c123_disk_select(void)
{
    /* Setup/Enable SPI port for shared access.
     */
    TM4C123_SSI->CR0  = tm4c123_disk_ssi_cr0;
    TM4C123_SSI->CPSR = tm4c123_disk_ssi_cpsr;
    TM4C123_SSI->CR1  = SSI_CR1_SSE;

    /* CS output, drive CS to L */
    TM4C123_SDCARD_CS_WRITE(0);

    /* The card will not drive DO for one more clock
     * after CS goes L, but will accept data right away.
     * The first thing after a select will be always
     * either a command (send_command), or a "Stop Token".
     * In both cases there will be a byte over the
     * bus, and hence DO will be stable.
     */
}


void tm4c123_disk_deselect(void)
{
    /* CS is output, drive CS to H */

    while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }
	
    TM4C123_SDCARD_CS_WRITE(1);

    /* The card drives the DO line at least one more
     * clock cycle after CS goes H. Hence send
     * one extra byte over the bus, if we get
     * here while SSI was enabled.
     */
    
    tm4c123_disk_send(0x00);

    while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }

    /* Disable SPI port for shared access
     */
    TM4C123_SSI->CR1 = 0;
}


/*
 * tm4c123_disk_send(uint8_t data)
 *
 * Send one byte, discard read data. The assumption
 * is that at this point both TX and RX FIFOs are
 * empty, so that a write is always possible. On
 * the read part there is a wait for the RX FIFO to
 * become not empty.
 */

void tm4c123_disk_send(uint8_t data)
{
  
    TM4C123_SSI->DR = data;

    while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }

    TM4C123_SSI->DR;
}


/*
 * tm4c123_disk_receive()
 *
 * Receive one byte, send 0xff as data. The assumption
 * is that at this point both TX and RX FIFOs are
 * empty, so that a write is always possible. On
 * the read part there is a wait for the RX FIFO to
 * become not empty.
 */

uint8_t tm4c123_disk_receive(void)
{
    TM4C123_SSI->DR = 0xff;

    while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }

    return TM4C123_SSI->DR;
}


/*
 * tm4c123_disk_send_block(const uint8_t *data)
 */

__attribute__((optimize("-O3"))) void tm4c123_disk_send_block(const uint8_t *data)
{
    unsigned int n;
    uint8_t data_l, data_h;
    uint32_t data16, crc16;

    crc16 = 0;

    /*
     * Idea is to stuff first up data into the TX FIFO till it's full
     * (or better said till there ae no more splots in the RX FIFO).
     * Then wait for at least one item in the RX FIFO to read it back,
     * and refill the TX FIFO. At the end, the RX FIFO is drained.
     */

    while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }

    TM4C123_SSI->CR1 = 0;
    TM4C123_SSI->CR0 = (tm4c123_disk_ssi_cr0 & ~SSI_CR0_DSS_M) | SSI_CR0_DSS_16;
    TM4C123_SSI->CR1 = SSI_CR1_SSE;
    
    for (n = 0; n < TM4C123_SSI_FIFO_COUNT; n++)
    {
	data_h = *data++;
	data_l = *data++;

#if (RFAT_CONFIG_DISK_CRC == 1)
	RFAT_UPDATE_CRC16(crc16, data_h);
	RFAT_UPDATE_CRC16(crc16, data_l);
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */
	
	data16 = (data_h << 8) | data_l;

	TM4C123_SSI->DR = data16;
    }
    
    for (n = TM4C123_SSI_FIFO_COUNT; n < (RFAT_BLK_SIZE / 2); n++)
    {
	data_h = *data++;
	data_l = *data++;

#if (RFAT_CONFIG_DISK_CRC == 1)
	RFAT_UPDATE_CRC16(crc16, data_h);
	RFAT_UPDATE_CRC16(crc16, data_l);
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */
	
	data16 = (data_h << 8) | data_l;

	while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }
        
	TM4C123_SSI->DR;
	TM4C123_SSI->DR = data16;
    }
    
    while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }
    
    TM4C123_SSI->DR;
    TM4C123_SSI->DR = crc16;
    
    for (n = 0; n < TM4C123_SSI_FIFO_COUNT; n++)
    {
	while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }
        
	TM4C123_SSI->DR;
    }

    while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }

    TM4C123_SSI->CR1 = 0;
    TM4C123_SSI->CR0 = (tm4c123_disk_ssi_cr0 & ~SSI_CR0_DSS_M) | SSI_CR0_DSS_8;
    TM4C123_SSI->CR1 = SSI_CR1_SSE;
}


/*
 * tm4c123_disk_receive_block(const uint8_t *data)
 *
 * Returns 0 on success, and non-zero on a CRC error.
 */

__attribute__((optimize("-O3"))) uint32_t tm4c123_disk_receive_block(uint8_t *data)
{
    unsigned int n;
    uint8_t data_l, data_h;
    uint32_t data16, crc16;

    crc16 = 0;

    /*
     * Idea is to stuff first up data into the TX FIFO till it's full
     * (or better said till there ae no more splots in the RX FIFO).
     * Then wait for at least one item in the RX FIFO to read it back,
     * and refill the TX FIFO. At the end, the RX FIFO is drained.
     */

    while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }

    TM4C123_SSI->CR1 = 0;
    TM4C123_SSI->CR0 = (tm4c123_disk_ssi_cr0 & ~SSI_CR0_DSS_M) | SSI_CR0_DSS_16;
    TM4C123_SSI->CR1 = SSI_CR1_SSE;
    
    for (n = 0; n < TM4C123_SSI_FIFO_COUNT; n++)
    {
	TM4C123_SSI->DR = 0xffff;
    }
    
    for (n = 0; n < (((RFAT_BLK_SIZE + 2) / 2) - TM4C123_SSI_FIFO_COUNT); n++)
    {
	while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }
	
	data16 = TM4C123_SSI->DR;
	TM4C123_SSI->DR = 0xffff;

	data_h = data16 >> 8;
	data_l = data16;

#if (RFAT_CONFIG_DISK_CRC == 1)
	RFAT_UPDATE_CRC16(crc16, data_h);
	RFAT_UPDATE_CRC16(crc16, data_l);
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

	*data++ = data_h;
	*data++ = data_l;
    }
    
    for (; n < (RFAT_BLK_SIZE / 2); n++)
    {
	while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }
	
	data16 = TM4C123_SSI->DR;

	data_h = data16 >> 8;
	data_l = data16;

#if (RFAT_CONFIG_DISK_CRC == 1)
	RFAT_UPDATE_CRC16(crc16, data_h);
	RFAT_UPDATE_CRC16(crc16, data_l);
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

	*data++ = data_h;
	*data++ = data_l;
    }
    
    while (!(TM4C123_SSI->SR & SSI_SR_RNE)) { continue; }
    
#if (RFAT_CONFIG_DISK_CRC == 1)
    crc16 ^= (TM4C123_SSI->DR & 0xffff);
#else /* (RFAT_CONFIG_DISK_CRC == 1) */
    TM4C123_SSI->DR;
#endif /* (RFAT_CONFIG_DISK_CRC == 1) */

    while (TM4C123_SSI->SR & SSI_SR_BSY) { continue; }

    TM4C123_SSI->CR1 = 0;
    TM4C123_SSI->CR0 = (tm4c123_disk_ssi_cr0 & ~SSI_CR0_DSS_M) | SSI_CR0_DSS_8;
    TM4C123_SSI->CR1 = SSI_CR1_SSE;

    return crc16;
}
