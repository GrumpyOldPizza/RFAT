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

#if !defined(_TM4C123_DISK_H)
#define _TM4C123_DISK_H

#include <stdbool.h>
#include <stdint.h>

/*
 * PB4      SCLK   SSI2CLK
 * PB6      MISO   SSI2RX
 * PB7      MOSI   SSI2TX
 *
 * PA3      CS (EEPROM)
 * PA4      CS (SDCARD)
 * PB5      CS (TFT)
 * PE5      DC (TFT)
 */

#define TM4C123_SSI_SPEED_DATA_TRANSFER  20000000

#define TM4C123_SSI_BASE                 SSI2_BASE
#define TM4C123_SSI_PERIPH               SYSCTL_PERIPH_SSI2

#define TM4C123_SSI_SCLK_GPIO_BASE       GPIOB_BASE
#define TM4C123_SSI_SCLK_GPIO_PERIPH     SYSCTL_PERIPH_GPIOB
#define TM4C123_SSI_SCLK_GPIO_CONFIG     GPIO_PB4_SSI2CLK
#define TM4C123_SSI_SCLK_GPIO_PIN        GPIO_PIN_4

#define TM4C123_SSI_MISO_GPIO_BASE       GPIOB_BASE
#define TM4C123_SSI_MISO_GPIO_PERIPH     SYSCTL_PERIPH_GPIOB
#define TM4C123_SSI_MISO_GPIO_CONFIG     GPIO_PB6_SSI2RX
#define TM4C123_SSI_MISO_GPIO_PIN        GPIO_PIN_6

#define TM4C123_SSI_MOSI_GPIO_BASE       GPIOB_BASE
#define TM4C123_SSI_MOSI_GPIO_PERIPH     SYSCTL_PERIPH_GPIOB
#define TM4C123_SSI_MOSI_GPIO_CONFIG     GPIO_PB7_SSI2TX
#define TM4C123_SSI_MOSI_GPIO_PIN        GPIO_PIN_7

#define TM4C123_SDCARD_CS_GPIO_BASE      GPIOA_BASE
#define TM4C123_SDCARD_CS_GPIO_PERIPH    SYSCTL_PERIPH_GPIOA
#define TM4C123_SDCARD_CS_GPIO_PIN       GPIO_PIN_4

/* Only define if CD is present for SDCARD socket */
// #define TM4C123_SDCARD_CD_GPIO_BASE      GPIOA_BASE
// #define TM4C123_SDCARD_CD_GPIO_PERIPH    SYSCTL_PERIPH_GPIOA
// #define TM4C123_SDCARD_CD_GPIO_PIN       GPIO_PIN_2
// #define TM4C123_SDCARD_CD_GPIO_INVERTED  1

/* Only define if WP is present for SDCARD socket */
// #define TM4C123_SDCARD_WP_GPIO_BASE      GPIOA_BASE
// #define TM4C123_SDCARD_WP_GPIO_PERIPH    SYSCTL_PERIPH_GPIOA
// #define TM4C123_SDCARD_WP_GPIO_PIN       GPIO_PIN_1
// #define TM4C123_SDCARD_WP_GPIO_INVERTED  0

/* Only define if LED is present for card activity */
#define TM4C123_DISK_LED_GPIO_BASE       GPIOF_BASE
#define TM4C123_DISK_LED_GPIO_PERIPH     SYSCTL_PERIPH_GPIOF
#define TM4C123_DISK_LED_GPIO_PIN        GPIO_PIN_1

/* Only define if EEPROM on the same SPI bus */
#define TM4C123_EEPROM_CS_GPIO_BASE      GPIOA_BASE
#define TM4C123_EEPROM_CS_GPIO_PERIPH    SYSCTL_PERIPH_GPIOA
#define TM4C123_EEPROM_CS_GPIO_PIN       GPIO_PIN_3

/* Only define if TFT on the same SPI bus */
#define TM4C123_TFT_CS_GPIO_BASE         GPIOB_BASE
#define TM4C123_TFT_CS_GPIO_PERIPH       SYSCTL_PERIPH_GPIOB
#define TM4C123_TFT_CS_GPIO_PIN          GPIO_PIN_5

/* Only define if TFT on the same SPI bus */
#define TM4C123_TFT_DC_GPIO_BASE         GPIOE_BASE
#define TM4C123_TFT_DC_GPIO_PERIPH       SYSCTL_PERIPH_GPIOE
#define TM4C123_TFT_DC_GPIO_PIN          GPIO_PIN_5


extern void tm4c123_spi_initialize(void);

extern void tm4c123_disk_time_start(void);
extern bool tm4c123_disk_time_elapsed(uint32_t time);

#define RFAT_PORT_DISK_TIME_START()              tm4c123_disk_time_start()
#define RFAT_PORT_DISK_TIME_ELAPSED(_time)       tm4c123_disk_time_elapsed((_time))

#if defined(TM4C123_DISK_LED_GPIO_PERIPH)

extern int  tm4c123_disk_lock(void);
extern void tm4c123_disk_unlock(void);

#define RFAT_PORT_DISK_LOCK()                    tm4c123_disk_lock()
#define RFAT_PORT_DISK_UNLOCK()                  tm4c123_disk_unlock()

#endif /* TM4C123_DISK_LED_GPIO_PERIPH */

#define RFAT_PORT_DISK_SPI_PRESENT()             tm4c123_disk_present()

extern bool     tm4c123_disk_present(void);

#if defined(TM4C123_SDCARD_WP_PERIPH)

#define RFAT_PORT_DISK_SPI_WRITE_PROTECTED()     tm4c123_disk_write_protected()

extern bool tm4c123_disk_write_protected(void);

#endif /* TM4C123_SDCARD_WP_PERIPH */

extern uint32_t tm4c123_disk_mode(int mode);
extern void     tm4c123_disk_select(void);
extern void     tm4c123_disk_deselect(void);
extern void     tm4c123_disk_send(uint8_t data);
extern uint8_t  tm4c123_disk_receive(void);
extern void     tm4c123_disk_send_block(const uint8_t *data);
extern uint32_t tm4c123_disk_receive_block(uint8_t *data);

#define RFAT_PORT_DISK_SPI_MODE(_mode)           tm4c123_disk_mode((_mode))
#define RFAT_PORT_DISK_SPI_SELECT()              tm4c123_disk_select()
#define RFAT_PORT_DISK_SPI_DESELECT()            tm4c123_disk_deselect()
#define RFAT_PORT_DISK_SPI_SEND(_data)           tm4c123_disk_send((_data))
#define RFAT_PORT_DISK_SPI_RECEIVE()             tm4c123_disk_receive()
#define RFAT_PORT_DISK_SPI_SEND_BLOCK(_data)     tm4c123_disk_send_block((_data))
#define RFAT_PORT_DISK_SPI_RECEIVE_BLOCK(_data)  tm4c123_disk_receive_block((_data))

#endif /* _TM4C123_DISK_H */
