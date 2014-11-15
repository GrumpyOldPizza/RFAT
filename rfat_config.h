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

#if !defined(_RFAT_CONFIG_h)
#define _RFAT_CONFIG_h

#define RFAT_VERSION_MAJOR                     1
#define RFAT_VERSION_MINOR                     0
#define RFAT_VERSION_BUILD                     67
#define RFAT_VERSION_STRING                    "1.0.67"

#define RFAT_CONFIG_MAX_FILES                  1
#define RFAT_CONFIG_FAT12_SUPPORTED            0
#define RFAT_CONFIG_VFAT_SUPPORTED             0
#define RFAT_CONFIG_UTF8_SUPPORTED             0
#define RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED 0
#define RFAT_CONFIG_CONTIGUOUS_SUPPORTED       1
#define RFAT_CONFIG_SEQUENTIAL_SUPPORTED       1
#define RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED    0
#define RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED     0
#define RFAT_CONFIG_FSINFO_SUPPORTED           0
#define RFAT_CONFIG_2NDFAT_SUPPORTED           1


#define RFAT_CONFIG_FAT_CACHE_ENTRIES          1
#define RFAT_CONFIG_DATA_CACHE_ENTRIES         0
#define RFAT_CONFIG_FILE_DATA_CACHE            0
#define RFAT_CONFIG_CLUSTER_CACHE_ENTRIES      0
#define RFAT_CONFIG_META_DATA_RETRIES          3
#define RFAT_CONFIG_DISK_CRC                   1
#define RFAT_CONFIG_DISK_COMMAND_RETRIES       3
#define RFAT_CONFIG_DISK_DATA_RETRIES          3

#define RFAT_CONFIG_STATISTICS                 0
#define RFAT_CONFIG_DISK_SIMULATE              0
#define RFAT_CONFIG_DISK_SIMULATE_BLKCNT       (unsigned long)(65536 * 64)
#define RFAT_CONFIG_DISK_SIMULATE_TRACE        1

#endif /* _RFAT_CONFIG_h */
