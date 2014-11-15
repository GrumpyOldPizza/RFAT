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

#include "rfat_core.h"
#include "rfat_port.h"

static rfat_volume_t rfat_volume;

static uint32_t rfat_cache[(1 +
			    RFAT_CONFIG_FAT_CACHE_ENTRIES +
			    ((RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) ? 1 : 0) +
			    (((RFAT_CONFIG_FILE_DATA_CACHE == 0) ? 1 : RFAT_CONFIG_MAX_FILES) * RFAT_CONFIG_DATA_CACHE_ENTRIES))
			   * (RFAT_BLK_SIZE / sizeof(uint32_t))];

static const char rfat_dirname_dot[11]    = ".          ";
static const char rfat_dirname_dotdot[11] = "..         ";

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)

static const uint8_t rfat_path_ldir_name_table[13] = {
    offsetof(rfat_ldir_t, ldir_name_1[0]),
    offsetof(rfat_ldir_t, ldir_name_1[2]),
    offsetof(rfat_ldir_t, ldir_name_1[4]),
    offsetof(rfat_ldir_t, ldir_name_1[6]),
    offsetof(rfat_ldir_t, ldir_name_1[8]),
    offsetof(rfat_ldir_t, ldir_name_2[0]),
    offsetof(rfat_ldir_t, ldir_name_2[2]),
    offsetof(rfat_ldir_t, ldir_name_2[4]),
    offsetof(rfat_ldir_t, ldir_name_2[6]),
    offsetof(rfat_ldir_t, ldir_name_2[8]),
    offsetof(rfat_ldir_t, ldir_name_2[10]),
    offsetof(rfat_ldir_t, ldir_name_3[0]),
    offsetof(rfat_ldir_t, ldir_name_3[2]),
};

#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */


static int rfat_volume_init(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;
    uint8_t *cache;
#if (RFAT_CONFIG_FILE_DATA_CACHE == 1)
#if (RFAT_CONFIG_MAX_FILES != 1)
    rfat_file_t *file_s, *file_e;
#endif /* (RFAT_CONFIG_MAX_FILES != 1) */
#endif /* (RFAT_CONFIG_FILE_DATA_CACHE == 1) */

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
    /* In order not to mess up rfat_volume_t too much, the rfat_boot_t.bpblog struct is aliased to
     * rfat_volume_t. The asserts below ensure that the struct elements don't move and mirror
     * the location relative to rfat_boot_t.
     */
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_map_flags) >= offsetof(rfat_boot_t, bpb40.bs_reserved_1));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_map_flags) >= offsetof(rfat_boot_t, bpb71.bs_reserved_2));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.bs_trail_sig) == 510);

    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_map_flags) == (offsetof(rfat_volume_t, map_flags) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_map_entries) == (offsetof(rfat_volume_t, map_entries) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_map_table) == (offsetof(rfat_volume_t, map_table) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_map_blkno) == (offsetof(rfat_volume_t, map_blkno) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_free_clscnt) == (offsetof(rfat_volume_t, free_clscnt) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_next_clsno) == (offsetof(rfat_volume_t, next_clsno) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_dir) == (offsetof(rfat_volume_t, dir) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_dir_flags) == (offsetof(rfat_volume_t, dir_flags) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_dir_entries) == (offsetof(rfat_volume_t, dir_entries) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_dir_index) == (offsetof(rfat_volume_t, dir_index) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_dir_clsno) == (offsetof(rfat_volume_t, dir_clsno) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_dot_clsno) == (offsetof(rfat_volume_t, dot_clsno) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_del_clsno) == (offsetof(rfat_volume_t, del_clsno) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_del_index) == (offsetof(rfat_volume_t, del_index) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_del_entries) == (offsetof(rfat_volume_t, del_entries) - offsetof(rfat_volume_t, bs_data)));

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_lfn_count) == (offsetof(rfat_volume_t, lfn_count) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_lfn_blkno) == (offsetof(rfat_volume_t, lfn_blkno) - offsetof(rfat_volume_t, bs_data)));
#if (RFAT_CONFIG_UTF8_SUPPORTED == 0)
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.log_lfn_name) == (offsetof(rfat_volume_t, lfn_name) - offsetof(rfat_volume_t, bs_data)));
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.bs_trail_sig) == (offsetof(rfat_volume_t, bs_trail_sig) - offsetof(rfat_volume_t, bs_data)));
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 0) */
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.bs_trail_sig) == (offsetof(rfat_volume_t, bs_trail_sig) - offsetof(rfat_volume_t, lfn_name)));
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 0) */
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    RFAT_CT_ASSERT(offsetof(rfat_boot_t, bpblog.bs_trail_sig) == (offsetof(rfat_volume_t, bs_trail_sig) - offsetof(rfat_volume_t, bs_data)));
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */


    volume->disk = rfat_disk_acquire();

    if (volume->disk)
    {
        volume->state = RFAT_VOLUME_STATE_INITIALIZED;
        volume->flags = 0;

        cache = (uint8_t*)&rfat_cache[0];

        volume->dir_cache.data = cache;
        cache += RFAT_BLK_SIZE;

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
        volume->map_cache.data = cache;
        cache += RFAT_BLK_SIZE;
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

#if (RFAT_CONFIG_FAT_CACHE_ENTRIES != 0)
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1)
        volume->fat_cache.data = cache;
        cache += RFAT_BLK_SIZE;
#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
        volume->fat_cache[0].data = cache;
        cache += RFAT_BLK_SIZE;
        volume->fat_cache[1].data = cache;
        cache += RFAT_BLK_SIZE;
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES != 0) */

#if (RFAT_CONFIG_DATA_CACHE_ENTRIES != 0)
#if (RFAT_CONFIG_FILE_DATA_CACHE == 0)
        volume->data_cache.data = cache;
        cache += RFAT_BLK_SIZE;
#else /* (RFAT_CONFIG_FILE_DATA_CACHE == 0) */
#if (RFAT_CONFIG_MAX_FILES == 1)
        volume->file_table[0].data_cache.data = cache;
#else /* (RFAT_CONFIG_MAX_FILES == 1) */
        file_s = &volume->file_table[0];
	file_e = &volume->file_table[RFAT_CONFIG_MAX_FILES];

	do
	{
            file_s->data_cache.data = cache;
            cache += RFAT_BLK_SIZE;

	    file_s++;
        }
	while (file_s < file_e);
#endif /* (RFAT_CONFIG_MAX_FILES == 1) */
#endif /* (RFAT_CONFIG_FILE_DATA_CACHE == 0) */
#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES != 0) */
    }
    else
    {
	status = F_ERR_INITFUNC;
    }

    return status;
}

static int rfat_volume_mount(rfat_volume_t * volume)
{
    int status = F_NO_ERROR;
    bool write_protected;
    uint32_t boot_blkno, tot_sec, cls_mask, cls_shift, blkcnt, blk_unit_size, product;
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
    uint32_t boot_blkcnt;
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
    uint32_t next_clsno;
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
    rfat_boot_t *boot;
#if (RFAT_CONFIG_MAX_FILES != 1)
    rfat_file_t *file_s, *file_e;
#endif /* (RFAT_CONFIG_MAX_FILES != 1) */
#if (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0)
    unsigned int index;
#endif /* (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0) */

#if (RFAT_CONFIG_STATISTICS == 1)
    memset(&volume->statistics, 0, sizeof(volume->statistics));
#endif /* (RFAT_CONFIG_STATISTICS == 1) */

    status = rfat_disk_info(volume->disk, &write_protected, &blkcnt, &blk_unit_size, &product);

    if (status == F_NO_ERROR)
    {
	if (volume->state == RFAT_VOLUME_STATE_CARDREMOVED)
	{
	    if (volume->product != product)
	    {
		volume->state = RFAT_VOLUME_STATE_UNUSABLE;
		
		status = F_ERR_UNUSABLE;
	    }
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
	    else
	    {
		/* Check whether the log needs to be replayed after a SDCARD power failure.
		 */
		if ((volume->log_lead_sig == RFAT_HTOFL(RFAT_LOG_LEAD_SIG)) &&
		    (volume->log_struct_sig == RFAT_HTOFL(RFAT_LOG_STRUCT_SIG)))
		{
		    status = rfat_volume_commit(volume);
		}
	    }
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
	}
	else
	{
	    volume->flags = 0;
	
	    if (write_protected)
	    {
		volume->flags |= RFAT_VOLUME_FLAG_WRITE_PROTECTED;
	    }

	    volume->product = product;
	    volume->serial = 0;

	    boot_blkno = 0;

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
	    /* For a FAT32 file system TRANSACTION_SAFE needs 3 blocks,
	     * which are #3, #4 & #5, which are conveniantly reserved
	     * by the FAT32 spec. FAT16 is somewhat more tricky. There
	     * are 2 ways to lay out the BPB relative to FAT/DIR and still
	     * respect the erase boundary. One is to used a fixed value
	     * of 1 for bpb_rsvd_sec_cnt. That implies that there is at
	     * least 1 unused block before the BPB (since FAT/DIR always
	     * have an even count). The other variant is to anchor the BPB
	     * at an erase boundary, which then results in a bpb_rsvd_sec_cnt
	     * of at least 2, again 1 reserved block.
	     */
	    boot_blkcnt = 0;

	    boot = (rfat_boot_t*)((void*)&volume->bs_data[0]);

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

	    boot = (rfat_boot_t*)((void*)volume->dir_cache.data);

#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

	    status = rfat_volume_read(volume, boot_blkno, (uint8_t*)boot);

	    if (status == F_NO_ERROR)
	    {
		/* This is somewhat iffy. MMC/SDSC do support a FILE_FORMAT setting in CSD
		 * that points to a FDC descriptor, without partition table. However the
		 * default is to use a partition table (with BPB or extended BPB). Thus
		 * there are checks in place for the presence of a FDC descriptor, and
		 * a validation check for the partition table to filter out exFAT for SDXC.
		 */
	       
		if (boot->bs.bs_trail_sig != RFAT_HTOFS(0xaa55))
		{
		    status = F_ERR_NOTFORMATTED;
		}
		else
		{
		    if (!((boot->bpb.bs_jmp_boot[0] == 0xe9) ||
			  ((boot->bpb.bs_jmp_boot[0] == 0xeb) && (boot->bpb.bs_jmp_boot[2] == 0x90))))
		    {
			if ((boot->mbr.mbr_par_table[0].mbr_sys_id == 0x01) ||
			    (boot->mbr.mbr_par_table[0].mbr_sys_id == 0x04) ||
			    (boot->mbr.mbr_par_table[0].mbr_sys_id == 0x06) ||
			    (boot->mbr.mbr_par_table[0].mbr_sys_id == 0x0b) ||
			    (boot->mbr.mbr_par_table[0].mbr_sys_id == 0x0c))
			{
			    boot_blkno = RFAT_FTOHL(boot->mbr.mbr_par_table[0].mbr_rel_sec);

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
			    if ((boot->mbr.mbr_par_table[1].mbr_sys_id == 0x00) &&
				(boot->mbr.mbr_par_table[2].mbr_sys_id == 0x00) &&
				(boot->mbr.mbr_par_table[3].mbr_sys_id == 0x00))
			    {
				boot_blkcnt = boot_blkno -1;
			    }
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

			    status = rfat_volume_read(volume, boot_blkno, (uint8_t*)boot);
			
			    if (status == F_NO_ERROR)
			    {
				if ((boot->bs.bs_trail_sig != RFAT_HTOFS(0xaa55)) ||
				    !((boot->bpb.bs_jmp_boot[0] == 0xe9) ||
				      ((boot->bpb.bs_jmp_boot[0] == 0xeb) && (boot->bpb.bs_jmp_boot[2] == 0x90))))
				{
				    status = F_ERR_NOTFORMATTED;
				}
			    }
			}
			else
			{
			    status = F_ERR_INVALIDMEDIA;
			}
		    }
		}

		if (status == F_NO_ERROR)
		{
		    /* Here we have a valid PBR signature, check whether there is a valid BPB.
		     */

		    /* Legal values for bpb_byts_per_sec are 512, 1024, 2048 and 4096.
		     * Hence if 0x8000 is ored in, it will be illegal. This 0x8000 mask is
		     * used to figure out whether TRANSACTION_SAFE was in rfat_volume_commit
		     * or not.
		     */

		    if ((boot->bpb.bpb_byts_per_sec != RFAT_HTOFS(RFAT_BLK_SIZE))
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
			&& ((boot->bpb.bpb_byts_per_sec != (0x8000 | RFAT_HTOFS(RFAT_BLK_SIZE))) ||
			    (boot->bpblog.log_lead_sig != RFAT_HTOFL(RFAT_LOG_LEAD_SIG)) ||
			    (boot->bpblog.log_struct_sig != RFAT_HTOFL(RFAT_LOG_STRUCT_SIG)))
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
			)
		    {
			status = F_ERR_NOTSUPPSECTORSIZE;
		    }
		    else if (!boot->bpb.bpb_sec_per_clus || (boot->bpb.bpb_sec_per_clus & (boot->bpb.bpb_sec_per_clus -1)))
		    {
			status = F_ERR_INVALIDMEDIA;
		    }
		    else if ((boot->bpb.bpb_num_fats == 0) || (boot->bpb.bpb_num_fats > 2))
		    {
			status = F_ERR_INVALIDMEDIA;
		    }
		    else if (!((boot->bpb.bpb_media == 0xf0) || (boot->bpb.bpb_media >= 0xf8)))
		    {
			status = F_ERR_INVALIDMEDIA;
		    }
		    else
		    {
#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) && (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
			memcpy(&volume->bs_data[0], boot, sizeof(volume->bs_data));
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) && (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */

			volume->cls_size = (boot->bpb.bpb_sec_per_clus * RFAT_BLK_SIZE);
			volume->cls_mask = volume->cls_size -1;

			for (cls_mask = 0x8000, cls_shift = 16; !(volume->cls_mask & cls_mask); cls_mask >>= 1, cls_shift--) { }

			volume->cls_shift = cls_shift;
			volume->cls_blk_shift = cls_shift - RFAT_BLK_SHIFT;
			volume->cls_blk_size = (1 << volume->cls_blk_shift);
			volume->cls_blk_mask = volume->cls_blk_size -1;

			volume->boot_blkno = boot_blkno;

			/* FAT32 differs from FAT12/FAT16 by having bpb_fat_sz_16 forced to 0. 
			 * The bpb_root_ent_cnt is ignored for the decision.
			 */
			if (boot->bpb.bpb_fat_sz_16 != 0)
			{
			    /* FAT12/FAT16 */

			    volume->fsinfo_blkofs = 0;
			    volume->bkboot_blkofs = 0;

			    if (boot->bpb.bpb_tot_sec_16 != 0)
			    {
				tot_sec = RFAT_FTOHS(boot->bpb.bpb_tot_sec_16);
			    }
			    else
			    {
				tot_sec = RFAT_FTOHL(boot->bpb40.bpb_tot_sec_32);
			    }

			    volume->fat_blkcnt = RFAT_FTOHS(boot->bpb.bpb_fat_sz_16);
			    volume->fat1_blkno = volume->boot_blkno + RFAT_FTOHS(boot->bpb40.bpb_rsvd_sec_cnt);

			    if (boot->bpb.bpb_num_fats == 1)
			    {
				volume->fat2_blkno = 0;
				volume->root_blkno = volume->fat1_blkno + volume->fat_blkcnt;

			    }
			    else
			    {
				volume->fat2_blkno = volume->fat1_blkno + volume->fat_blkcnt;
				volume->root_blkno = volume->fat2_blkno + volume->fat_blkcnt;
			    }

			    volume->root_clsno = RFAT_CLSNO_NONE;
			    volume->root_blkcnt = ((RFAT_FTOHS(boot->bpb40.bpb_root_ent_cnt) * 32) + RFAT_BLK_MASK) >> RFAT_BLK_SHIFT;

			    volume->cls_blk_offset = (volume->root_blkno + volume->root_blkcnt) - (2 << volume->cls_blk_shift);

			    if (boot->bpb40.bs_boot_sig == 0x29)
			    {
				volume->serial = RFAT_FTOHL(boot->bpb40.bs_vol_id);

#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
				if (boot->bpb40.bs_nt_reserved & 0x01)
				{
				    volume->flags |= RFAT_VOLUME_FLAG_MOUNTED_DIRTY;
				}
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */
			    }
			}
			else
			{
			    /* FAT32 */

			    volume->fsinfo_blkofs = RFAT_FTOHS(boot->bpb71.bpb_fsinfo);
			    volume->bkboot_blkofs = RFAT_FTOHS(boot->bpb71.bpb_bkboot);

			    tot_sec = RFAT_FTOHL(boot->bpb71.bpb_tot_sec_32);

			    volume->fat_blkcnt = RFAT_FTOHL(boot->bpb71.bpb_fat_sz_32);
			    volume->fat1_blkno = volume->boot_blkno + RFAT_FTOHS(boot->bpb71.bpb_rsvd_sec_cnt);

			    /* It's tempting to use bpb71.bpb_ext_flags here to modify mirroring, 
			     * it seems to be problematic. For one most of the OSes do not support
			     * or respect this flag. The other minor detail is that Win95/Win98
			     * do use different settings than Win2k/WinXP/Win7.
			     */
			    if (boot->bpb.bpb_num_fats == 1)
			    {
				volume->fat2_blkno = 0;

				volume->cls_blk_offset = (volume->fat1_blkno + volume->fat_blkcnt) - (2 << volume->cls_blk_shift);
			    }
			    else
			    {
				volume->fat2_blkno = volume->fat1_blkno + volume->fat_blkcnt;

				volume->cls_blk_offset = (volume->fat2_blkno + volume->fat_blkcnt) - (2 << volume->cls_blk_shift);
			    }

			    volume->root_clsno = RFAT_FTOHL(boot->bpb71.bpb_root_clus);
			    volume->root_blkno = volume->cls_blk_offset + (volume->root_clsno << volume->cls_blk_shift);
			    volume->root_blkcnt = volume->cls_blk_size;

			    if (boot->bpb71.bs_boot_sig == 0x29)
			    {
				volume->serial = RFAT_FTOHL(boot->bpb71.bs_vol_id);

#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
				if (boot->bpb71.bs_nt_reserved & 0x01)
				{
				    volume->flags |= RFAT_VOLUME_FLAG_MOUNTED_DIRTY;
				}
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */
			    }
			}

			if (status == F_NO_ERROR)
			{
			    volume->last_clsno = (((boot_blkno + tot_sec) - volume->cls_blk_offset) >> volume->cls_blk_shift) -1;

#if (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1) || (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
			    if (blk_unit_size == 0)
			    {
				/* Estimate the upper limit of the AU Size according to 4.13.1.8.1 from
				 * the blkcnt to compute volume->blk_unit_size, used to align contiguous
				 * cluster allocations.
				 */

				if (blkcnt < 4209984)
				{
				    if (blkcnt <= 131072)        /* <= 64MB  -> 512k */
				    {
					blk_unit_size = 1024;
				    }
				    else if (blkcnt <= 524288)   /* <= 256MB -> 1MB  */
				    {
					blk_unit_size = 2048;
				    }
				    else                         /* <= 1GB   -> 2MB  */
				    {
					blk_unit_size = 4096;
				    }
				}
				else
				{
				    if (blkcnt <= 67108864)      /* <= 32GB  -> 4MB  */
				    {
					blk_unit_size = 8192;
				    }
				    else                         /* <= 64GB  -> 16MB */
				    {
					blk_unit_size = 32768;
				    }
				}
			    }

			    volume->blk_unit_size = blk_unit_size;
			    
			    volume->start_clsno = ((((volume->cls_blk_offset + (2 << volume->cls_blk_shift) + blk_unit_size -1)
						     / blk_unit_size) * blk_unit_size)
						   - volume->cls_blk_offset) >> volume->cls_blk_shift;
			    volume->end_clsno = ((((((volume->last_clsno +1) << volume->cls_blk_shift) + volume->cls_blk_offset)
						   / blk_unit_size) * blk_unit_size)
						 - volume->cls_blk_offset) >> volume->cls_blk_shift;
#endif /* (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1) || (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */

			    if (status == F_NO_ERROR)
			    {
				if ((volume->last_clsno -1) < 4085)
				{
#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
				    volume->type = RFAT_VOLUME_TYPE_FAT12;
#else /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
				    status = F_ERR_INVALIDMEDIA;
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
				}
				else
				{
				    if ((volume->last_clsno -1) < 65525)
				    {
					volume->type = RFAT_VOLUME_TYPE_FAT16;
				    }
				    else
				    {
					volume->type = RFAT_VOLUME_TYPE_FAT32;
				    }
				}

				if (status == F_NO_ERROR)
				{
				    /* Assign the caches.
				     */
				    volume->dir_cache.blkno = RFAT_BLKNO_INVALID;

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
				    volume->map_cache.blkno = RFAT_BLKNO_INVALID;
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

#if (RFAT_CONFIG_FAT_CACHE_ENTRIES != 0)
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1)
				    volume->fat_cache.blkno = RFAT_BLKNO_INVALID;
#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
				    volume->fat_cache[0].blkno = RFAT_BLKNO_INVALID;
				    volume->fat_cache[1].blkno = RFAT_BLKNO_INVALID;
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES != 0) */
			    
#if (RFAT_CONFIG_FILE_DATA_CACHE == 0)
				    volume->data_file = NULL;
#if (RFAT_CONFIG_DATA_CACHE_ENTRIES != 0)
				    volume->data_cache.blkno = RFAT_BLKNO_INVALID;
#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES != 0) */
#endif /* (RFAT_CONFIG_FILE_DATA_CACHE == 0) */
			    
#if (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0)
				    for (index = 0; index < RFAT_CONFIG_CLUSTER_CACHE_ENTRIES; index++)
				    {
					volume->cluster_cache[index].clsno   = RFAT_CLSNO_NONE;
					volume->cluster_cache[index].clsdata = RFAT_CLSNO_NONE;
				    }
#endif /* (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0) */
			    
				    volume->cwd_clsno = RFAT_CLSNO_NONE;
			    
#if (RFAT_CONFIG_MAX_FILES == 1)
				    volume->file_table[0].mode = 0;
#else /* (RFAT_CONFIG_MAX_FILES == 1) */
				    file_s = &volume->file_table[0];
				    file_e = &volume->file_table[RFAT_CONFIG_MAX_FILES];
			    
				    do
				    {
					file_s->mode = 0;
					file_s++;
				    }
				    while (file_s < file_e);
#endif /* (RFAT_CONFIG_MAX_FILES == 1) */

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
				    if (!write_protected)
				    {
					/* Check whether the log needs to be replayed, or whether
					 * the BPB has to be set up in memory.
					 */
					if ((boot->bpblog.log_lead_sig == RFAT_HTOFL(RFAT_LOG_LEAD_SIG)) &&
					    (boot->bpblog.log_struct_sig == RFAT_HTOFL(RFAT_LOG_STRUCT_SIG)))
					{
					    status = rfat_volume_commit(volume);
					}
					else
					{
					    if ((boot->bpb.bpb_num_fats == 2) && (volume->fat_blkcnt <= 8192))
					    {
						if (volume->type != RFAT_VOLUME_TYPE_FAT32)
						{
						    if (RFAT_FTOHS(boot->bpb40.bpb_rsvd_sec_cnt) != 1)
						    {
							boot->bpblog.log_lfn_blkno = volume->boot_blkno +1;
						    }
						    else
						    {
							if (boot_blkcnt == 0)
							{
							    status = F_ERR_UNUSABLE;
							}
							else
							{
							    boot->bpblog.log_lfn_blkno = volume->boot_blkno -1;
							}
						    }
					    
						    boot->bpblog.log_map_blkno = RFAT_BLKNO_RESERVED;
						}
						else
						{
						    if ((volume->fsinfo_blkofs != 1) || (volume->bkboot_blkofs != 6))
						    {
							status = F_ERR_UNUSABLE;
						    }
						    else
						    {
							boot->bpblog.log_lfn_blkno = volume->boot_blkno +3;
							boot->bpblog.log_map_blkno = volume->boot_blkno +4;
						    }
						}
					
						if (status == F_NO_ERROR)
						{
						    boot->bpblog.log_map_flags = 0;
						    boot->bpblog.log_map_entries = 0;
						    boot->bpblog.log_dir_flags = 0;
						    boot->bpblog.log_lead_sig = RFAT_HTOFL(0x00000000);
						    boot->bpblog.log_struct_sig = RFAT_HTOFL(0x00000000);
						}
					    }
					    else
					    {
						status = F_ERR_UNUSABLE;
					    }
					}
				    }
				    else
				    {
					status = F_ERR_WRITEPROTECT;
				    }
				
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
				    /* For TRANSACTION_SAFE the FSINFO might have been already read
				     * throu rfat_volume_commit(). In this case RFAT_VOLUME_FLAG_FSINFO_VALID
				     * would be already set ...
				     */
				    if (!(volume->flags & RFAT_VOLUME_FLAG_FSINFO_VALID))
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
				    {
					volume->next_clsno = 2;
				
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
					volume->free_clscnt = 0;

					if (status == F_NO_ERROR)
					{
					    if (volume->fsinfo_blkofs != 0)
					    {
						rfat_fsinfo_t *fsinfo = (rfat_fsinfo_t*)((void*)volume->dir_cache.data);

						status = rfat_volume_read(volume, volume->boot_blkno + volume->fsinfo_blkofs, (uint8_t*)fsinfo);
				    
						if (status == F_NO_ERROR)
						{
						    if ((fsinfo->fsi_lead_sig  == RFAT_HTOFL(0x41615252)) &&
							(fsinfo->fsi_struc_sig == RFAT_HTOFL(0x61417272)) &&
							(fsinfo->fsi_trail_sig == RFAT_HTOFL(0xaa550000)))
						    {
#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
							if (!(volume->flags & RFAT_VOLUME_FLAG_MOUNTED_DIRTY))
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */
							{
							    if (fsinfo->fsi_free_count != 0xffffffff)
							    {
								volume->free_clscnt = RFAT_FTOHL(fsinfo->fsi_free_count);
							
								if (volume->free_clscnt <= (volume->last_clsno -1))
								{
								    volume->flags |= RFAT_VOLUME_FLAG_FSINFO_VALID;
								}
							    }
							}

							if (fsinfo->fsi_nxt_free != 0xffffffff)
							{
							    next_clsno = RFAT_FTOHL(fsinfo->fsi_nxt_free);

							    if ((next_clsno >= 2) && (next_clsno <= volume->last_clsno))
							    {
								volume->next_clsno = next_clsno;
							    }
							}
						    }
						}
					    }
					}
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
				    }

				    if (status == F_NO_ERROR)
				    {
#if (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1) 
					volume->free_clsno = volume->start_clsno;
					volume->base_clsno = volume->start_clsno;
					volume->limit_clsno = volume->start_clsno;
#endif /* (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1) */
				    }
				}
			    }
			}
		    }
		}
	    }
	}
	
	if (status == F_NO_ERROR)
	{
	    volume->state = RFAT_VOLUME_STATE_MOUNTED;
	}
    }

    return status;
}


static int rfat_volume_unmount(rfat_volume_t * volume)
{
    int status = F_NO_ERROR;
    rfat_file_t *file;
#if (RFAT_CONFIG_MAX_FILES > 1)
    rfat_file_t *file_e;
#endif /* (RFAT_CONFIG_MAX_FILES > 1) */

    if (volume->state >= RFAT_VOLUME_STATE_MOUNTED)
    {
#if (RFAT_CONFIG_MAX_FILES == 1)
	file = &volume->file_table[0];

	if (file->mode)
	{
	    status = rfat_file_close(volume, file);
	}
#else /* (RFAT_CONFIG_MAX_FILES == 1) */
	file = &volume->file_table[0];
	file_e = &volume->file_table[RFAT_CONFIG_MAX_FILES];
	    
	do
	{
	    if (file->mode)
	    {
		status = rfat_file_close(volume, file);
	    }
		
	    file++;
	}
	while ((status == F_NO_ERROR) && (file < file_e));
#endif /* (RFAT_CONFIG_MAX_FILES == 1) */

	if (status == F_NO_ERROR)
	{
	    /* Revert the state to be RFAT_VOLUME_STATE_INITIALIZED, so that can be remounted.
	     */
	    volume->state = RFAT_VOLUME_STATE_INITIALIZED;
	}
    }

    return status;
}

static int rfat_volume_lock(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

    if (volume->state == RFAT_VOLUME_STATE_NONE)
    {
	status = F_ERR_INITFUNC;
    }
    else
    {
#if defined(RFAT_PORT_CORE_LOCK)
	if (!RFAT_PORT_CORE_LOCK())
	{
	    status = F_ERR_BUSY;
	}
	else
#endif /* RFAT_PORT_CORE_LOCK */
	{
	    if (volume->state == RFAT_VOLUME_STATE_UNUSABLE)
	    {
		status = F_ERR_UNUSABLE;
	    }
	    else
	    {
		/*
		 * Check here whether the volume is mounted. If not mount it. If it cannot
		 * be mounted, throw an error.
		 */
		
		if (volume->state != RFAT_VOLUME_STATE_MOUNTED)
		{
		    if (volume->disk == NULL)
		    {
			status = F_ERR_INITFUNC;
		    }
		    else
		    {
			status = rfat_volume_mount(volume);
		    }
		}
	    }

#if defined(RFAT_PORT_CORE_UNLOCK)
	    if (status != F_NO_ERROR)
	    {
		RFAT_PORT_CORE_UNLOCK();
	    }
#endif /* RFAT_PORT_CORE_UNLOCK */
	}
    }

    return status;
}

static int rfat_volume_lock_noinit(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

    if (volume->state == RFAT_VOLUME_STATE_NONE)
    {
#if defined(RFAT_PORT_CORE_INIT)
	if (RFAT_PORT_CORE_INIT())
#endif /* RFAT_PORT_CORE_INIT */
	{
	    volume->state = RFAT_VOLUME_STATE_INITIALIZED;
	}
    }

    if (volume->state == RFAT_VOLUME_STATE_NONE)
    {
	status = F_ERR_INITFUNC;
    }
    else
    {
#if defined(RFAT_PORT_CORE_LOCK)
	if (!RFAT_PORT_CORE_LOCK())
	{
	    status = F_ERR_BUSY;
	}
#endif /* RFAT_PORT_CORE_LOCK */
    }

    return status;
}

static int  rfat_volume_lock_nomount(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

    if (volume->state == RFAT_VOLUME_STATE_NONE)
    {
	status = F_ERR_INITFUNC;
    }
    else
    {
#if defined(RFAT_PORT_CORE_LOCK)
	if (!RFAT_PORT_CORE_LOCK())
	{
	    status = F_ERR_BUSY;
	}
	else
#endif /* RFAT_PORT_CORE_LOCK */
	{
	    if (volume->disk == NULL)
	    {
		status = F_ERR_INITFUNC;
	    }

#if defined(RFAT_PORT_CORE_UNLOCK)
	    RFAT_PORT_CORE_UNLOCK();
#endif /* RFAT_PORT_CORE_UNLOCK */
	}
    }

    return status;
}

static int rfat_volume_unlock(rfat_volume_t *volume, int status)
{
    if (status != F_NO_ERROR)
    {
#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
	if (status == F_ERR_INVALIDSECTOR)
	{
	    volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
	}
	else
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */
	{
	    if (status == F_ERR_CARDREMOVED)
	    {
		volume->state = RFAT_VOLUME_STATE_CARDREMOVED;
	    }

	    if (status == F_ERR_UNUSABLE)
	    {
		volume->state = RFAT_VOLUME_STATE_UNUSABLE;
	    }
	}
    }

#if defined(RFAT_PORT_CORE_UNLOCK)
    RFAT_PORT_CORE_UNLOCK();
#endif /* RFAT_PORT_CORE_UNLOCK */

    return status;
}

static int rfat_volume_read(rfat_volume_t *volume, uint32_t address, uint8_t *data)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_META_DATA_RETRIES != 0)
    unsigned int retries = RFAT_CONFIG_META_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_META_DATA_RETRIES != 0) */
    unsigned int retries = 0;
#endif /* (RFAT_CONFIG_META_DATA_RETRIES != 0) */

    do
    {
	// status = rfat_disk_read(volume->disk, address, data);
	status = rfat_disk_read_sequential(volume->disk, address, 1, data);
		
	if ((status == F_ERR_ONDRIVE) && (retries >= 1))
	{
	    status = F_NO_ERROR;
	    retries--;
	}
	else
	{
	    retries = 0;
	}
    }
    while ((status == F_NO_ERROR) && retries);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
    if (status == F_ERR_INVALIDSECTOR)
    {
	volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

    if (status != F_NO_ERROR)
    {
	if (status != F_ERR_CARDREMOVED)
	{
	    status = F_ERR_UNUSABLE;
	}
    }

    return status;
}

static int rfat_volume_write(rfat_volume_t *volume, uint32_t address, const uint8_t *data)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_META_DATA_RETRIES != 0)
    unsigned int retries = RFAT_CONFIG_META_DATA_RETRIES +1;
#else /* (RFAT_CONFIG_META_DATA_RETRIES != 0) */
    unsigned int retries = 0;
#endif /* (RFAT_CONFIG_META_DATA_RETRIES != 0) */

    do
    {
	status = rfat_disk_write(volume->disk, address, data);
		
	if ((status == F_ERR_ONDRIVE) && (retries >= 1))
	{
	    status = F_NO_ERROR;
	    retries--;
	}
	else
	{
	    retries = 0;
	}
    }
    while ((status == F_NO_ERROR) && retries);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
    if (status == F_ERR_INVALIDSECTOR)
    {
	volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

    if (status != F_NO_ERROR)
    {
	if (status != F_ERR_CARDREMOVED)
	{
	    status = F_ERR_UNUSABLE;
	}
    }

    return status;
}

static int  rfat_volume_zero(rfat_volume_t *volume, uint32_t address, uint32_t length, volatile uint8_t *p_status)
{
    int status = F_NO_ERROR;
    uint8_t *data;

    status = rfat_dir_cache_flush(volume);

    if (status == F_NO_ERROR)
    {
	data = volume->dir_cache.data;
	volume->dir_cache.blkno = RFAT_BLKNO_INVALID;
	
	memset(data, 0, RFAT_BLK_SIZE);

	do
	{
	    status = rfat_disk_write_sequential(volume->disk, address, 1, data, p_status);

	    address++;
	    length--;
	}
	while ((status == F_NO_ERROR) && length);
    }

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
    if (status == F_ERR_INVALIDSECTOR)
    {
	volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

    if (p_status == NULL)
    {
	if (status == F_NO_ERROR)
	{
	    status = rfat_disk_sync(volume->disk, NULL);
	}

	if (status != F_NO_ERROR)
	{
	    if (status != F_ERR_CARDREMOVED)
	    {
		status = F_ERR_UNUSABLE;
	    }
	}
    }

    return status;
}

#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)

static int rfat_volume_fsinfo(rfat_volume_t *volume, uint32_t free_clscnt, uint32_t next_clsno)
{
    int status = F_NO_ERROR;
    uint32_t blkno;
    rfat_fsinfo_t *fsinfo;
    
    status = rfat_dir_cache_flush(volume);
	    
    if (status == F_NO_ERROR)
    {
	blkno = (volume->boot_blkno + volume->fsinfo_blkofs);
	fsinfo = (rfat_fsinfo_t*)((void*)volume->dir_cache.data);
	
	if (volume->dir_cache.blkno != blkno)
	{
	    memset((uint8_t*)fsinfo, 0, RFAT_BLK_SIZE);
	    
	    fsinfo->fsi_lead_sig   = RFAT_HTOFL(0x41615252);
	    fsinfo->fsi_struc_sig  = RFAT_HTOFL(0x61417272);
	    fsinfo->fsi_trail_sig  = RFAT_HTOFL(0xaa550000);
	    
	    volume->dir_cache.blkno = blkno;
	}		
	
	fsinfo->fsi_free_count = RFAT_HTOFL(free_clscnt);
	fsinfo->fsi_nxt_free   = RFAT_HTOFL(next_clsno);
	
	status = rfat_volume_write(volume, blkno, (uint8_t*)fsinfo);
    }

    return status;
}

#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)

static int rfat_volume_dirty(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
    rfat_boot_t *boot;
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */

    if (volume->state == RFAT_VOLUME_STATE_MOUNTED)
    {
#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
	if (!(volume->flags & RFAT_VOLUME_FLAG_MOUNTED_DIRTY) &&
	    !(volume->flags & RFAT_VOLUME_FLAG_VOLUME_DIRTY))
	{
	    status = rfat_dir_cache_flush(volume);

	    if (status == F_NO_ERROR)
	    {
		boot = (rfat_boot_t*)((void*)volume->dir_cache.data);

		if (volume->dir_cache.blkno != volume->boot_blkno)
		{
		    memcpy((uint8_t*)boot, volume->bs_data, sizeof(volume->bs_data));
		    memset((uint8_t*)boot + sizeof(volume->bs_data), 0, 510 - sizeof(volume->bs_data));

		    boot->bpb.bs_trail_sig = RFAT_HTOFS(0xaa55);

		    volume->dir_cache.blkno = volume->boot_blkno;
		}

		if (status == F_NO_ERROR)
		{
		    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		    {
			boot->bpb71.bs_nt_reserved |= 0x01;
		    }
		    else
		    {
			boot->bpb40.bs_nt_reserved |= 0x01;
		    }

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
		    if (volume->flags & RFAT_VOLUME_FLAG_MEDIA_FAILURE)
		    {
			if (volume->type == RFAT_VOLUME_TYPE_FAT32)
			{
			    boot->bpb71.bs_nt_reserved |= 0x02;
			}
			else
			{
			    boot->bpb40.bs_nt_reserved |= 0x02;
			}
		    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */
		    
		    status = rfat_volume_write(volume, volume->boot_blkno, (uint8_t*)boot);

		    if (status == F_NO_ERROR)
		    {
			volume->flags |= RFAT_VOLUME_FLAG_VOLUME_DIRTY;
		    }
		}
	    }
	}
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */
    }

    return status;
}

static int rfat_volume_clean(rfat_volume_t *volume, int status_o)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
    rfat_boot_t *boot;
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */

    if (volume->state == RFAT_VOLUME_STATE_MOUNTED)
    {
#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	if ((volume->fsinfo_blkofs != 0) &&
	    (volume->flags & RFAT_VOLUME_FLAG_FSINFO_DIRTY))
	{
	    if (!(volume->flags & RFAT_VOLUME_FLAG_MOUNTED_DIRTY) &&
		(volume->flags & RFAT_VOLUME_FLAG_FSINFO_VALID))
	    {
		status = rfat_volume_fsinfo(volume, volume->free_clscnt, volume->next_clsno);
	    }
	    else
	    {
		status = rfat_volume_fsinfo(volume, 0xffffffff, volume->next_clsno);
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_DIRTY;
	    }
	}
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */

	if (!(volume->flags & RFAT_VOLUME_FLAG_MOUNTED_DIRTY) &&
	    (volume->flags & RFAT_VOLUME_FLAG_VOLUME_DIRTY))
	{
 	    status = rfat_dir_cache_flush(volume);

	    if (status == F_NO_ERROR)
	    {
		boot = (rfat_boot_t*)((void*)volume->dir_cache.data);

		if (volume->dir_cache.blkno != volume->boot_blkno)
		{
		    memcpy((uint8_t*)boot, volume->bs_data, 90);
		    memset((uint8_t*)boot + 90, 0, 510 - 90);

		    boot->bpb.bs_trail_sig = RFAT_HTOFS(0xaa55);

		    volume->dir_cache.blkno = volume->boot_blkno;
		}

		if (status == F_NO_ERROR)
		{
		    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		    {
			boot->bpb71.bs_nt_reserved &= ~0x01;
		    }
		    else
		    {
			boot->bpb40.bs_nt_reserved &= ~0x01;
		    }

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
		    if (volume->flags & RFAT_VOLUME_FLAG_MEDIA_FAILURE)
		    {
			if (volume->type == RFAT_VOLUME_TYPE_FAT32)
			{
			    boot->bpb71.bs_nt_reserved |= 0x02;
			}
			else
			{
			    boot->bpb40.bs_nt_reserved |= 0x02;
			}
		    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */
		    
		    status = rfat_volume_write(volume, volume->boot_blkno, (uint8_t*)boot);

		    if (status == F_NO_ERROR)
		    {
			volume->flags &= ~RFAT_VOLUME_FLAG_VOLUME_DIRTY;
		    }
		}
	    }
	}

#else /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	if ((volume->fsinfo_blkofs != 0) &&
	    (volume->flags & RFAT_VOLUME_FLAG_FSINFO_DIRTY))
	{
	    status = rfat_volume_fsinfo(volume, 0xffffffff, volume->next_clsno);
	    
	    if (status == F_NO_ERROR)
	    {
		volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_DIRTY;
	    }
	}
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */
    }

    if (status_o != F_NO_ERROR)
    {
	status = status_o;
    }

    return status;
}

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */

static int rfat_volume_record(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1)
    uint16_t bs_trail_sig;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
    rfat_boot_t *boot;

    status = rfat_map_cache_flush(volume);

    if (status == F_NO_ERROR)
    {
	if (volume->map_flags
	    || ((volume->dir_flags & (RFAT_DIR_FLAG_DESTROY_ENTRY | RFAT_DIR_FLAG_CREATE_ENTRY)) == (RFAT_DIR_FLAG_DESTROY_ENTRY | RFAT_DIR_FLAG_CREATE_ENTRY))
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	    || ((volume->dir_flags & RFAT_DIR_FLAG_DESTROY_ENTRY) && volume->del_entries)
	    || ((volume->dir_flags & RFAT_DIR_FLAG_CREATE_ENTRY) && volume->dir_entries)
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    )
	{
	    boot = (rfat_boot_t*)((void*)&volume->bs_data[0]);

	    /* Make sure the file system is marked unusable other
	     * than for TRANSACTION_SAFE recovery.
	     */
	    boot->bpb.bpb_byts_per_sec |= 0x8000;

	    /* Fill in the heads to signal an uncomitted rfat_volume_record().
	     */
	    boot->bpblog.log_lead_sig = RFAT_HTOFL(RFAT_LOG_LEAD_SIG);
	    boot->bpblog.log_struct_sig = RFAT_HTOFL(RFAT_LOG_STRUCT_SIG);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
	    if (volume->flags & RFAT_VOLUME_FLAG_MEDIA_FAILURE)
	    {
		if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		{
		    boot->bpb71.bs_nt_reserved |= 0x02;
		}
		else
		{
		    boot->bpb40.bs_nt_reserved |= 0x02;
		}
	    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */
	    
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1)
	    /* If VFAT and UTF8 are enabled, VFAT uses uint16_t per lfn_name character. That will
	     * overflow the boot sector with the maximum length of 255. Hence detect the overflow
	     * case, and write the lfn_name to a separate block, patch in the bs_trail_sig, 
	     * write the boot block, and then undo the patching of the bs_trail_sig.
	     */
	    if ((volume->dir_flags & RFAT_DIR_FLAG_CREATE_ENTRY) &&
		(volume->dir_entries != 0) &&
		(volume->lfn_count > 128))
	    {
		status = rfat_volume_write(volume, volume->lfn_blkno, (uint8_t*)volume->lfn_name);
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		bs_trail_sig = boot->bpb.bs_trail_sig;
		
		boot->bpb.bs_trail_sig = RFAT_HTOFS(0xaa55);
		
		status = rfat_volume_write(volume, volume->boot_blkno, (uint8_t*)boot);
		
		boot->bpb.bs_trail_sig = bs_trail_sig;
	    }
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
	    status = rfat_volume_write(volume, volume->boot_blkno, (uint8_t*)boot);
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
	}

	if (status == F_NO_ERROR)
	{
	    status = rfat_volume_commit(volume);
	}
    }

    return status;
}

static int rfat_volume_commit(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;
    uint32_t clsno, blkno, index;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    unsigned int sequence, chksum, s, i, cc;
    uint32_t blkno_e, offset;
#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
    uint16_t bs_trail_sig;
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
    rfat_dir_t *dir_e;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    rfat_dir_t *dir;
    rfat_cache_entry_t *entry;
    rfat_boot_t *boot;

    boot = (rfat_boot_t*)((void*)&volume->bs_data[0]);
	
    if (boot->bpblog.log_lead_sig == RFAT_HTOFL(RFAT_LOG_LEAD_SIG))
    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1)
	/* At this point, the boot block had been loaded. For VFAT and UTF8
	 * the remaining lfn_name[] characters have to be loaded as there is
	 * not enough space for 255 unicode characters.
	 */
	if ((volume->dir_flags & RFAT_DIR_FLAG_CREATE_ENTRY) &&
	    (volume->dir_entries != 0) &&
	    (volume->lfn_count > 128))
	{
	    status = rfat_volume_read(volume, volume->lfn_blkno, (uint8_t*)volume->lfn_name);
	}
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
    }

    if (status == F_NO_ERROR)
    {
	if (volume->map_flags & RFAT_MAP_FLAG_MAP_CHANGED)
	{
	    status = rfat_map_cache_resolve(volume);
	}
    }

#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
    /* Update the FSINFO before dealing with RFAT_DIR_FLAG_CREATE_ENTRY, so that
     * volume->dir_cache is valid and not nuked by rfat_volume_fsinfo.
     * N.b. RFAT_VOLUME_FLAG_FSINFO_VALID & RFAT_VOLUME_FLAG_FSINFO_DIRTY are 
     * set if RFAT_MAP_FLAG_FSINFO was set (which only gets set if there was
     * a map/fat update pending).
     */
    if (status == F_NO_ERROR)
    {
	if ((volume->fsinfo_blkofs != 0) &&
	    (volume->flags & RFAT_VOLUME_FLAG_FSINFO_VALID) &&
	    (volume->flags & RFAT_VOLUME_FLAG_FSINFO_DIRTY))
	{
	    status = rfat_volume_fsinfo(volume, volume->free_clscnt, volume->next_clsno);

	    if (status == F_NO_ERROR)
	    {
		volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_DIRTY;
	    }
	}
    }
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */

    if (status == F_NO_ERROR)
    {
	if (volume->dir_flags & (RFAT_DIR_FLAG_SYNC_ENTRY | RFAT_DIR_FLAG_ACCESS_ENTRY | RFAT_DIR_FLAG_MODIFY_ENTRY))
	{
	    clsno = volume->dir_clsno;
	    index = volume->dir_index;

	    if (clsno == RFAT_CLSNO_NONE)
	    {
		blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index);
	    }
	    else
	    {
		blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_INDEX_TO_BLKCNT(index);
	    }
    
	    status = rfat_dir_cache_read(volume, blkno, &entry);

	    if (status == F_NO_ERROR)
	    {
		dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index)));

		dir->dir_clsno_lo = volume->dir.dir_clsno_lo;
		dir->dir_file_size = volume->dir.dir_file_size;
		
		if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		{
		    dir->dir_clsno_hi = volume->dir.dir_clsno_hi;
		}
		
		if (volume->dir_flags & RFAT_DIR_FLAG_ACCESS_ENTRY)
		{
		    dir->dir_acc_date = volume->dir.dir_acc_date;
		}
		
		if (volume->dir_flags & RFAT_DIR_FLAG_MODIFY_ENTRY)
		{
		    dir->dir_attr |= RFAT_DIR_ATTR_ARCHIVE;
		    dir->dir_wrt_time = volume->dir.dir_wrt_time;
		    dir->dir_wrt_date = volume->dir.dir_wrt_date;
		}

		status = rfat_dir_cache_write(volume);

		if (status == F_NO_ERROR)
		{
		    volume->dir_flags &= ~(RFAT_DIR_FLAG_SYNC_ENTRY | RFAT_DIR_FLAG_ACCESS_ENTRY | RFAT_DIR_FLAG_MODIFY_ENTRY);
		}
	    }
	}
    }

    if (status == F_NO_ERROR)
    {
	if (volume->dir_flags & RFAT_DIR_FLAG_DESTROY_ENTRY)
	{
	    clsno = volume->del_clsno;
	    index = volume->del_index;

	    if (clsno == RFAT_CLSNO_NONE)
	    {
		clsno = volume->root_clsno;
		blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		blkno_e = volume->root_blkno + volume->root_blkcnt;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    }
	    else
	    {
		blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_INDEX_TO_BLKCNT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		blkno_e = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    }
    
	    status = rfat_dir_cache_read(volume, blkno, &entry);

	    if (status == F_NO_ERROR)
	    {
		dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index)));

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		if (volume->del_entries != 0)
		{
		    sequence = volume->del_entries;
		    
		    dir_e = (rfat_dir_t*)((void*)(entry->data + RFAT_BLK_SIZE));
		    
		    do
		    {
			dir->dir_name[0] = 0xe5;
			
			dir++;
			
			if (dir == dir_e)
			{
			    status = rfat_dir_cache_write(volume);
			    
			    blkno++;
			    
			    if (blkno == blkno_e)
			    {
				status = rfat_cluster_read(volume, clsno, &clsno);
				
				if (status == F_NO_ERROR)
				{
				    blkno = RFAT_CLSNO_TO_BLKNO(clsno);
				    blkno_e = blkno + volume->cls_blk_size;
				}
			    }
			    
			    if (status == F_NO_ERROR)
			    {
				status = rfat_dir_cache_read(volume, blkno, &entry);
			    }
			}
			
			sequence--;
		    }
		    while ((status == F_NO_ERROR) && (sequence != 0));
		}

		if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
		{
		    dir->dir_name[0] = 0xe5;
		    
		    status = rfat_dir_cache_write(volume);

		    if (status == F_NO_ERROR)
		    {
			volume->dir_flags &= ~RFAT_DIR_FLAG_DESTROY_ENTRY;
		    }
		}
	    }
	}
    }

    if (status == F_NO_ERROR)
    {
	if (volume->dir_flags & RFAT_DIR_FLAG_CREATE_ENTRY)
	{
	    clsno = volume->dir_clsno;
	    index = volume->dir_index;

	    if (clsno == RFAT_CLSNO_NONE)
	    {
		clsno = volume->root_clsno;
		blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		blkno_e = volume->root_blkno + volume->root_blkcnt;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    }
	    else
	    {
		blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_INDEX_TO_BLKCNT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		blkno_e = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    }
    
	    status = rfat_dir_cache_read(volume, blkno, &entry);

	    if (status == F_NO_ERROR)
	    {
		dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index)));
		
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		if (volume->dir_entries)
		{
		    sequence = 0x40 | volume->dir_entries;
		    chksum = rfat_name_checksum_dosname(volume->dir.dir_name);

		    dir_e = (rfat_dir_t*)((void*)(entry->data + RFAT_BLK_SIZE));

		    do
		    {
			s = 0;
			i = ((sequence & 0x1f) - 1) * 13;
		
			dir->dir_name[0] = sequence;
			dir->dir_attr = RFAT_DIR_ATTR_LONG_NAME;
			dir->dir_nt_reserved = 0x00;
			dir->dir_crt_time_tenth = chksum;
			dir->dir_clsno_lo = 0x0000;
		
			do
			{
			    offset = rfat_path_ldir_name_table[s++];
		    
			    if (i < volume->lfn_count)
			    {
				cc = volume->lfn_name[i];
			
				((uint8_t*)dir)[offset +0] = cc;
				((uint8_t*)dir)[offset +1] = cc >> 8;
			    }
			    else
			    {
				if (i == volume->lfn_count)
				{
				    ((uint8_t*)dir)[offset +0] = 0x00;
				    ((uint8_t*)dir)[offset +1] = 0x00;
				}
				else
				{
				    ((uint8_t*)dir)[offset +0] = 0xff;
				    ((uint8_t*)dir)[offset +1] = 0xff;
				}
			    }

			    i++;
			}
			while (s < 13);
		
			dir++;

			if (dir == dir_e)
			{
			    status = rfat_dir_cache_write(volume);

			    blkno++;

			    if (blkno == blkno_e)
			    {
				status = rfat_cluster_read(volume, clsno, &clsno);
		
				if (status == F_NO_ERROR)
				{
				    blkno = RFAT_CLSNO_TO_BLKNO(clsno);
				    blkno_e = blkno + volume->cls_blk_size;
				}
			    }

			    if (status == F_NO_ERROR)
			    {
				status = rfat_dir_cache_read(volume, blkno, &entry);
			    }
			}

			sequence = (sequence & 0x1f) -1;
		    }
		    while ((status == F_NO_ERROR) && (sequence != 0));
		}

		if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
		{
		    memcpy(dir, &volume->dir, sizeof(rfat_dir_t));

		    status = rfat_dir_cache_write(volume);

		    if (status == F_NO_ERROR)
		    {
			volume->dir_flags &= ~RFAT_DIR_FLAG_CREATE_ENTRY;
		    }
		}
	    }
	}
    }

    if (status == F_NO_ERROR)
    {
	if (boot->bpblog.log_lead_sig == RFAT_HTOFL(RFAT_LOG_LEAD_SIG))
	{
	    boot->bpb.bpb_byts_per_sec &= ~0x8000;

	    boot->bpblog.log_lead_sig = RFAT_HTOFL(0x00000000);
	    boot->bpblog.log_struct_sig = RFAT_HTOFL(0x00000000);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
	    if (volume->flags & RFAT_VOLUME_FLAG_MEDIA_FAILURE)
	    {
		if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		{
		    boot->bpb71.bs_nt_reserved |= 0x02;
		}
		else
		{
		    boot->bpb40.bs_nt_reserved |= 0x02;
		}
	    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1)
	    /* For VFAT and UTF8 the lfn_name[] needs to patched to contain
	     * the bs_trail_sig before writing the boot block, and the unpatched
	     * afterwards.
	     */
	    bs_trail_sig = boot->bpb.bs_trail_sig;

	    boot->bpb.bs_trail_sig = RFAT_HTOFS(0xaa55);
	
	    status = rfat_volume_write(volume, volume->boot_blkno, (uint8_t*)boot);
	
	    boot->bpb.bs_trail_sig = bs_trail_sig;

#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1) */

	    status = rfat_volume_write(volume, volume->boot_blkno, (uint8_t*)boot);

#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) && (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
	}
    }

    return status;
}

#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */


static int  rfat_volume_format(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;
    bool write_protected;
    unsigned int fattype, sys_id;
    uint32_t blkcnt, blk_unit_size, cls_blk_shift, clscnt, clscnt_e, tot_sec;
    uint32_t boot_blkno, fat1_blkno, fat2_blkno, root_blkno, clus_blkno, fat_blkcnt, root_blkcnt;
    uint32_t bkboot_blkofs, fsinfo_blkofs;
    uint32_t hpc, spt, start_h, start_s, start_c, end_h, end_s, end_c;
    uint32_t product;
    rfat_boot_t *boot;
    rfat_fsinfo_t *fsinfo;

    status = rfat_disk_info(volume->disk, &write_protected, &blkcnt, &blk_unit_size, &product);

    if (status == F_NO_ERROR)
    {
	if (write_protected)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
#if (RFAT_CONFIG_FAT12_SUPPORTED == 0)
	    /* SDSC with 64MB or less end up being FAT12. So let's reject them
	     * early.
	     */
	    if (blkcnt <= 131072)
	    {
		status = F_ERR_MEDIATOOSMALL;
	    }
	    else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 0) */
	    {
		/* SDSC/SDHC are limited to 32GB. Above it's a SDXC,
		 * which would exFAT ... but a 64GB SDXC still works
		 * fine with FAT32.
		 */
		if (blkcnt > 134217728)
		{
		    status = F_ERR_MEDIATOOLARGE;
		}
	    }
	}

	if (status == F_NO_ERROR)
	{
	    /*
	     * There is a upper limit of 4153344 blocks for SDSC cards, and
	     * a lower limit of 4211712 for SDHC cards as per SD spec (CSD description).
	     *
	     * Hower the real lower limit for SHDC is:
	     *
	     *    16384 + (65525 * 64) = 4209984
	     *    4211712 = 8192 + 8192 + (65552 * 64)
	     *
	     *    (16384 is the required padding upfront for SDHC at this size)
	     *
	     * The upper legal limit for SDSC is also slightly different from the
	     * spec if one thinks about it:
	     *
	     *    768 + (65524 * 64) = 4194304
	     *    4153344 = 768 + (64884 * 64)
	     *
	     *    (768 is the required padding upfront for SDSC at this size)
	     *
	     * Thus use those corrected limits to figure out SDSC/SDHC.
	     */
    
	    if (blkcnt < 4209984)
	    {
		/* SDSC, FAT12/FAT16 */

		/* Number of Heads and Sectors Per Track ...
		 *
		 * See Microsoft's MBR specs and the term "translation mode".
		 * 
		 * We want to use powers of 2, which means the upper limit for
		 * that representation is 1024 * 128 * 32  = 4194304. That happens
		 * to be also the upper limit for SDSC here. If the size is smaller
		 * than 2 * 16, use "hpc" 2 and "spt" 16, otherwise "spt" is 32, and 
		 * "hpc" is the smallest power of 2.
		 */

#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
		if (blkcnt <= (1024 * 2 * 16))
		{
		    hpc = 2;
		    spt = 16;
		}
		else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
		{
		    hpc = 2;
		    spt = 32;
                    
		    while (hpc < 128)
		    {
			if (blkcnt <= (1024 * hpc * spt))
			{
			    break;
			}

			hpc <<= 1;
		    }
		}

		/* This table is derived from a ScanDisk SDCARD Product Manual.
		 *
		 * "au_blk_size", is derived by assuming that "Total LBAs" - "User Data Sectors"
		 * needs to be a multiple of a power of 2. It was assumed that the maximum value
		 * would map to the proper "au_blk_size". Next "Total LBAs" - "Total Paration Sectors"
		 * allows to compute the number of blocks per FAT. Assuming that the FAT does not
		 * contain more than one partial populated blocks, this allows to compute the
		 * cluster size picked.
		 * 
		 * The 16MB threshold was derived from a ScanDisk MMC Card Product Manual.
		 * Cards with 4MB and 8MB capacity have an erase size of 8k, hence it's assumed
		 * here that this would map to SDCARDs as well. In reality it probably does not
		 * matter as there are not such cards out there.
		 */
#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
		if (blkcnt <= 16384)            /* <= 8MB   -> 8k/8k   */
		{
		    cls_blk_shift = 4;
		    blk_unit_size = 16;
		}
		else if (blkcnt <= 131072)      /* <= 64MB  -> 16k/16k */
		{
		    cls_blk_shift = 5;
		    blk_unit_size = 32;
		}
		else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
		{
		    if (blkcnt <= 524288)       /* <= 256MB -> 16k/32k */
		    {
			cls_blk_shift = 5;
			blk_unit_size = 64;
		    }
		    else if (blkcnt <= 2097152) /* <= 1GB   -> 16k/64k */
		    {
			cls_blk_shift = 5;
			blk_unit_size = 128;
		    }
		    else                        /* <= 2GB   -> 32k/64k */
		    {
			cls_blk_shift = 6;
			blk_unit_size = 128;
		    }
		}

		/*
		 * FAT12/FAT16 layout requires that the MBR takes up "blk_unit_size" blocks. There is also 32 blocks for
		 * the root directory. Then there is one boot sector
		 *
		 * To compute fat_blkcnt, first estimate clscnt_e based upon the minimum system area, then compute
		 * the required system area based upon this clscnt_e. With the required system area size recompute clscnt.
		 * If clscnt is less than clscnt_e, use clscnt as clscnt_e and restart the process.
		 *
		 * The will incrementally adjust the clscnt estimate downwards, minimizing the required system area.
		 */

		bkboot_blkofs = 0;
		fsinfo_blkofs = 0;

		root_blkcnt = 32;

		clscnt = ((blkcnt - 2 * blk_unit_size) & ~(blk_unit_size -1)) >> cls_blk_shift;

#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
		if (clscnt < 4085)
		{
		    fattype = RFAT_VOLUME_TYPE_FAT12;
		}
		else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
		{
		    fattype = RFAT_VOLUME_TYPE_FAT16;

		    if (clscnt > 65524)
		    {
			clscnt = 65524;
		    }
		}


		do 
		{
		    clscnt_e = clscnt;
			    
#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
		    if (fattype == RFAT_VOLUME_TYPE_FAT12)
		    {
			fat_blkcnt = (((((clscnt + 2) * 3) + 1) / 2) + (RFAT_BLK_SIZE -1)) >> RFAT_BLK_SHIFT;
		    }
		    else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
		    {
			fat_blkcnt = ((clscnt + 2) * 2 + (RFAT_BLK_SIZE -1)) >> RFAT_BLK_SHIFT;
		    }

		    clus_blkno = blk_unit_size + ((1 + 2 * fat_blkcnt + root_blkcnt + (blk_unit_size -1)) & ~(blk_unit_size -1));
				
		    clscnt = ((blkcnt - clus_blkno) & ~(blk_unit_size -1)) >> cls_blk_shift;

		    if (clscnt > clscnt_e)
		    {
			clscnt = clscnt_e;
		    }
		}
		while (clscnt != clscnt_e);

		root_blkno = (clus_blkno - 32);
		fat2_blkno = root_blkno - fat_blkcnt;
		fat1_blkno = fat2_blkno - fat_blkcnt;
		boot_blkno = fat1_blkno - 1;
	    }
	    else
	    {
		/* SDHC, FAT32 */

		/* Number of Heads and Sectors Per Track ...
		 *
		 * See Microsoft's MBR specs and the term "translation mode".
		 */

		if (blkcnt <= 8388608)
		{
		    hpc = 128;
		    spt = 63;
		}
		else
		{
		    hpc = 255;
		    spt = 63;
		}


		if (blkcnt <= 67108864) /* <= 32GB  -> 4MB  */
		{
		    cls_blk_shift = 6;
		    blk_unit_size = 8192;
			    
		    if (blkcnt <= 33554432)
		    {
			/* For a 16GB SDHC one has to make sure that the FAT1/FAT2 do not
			 * overflow the 1 allocation unit that is available for the 
			 * "System Area".
			 *
			 * (8192 - 9) / 2     = 4091     blocks are available per FAT
			 * (8187 * 128) - 2   = 523646   data clusters
			 * 523646 * 64        = 33513344 data blocks
			 * 33513344 + 8192    = 33521536 total blocks
			 * 33521536 & ~8191   = 33513472 usable blocks
			 */
				
			if (blkcnt > 33513472)
			{
			    blkcnt = 33513472;
			}

			clus_blkno = 16384;
		    }
		    else
		    {
			/* For a 32GB SDHC one has to make sure that the FAT1/FAT2 do not
			 * overflow the a 2 allocation units that are available for the 
			 * "System Area".
			 *
			 * (16384 - 9) / 2    = 8187     blocks are available per FAT
			 * (8187 * 128) - 2   = 1047934  data clusters
			 * 1047934 * 64       = 67067776 data blocks
			 * 67067776 + 8192    = 67075968 total blocks
			 * 67075968 & ~8191   = 67067904 usable blocks
			 */
				
			if (blkcnt > 67067904)
			{
			    blkcnt = 67067904;
			}

			clus_blkno = 24576;
		    }
		}
		else                   /* <= 64GB  -> 16MB */
		{
		    /* The guess "blk_unit_size" for a 64GB card has been confirmed
		     * by looking at various available SDXC cards which all were formatted
		     * the same way.
		     */

		    cls_blk_shift = 7;
		    blk_unit_size = 32768;

		    /* For a 64GB SDXC one has to make sure that the FAT1/FAT2 do not
		     * overflow the single allocation unit that is available for the 
		     * "System Area".
		     *
		     * 16384 / 2          = 8192      blocks are available per FAT
		     * (8192 * 128) - 2   = 1048574   data clusters
		     * 1048574 * 128      = 134217472 data blocks
		     * 134217472 + 49152  = 134266624 total blocks
		     * 134266624 & ~32767 = 134250496 usable blocks
		     *
		     * However a 64GB SDHC can have only 134217728, so there is nothing
		     * to clamp.
		     */

		    clus_blkno = 65536;
		}

		/* FAT32 is layed out different than FAT12/FAT16. There is no root_blkno, and we know
		 * that there are at least 65525 clusters. Also there are at least 9 reserved blocks
		 * minimum.
		 */

		bkboot_blkofs = 6;
		fsinfo_blkofs = 1;

		root_blkcnt = (1 << cls_blk_shift);

		fattype = RFAT_VOLUME_TYPE_FAT32;

		clscnt = ((blkcnt - clus_blkno) & ~(blk_unit_size -1)) >> cls_blk_shift;
		fat_blkcnt = ((clscnt + 2) * 4 + (RFAT_BLK_SIZE -1)) >> RFAT_BLK_SHIFT;

		root_blkno = clus_blkno;
		fat2_blkno = clus_blkno - fat_blkcnt;
		fat1_blkno = fat2_blkno - fat_blkcnt;
		boot_blkno = blk_unit_size;
	    }

		
	    if (status == F_NO_ERROR)
	    {
		volume->type = fattype;

		volume->boot_blkno = boot_blkno;	
		volume->bkboot_blkofs = bkboot_blkofs;
		volume->fsinfo_blkofs = fsinfo_blkofs;
			
		volume->fat_blkcnt = fat_blkcnt;
		volume->fat1_blkno = fat1_blkno;
		volume->fat2_blkno = fat2_blkno;
		volume->root_blkno = root_blkno;
		volume->root_blkcnt = root_blkcnt;
			
		volume->last_clsno = clscnt +1;

		volume->cls_blk_size = 1 << cls_blk_shift;


		blkcnt = clus_blkno + (clscnt << cls_blk_shift);

		tot_sec = (blkcnt - boot_blkno);

		if (fattype != RFAT_VOLUME_TYPE_FAT32)
		{
		    if (tot_sec < 32680)
		    {
			sys_id = 0x01;
		    }
		    else if (tot_sec < 65536)
		    {
			sys_id = 0x04;
		    }
		    else
		    {
			sys_id = 0x06;
		    }
		}
		else
		{
		    /* Select sys_id to get either CHS/LBA or LBA.
		     */
		    if (blkcnt <= 16450560)
		    {
			sys_id = 0x0b;
		    }
		    else
		    {
			sys_id = 0x0c;
		    }
		}

		/* CHS has max legal values (0..1023) * (0..254) * (1..63). If the LBA is outside
		 * this CHS range, then use the maximum value. This can only happen with FAT32,
		 * in which case the partition type signals to use LBA anyway.
		 */
		if (sys_id == 0x0c)
		{
		    start_c = 1023;
		    start_h = 254;
		    start_s = 63;
			
		    end_c   = 1023;
		    end_h   = 254;
		    end_s   = 63;
		}
		else
		{
		    start_c = boot_blkno / (hpc * spt);
		    start_h = (boot_blkno - (start_c * hpc * spt)) / spt;
		    start_s = boot_blkno - (start_c * hpc * spt) - (start_h * spt) + 1;
			
		    end_c = (blkcnt-1) / (hpc * spt);
		    end_h = ((blkcnt-1) - (end_c * hpc * spt)) / spt;
		    end_s = (blkcnt-1) - (end_c * hpc * spt) - (end_h * spt) + 1;
		}

		/* Write the MBR */

		boot = (rfat_boot_t*)((void*)volume->dir_cache.data);
		volume->dir_cache.blkno = RFAT_BLKNO_INVALID;

		memset(boot, 0, RFAT_BLK_SIZE);

		boot->mbr.mbr_par_table[0].mbr_boot_ind     = 0x00;
		boot->mbr.mbr_par_table[0].mbr_start_chs[0] = start_h;
		boot->mbr.mbr_par_table[0].mbr_start_chs[1] = ((start_c << 6) | start_s);
		boot->mbr.mbr_par_table[0].mbr_start_chs[2] = (start_c >> 2);
		boot->mbr.mbr_par_table[0].mbr_sys_id       = sys_id;
		boot->mbr.mbr_par_table[0].mbr_end_chs[0]   = end_h; 
		boot->mbr.mbr_par_table[0].mbr_end_chs[1]   = ((end_c << 6) | end_s);
		boot->mbr.mbr_par_table[0].mbr_end_chs[2]   = (end_c >> 2);
		boot->mbr.mbr_par_table[0].mbr_rel_sec      = RFAT_HTOFL(boot_blkno);
		boot->mbr.mbr_par_table[0].mbr_tot_sec      = RFAT_HTOFL(tot_sec);
		boot->bs.bs_trail_sig                       = RFAT_HTOFS(0xaa55);

		status = rfat_volume_write(volume, 0, (uint8_t*)boot);

		if (status == F_NO_ERROR)
		{
		    /* Write the PBR */
                
		    memset(boot, 0, RFAT_BLK_SIZE);

		    boot->bpb.bs_jmp_boot[0]   = 0xeb;
		    boot->bpb.bs_jmp_boot[1]   = 0x00;
		    boot->bpb.bs_jmp_boot[2]   = 0x90;
		    boot->bpb.bs_oem_name[0]   = ' ';
		    boot->bpb.bs_oem_name[1]   = ' ';
		    boot->bpb.bs_oem_name[2]   = ' ';
		    boot->bpb.bs_oem_name[3]   = ' ';
		    boot->bpb.bs_oem_name[4]   = ' ';
		    boot->bpb.bs_oem_name[5]   = ' ';
		    boot->bpb.bs_oem_name[6]   = ' ';
		    boot->bpb.bs_oem_name[7]   = ' ';
		    boot->bpb.bpb_byts_per_sec = RFAT_HTOFS(RFAT_BLK_SIZE);
		    boot->bpb.bpb_sec_per_clus = (1 << cls_blk_shift);
		    boot->bpb.bpb_rsvd_sec_cnt = RFAT_HTOFS(fat1_blkno - boot_blkno);
		    boot->bpb.bpb_num_fats     = 2;
		    boot->bpb.bpb_media        = 0xf8;
		    boot->bpb.bpb_sec_per_trk  = spt;
		    boot->bpb.bpb_num_heads    = hpc;
		    boot->bs.bs_trail_sig      = RFAT_HTOFS(0xaa55);

		    if (fattype != RFAT_VOLUME_TYPE_FAT32)
		    {
			boot->bpb.bpb_root_ent_cnt   = RFAT_HTOFS(512);
			boot->bpb.bpb_tot_sec_16     = RFAT_HTOFS((tot_sec >= 65536) ? 0 : tot_sec);
			boot->bpb.bpb_fat_sz_16      = RFAT_HTOFS(fat_blkcnt);
			boot->bpb40.bpb_hidd_sec_32  = RFAT_HTOFL(boot_blkno);
			boot->bpb40.bpb_tot_sec_32   = RFAT_HTOFS((tot_sec >= 65536) ? tot_sec : 0);
			boot->bpb40.bs_drv_num       = 0x80;
			boot->bpb40.bs_nt_reserved   = 0x00;
			boot->bpb40.bs_boot_sig      = 0x29;
			boot->bpb40.bs_vol_id        = RFAT_HTOFL(product);

			memcpy(boot->bpb40.bs_vol_lab, "NO NAME    ", sizeof(boot->bpb40.bs_vol_lab));

#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
			if (fattype == RFAT_VOLUME_TYPE_FAT12)
			{
			    memcpy(boot->bpb40.bs_fil_sys_type, "FAT12   ", sizeof(boot->bpb40.bs_fil_sys_type));
			}
			else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
			{
			    memcpy(boot->bpb40.bs_fil_sys_type, "FAT16   ", sizeof(boot->bpb40.bs_fil_sys_type));
			}

			status = rfat_volume_write(volume, boot_blkno, (uint8_t*)boot);
		    }
		    else
		    {
			boot->bpb.bpb_root_ent_cnt   = RFAT_HTOFS(0);
			boot->bpb.bpb_tot_sec_16     = RFAT_HTOFS(0);
			boot->bpb.bpb_fat_sz_16      = RFAT_HTOFS(0);
			boot->bpb71.bpb_hidd_sec_32  = RFAT_HTOFL(boot_blkno);
			boot->bpb71.bpb_tot_sec_32   = RFAT_HTOFL(tot_sec);
			boot->bpb71.bpb_fat_sz_32    = RFAT_HTOFL(fat_blkcnt);
			boot->bpb71.bpb_ext_flags    = RFAT_HTOFS(0x0000);
			boot->bpb71.bpb_fs_ver       = RFAT_HTOFS(0x0000);
			boot->bpb71.bpb_root_clus    = RFAT_HTOFL(2);
			boot->bpb71.bpb_fsinfo       = RFAT_HTOFS(1);
			boot->bpb71.bpb_bkboot       = RFAT_HTOFS(6);
			boot->bpb71.bs_drv_num       = 0x80;
			boot->bpb71.bs_nt_reserved   = 0x00;
			boot->bpb71.bs_boot_sig      = 0x29;
			boot->bpb71.bs_vol_id        = RFAT_HTOFL(product);

			memcpy(boot->bpb71.bs_vol_lab, "NO NAME    ", sizeof(boot->bpb71.bs_vol_lab));
			memcpy(boot->bpb71.bs_fil_sys_type, "FAT32   ", sizeof(boot->bpb71.bs_fil_sys_type));

			status = rfat_volume_write(volume, boot_blkno, (uint8_t*)boot);

			if (status == F_NO_ERROR)
			{
			    status = rfat_volume_write(volume, boot_blkno +6, (uint8_t*)boot);

			    if (status == F_NO_ERROR)
			    {
				fsinfo = (rfat_fsinfo_t*)((void*)volume->dir_cache.data);
				memset(fsinfo, 0, RFAT_BLK_SIZE);

				fsinfo->fsi_lead_sig   = RFAT_HTOFL(0x41615252);
				fsinfo->fsi_struc_sig  = RFAT_HTOFL(0x61417272);
				fsinfo->fsi_free_count = RFAT_HTOFL(0xffffffff);
				fsinfo->fsi_nxt_free   = RFAT_HTOFL(0xffffffff);
				fsinfo->fsi_trail_sig  = RFAT_HTOFL(0xaa550000);
					
				status = rfat_volume_write(volume, boot_blkno +1, (uint8_t*)fsinfo);
					
				if (status == F_NO_ERROR)
				{
				    status = rfat_volume_write(volume, boot_blkno +7, (uint8_t*)fsinfo);

				    if (status == F_NO_ERROR)
				    {
					memset(boot, 0, RFAT_BLK_SIZE);
				    
					boot->bs.bs_trail_sig = RFAT_HTOFS(0xaa55);
						
					status = rfat_volume_write(volume, boot_blkno +2, (uint8_t*)boot);
				    
					if (status == F_NO_ERROR)
					{
					    status = rfat_volume_write(volume, boot_blkno +8, (uint8_t*)boot);
					}
				    }
				}
			    }
			}
		    }

		    if (status == F_NO_ERROR)
		    {
			status = rfat_volume_erase(volume);
		    }
		}
	    }
	}
    }

    return status;
}

static int rfat_volume_erase(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;
    uint32_t clscnt;
    uint32_t boot_blkno, fat1_blkno, fat2_blkno, root_blkno, fat_blkcnt, root_blkcnt;
    uint32_t bkboot_blkofs, fsinfo_blkofs;
    rfat_boot_t *boot;
    rfat_fsinfo_t *fsinfo;
    void *data;

    boot = (rfat_boot_t*)((void*)volume->dir_cache.data);
    volume->dir_cache.blkno = RFAT_BLKNO_INVALID;

    boot_blkno = volume->boot_blkno;	
    bkboot_blkofs = volume->bkboot_blkofs;
    fsinfo_blkofs = volume->fsinfo_blkofs;
    
    fat_blkcnt = volume->fat_blkcnt;
    fat1_blkno = volume->fat1_blkno;
    fat2_blkno = volume->fat2_blkno;

    clscnt = volume->last_clsno -1;

    if (volume->type != RFAT_VOLUME_TYPE_FAT32)
    {
	root_blkno = volume->root_blkno;
	root_blkcnt = volume->root_blkcnt;
    }
    else
    {
	root_blkno = ((fat2_blkno == 0) ? (fat1_blkno + fat_blkcnt) : (fat2_blkno + fat_blkcnt));
	root_blkcnt = volume->cls_blk_size;
	
	status = rfat_volume_read(volume, boot_blkno, (uint8_t*)boot);
	
	if (status == F_NO_ERROR)
	{
	    boot->bpb71.bpb_ext_flags = RFAT_HTOFS(0x0000);
	    boot->bpb71.bpb_root_clus = RFAT_HTOFL(2);
	    
	    if (bkboot_blkofs)
	    {
		status = rfat_volume_write(volume, boot_blkno + bkboot_blkofs, (uint8_t*)boot);
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		status = rfat_volume_write(volume, boot_blkno, (uint8_t*)boot);
	    }
	}
    }

    if (status == F_NO_ERROR)
    {
	/* For FAT32 above root_clsno got forced to be 2, so that fat1/fat2/root are contiguous.
	 */
	status = rfat_volume_zero(volume, fat1_blkno, ((root_blkno - fat1_blkno) + root_blkcnt), NULL);
	
	if (status == F_NO_ERROR)
	{
	    /* Write first FAT1/FAT2 entry */
	    
	    data = (void*)volume->dir_cache.data;
	    memset(data, 0, RFAT_BLK_SIZE);

#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
	    if (volume->type == RFAT_VOLUME_TYPE_FAT12)
	    {
		((uint8_t*)data)[0] = 0xf8;
		((uint8_t*)data)[1] = 0xff;
		((uint8_t*)data)[2] = 0xff;
	    }
	    else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
	    {
		if (volume->type == RFAT_VOLUME_TYPE_FAT16)
		{
		    ((uint16_t*)data)[0] = RFAT_HTOFS(0xfff8);
		    ((uint16_t*)data)[1] = RFAT_HTOFS(0xffff);
		}
		else
		{
		    ((uint32_t*)data)[0] = RFAT_HTOFS(0x0ffffff8);
		    ((uint32_t*)data)[1] = RFAT_HTOFS(0x0fffffff);
		    ((uint32_t*)data)[2] = RFAT_HTOFS(0x0fffffff);
		}
	    }

	    status = rfat_volume_write(volume, fat1_blkno, data);
		
	    if (status == F_NO_ERROR)
	    {
		if (fat2_blkno)
		{
		    status = rfat_volume_write(volume, fat2_blkno, data);
		}
	    }

	    if (fsinfo_blkofs)
	    {
		if (status == F_NO_ERROR)
		{
		    fsinfo = (rfat_fsinfo_t*)((void*)volume->dir_cache.data);
		    memset(fsinfo, 0, RFAT_BLK_SIZE);

		    fsinfo->fsi_lead_sig   = RFAT_HTOFL(0x41615252);
		    fsinfo->fsi_struc_sig  = RFAT_HTOFL(0x61417272);
		    fsinfo->fsi_free_count = RFAT_HTOFL(clscnt);
		    fsinfo->fsi_nxt_free   = RFAT_HTOFL(3);
		    fsinfo->fsi_trail_sig  = RFAT_HTOFL(0xaa550000);
			
		    status = rfat_volume_write(volume, boot_blkno + fsinfo_blkofs, (uint8_t*)fsinfo);
			
		    if (status == F_NO_ERROR)
		    {
			if (bkboot_blkofs)
			{
			    status = rfat_volume_write(volume, boot_blkno + bkboot_blkofs + fsinfo_blkofs, (uint8_t*)fsinfo);
			}
		    }
		}
	    }
	}
    }

    return status;
}

/***********************************************************************************************************************/

static int rfat_dir_cache_write(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

#if (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0)
    if (volume->data_file)
    {
	rfat_file_t *file = volume->data_file;

	status = rfat_disk_write_sequential(volume->disk, volume->dir_cache.blkno, 1, volume->dir_cache.data, &file->status);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
	if (status == F_ERR_INVALIDSECTOR)
	{
	    volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
	}
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

	if (status == F_NO_ERROR)
	{
	    RFAT_VOLUME_STATISTICS_COUNT(data_cache_write);
	}

	/* Unconditionally clean the dirty condition, or
	 * otherwise the while system would get stuck.
	 */
	volume->data_file = NULL;
    }
    else
#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0) */
    {
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) && (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
	if (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
	{
	    status = rfat_map_cache_write(volume, volume->dir_cache.blkno, volume->dir_cache.data);

	    if (status == F_NO_ERROR)
	    {
		RFAT_VOLUME_STATISTICS_COUNT(fat_cache_write);

		volume->flags &= ~RFAT_VOLUME_FLAG_FAT_DIRTY;
	    }
	}
	else
	{
	    status = rfat_volume_write(volume, volume->dir_cache.blkno, volume->dir_cache.data);

	    if (status == F_NO_ERROR)
	    {
		RFAT_VOLUME_STATISTICS_COUNT(dir_cache_write);
	    }
	}
#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) && (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
	status = rfat_volume_write(volume, volume->dir_cache.blkno, volume->dir_cache.data);
	
	if (status == F_NO_ERROR)
	{
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
	    if (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
	    {
		RFAT_VOLUME_STATISTICS_COUNT(fat_cache_write);
		
#if (RFAT_CONFIG_2NDFAT_SUPPORTED == 1)
		if (volume->fat2_blkno)
		{
		    status = rfat_volume_write(volume, volume->dir_cache.blkno + volume->fat_blkcnt, volume->dir_cache.data);
		}
		
		if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_2NDFAT_SUPPORTED == 1) */
		{
		    volume->flags &= ~RFAT_VOLUME_FLAG_FAT_DIRTY;
		}
	    }
	    else
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
	    {
		RFAT_VOLUME_STATISTICS_COUNT(dir_cache_write);
	    }
	}
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) && (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
    }

    return status;
}

static int rfat_dir_cache_fill(rfat_volume_t *volume, uint32_t blkno, int zero)
{
    int status = F_NO_ERROR;

#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) || (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0)
    if (0
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
	|| (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
#if (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0)
	|| volume->data_file
#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0) */
	)
    {
        status = rfat_dir_cache_write(volume);
    }

    if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) || (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0) */
    {
	if (zero)
	{
	    memset(volume->dir_cache.data, 0, RFAT_BLK_SIZE);

	    volume->dir_cache.blkno = blkno;
	}
	else
	{
	    status = rfat_volume_read(volume, blkno, volume->dir_cache.data);
	    
	    if (status == F_NO_ERROR)
	    {
		volume->dir_cache.blkno = blkno;
	    }
	}
    }

    return status;
}

static int rfat_dir_cache_read(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    if (volume->dir_cache.blkno != blkno)
    {
	RFAT_VOLUME_STATISTICS_COUNT(dir_cache_miss);
	RFAT_VOLUME_STATISTICS_COUNT(dir_cache_read);

	status = rfat_dir_cache_fill(volume, blkno, FALSE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(dir_cache_hit);
    }

    *p_entry = &volume->dir_cache;

    return status;
}

static int rfat_dir_cache_zero(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    if (volume->dir_cache.blkno != blkno)
    {
	RFAT_VOLUME_STATISTICS_COUNT(dir_cache_miss);
	RFAT_VOLUME_STATISTICS_COUNT(dir_cache_zero);

	status = rfat_dir_cache_fill(volume, blkno, TRUE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(dir_cache_hit);
    }

    *p_entry = &volume->dir_cache;

    return status;
}

static int rfat_dir_cache_flush(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) || (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0)
    if (0
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
	|| (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
#if (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0)
	|| volume->data_file
#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0) */
	)
    {
#if (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0)
	if (volume->data_file)
	{
	    RFAT_VOLUME_STATISTICS_COUNT(data_cache_flush);
	}
#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0) */

#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
	if (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
	{
	    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_flush);
	}
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */

	status = rfat_dir_cache_write(volume);
    }
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) || (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0) */

    return status;
}

/***********************************************************************************************************************/

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)

static int rfat_map_cache_fill(rfat_volume_t *volume, uint32_t page)
{
    int status = F_NO_ERROR;

    if (volume->map_flags & RFAT_MAP_FLAG_MAP_DIRTY)
    {
	status = rfat_volume_write(volume, volume->map_cache.blkno, volume->map_cache.data);
    }
	    
    if (status == F_NO_ERROR)
    {
	if (!(volume->map_flags & (RFAT_MAP_FLAG_MAP_0_CHANGED << page)))
	{
	    memset(volume->map_cache.data, 0, RFAT_BLK_SIZE);
	    
	    volume->map_cache.blkno = volume->map_blkno + page;
	}
	else
	{
	    status = rfat_volume_read(volume, (volume->map_blkno + page), volume->map_cache.data);

	    if (status == F_NO_ERROR)
	    {
		volume->map_cache.blkno = volume->map_blkno + page;
	    }
	}
    }

    return status;
}

static int rfat_map_cache_write(rfat_volume_t *volume, uint32_t blkno, const uint8_t *data)
{
    int status = F_NO_ERROR;
    uint32_t page, index, mask, offset;
    uint32_t *map;

    page   = (blkno - volume->fat1_blkno) >> (RFAT_BLK_SHIFT + 8);
    index  = ((blkno - volume->fat1_blkno) >> 5) & RFAT_BLK_MASK;
    mask   = 1ul << ((blkno - volume->fat1_blkno) & 31);
    offset = volume->fat_blkcnt;

    if (volume->map_cache.blkno != (volume->map_blkno + page))
    {
	status = rfat_map_cache_fill(volume, page);
    }

    if (status == F_NO_ERROR)
    {
	map = (uint32_t*)((void*)volume->map_cache.data);
	
	if (!(map[index] & mask))
	{
	    map[index] |= mask;
	    
	    volume->map_flags |= (RFAT_MAP_FLAG_MAP_DIRTY | (RFAT_MAP_FLAG_MAP_0_CHANGED << page));
	    
	    if (volume->map_entries >= RFAT_MAP_TABLE_ENTRIES)
	    {
		volume->map_entries = RFAT_MAP_TABLE_OVERFLOW;
	    }
	    else
	    {
		volume->map_table[volume->map_entries++] = (blkno - volume->fat1_blkno);
	    }
	}

	status = rfat_volume_write(volume, (blkno + offset), data);
    }

    return status;
}

static int rfat_map_cache_read(rfat_volume_t *volume, uint32_t blkno, uint8_t *data)
{
    int status = F_NO_ERROR;
    uint32_t page, index, mask, offset;
    uint32_t *map;

    page   = (blkno - volume->fat1_blkno) >> (RFAT_BLK_SHIFT + 8);
    index  = ((blkno - volume->fat1_blkno) >> 5) & RFAT_BLK_MASK;
    mask   = 1ul << ((blkno - volume->fat1_blkno) & 31);
    offset = 0;

    if (volume->map_cache.blkno != (volume->map_blkno + page))
    {
	status = rfat_map_cache_fill(volume, page);
    }

    if (status == F_NO_ERROR)
    {
	map = (uint32_t*)((void*)volume->map_cache.data);
	
	if (map[index] & mask)
	{
	    offset = volume->fat_blkcnt;
	}

	status = rfat_volume_read(volume, (blkno + offset), data);
    }

    return status;
}

static int rfat_map_cache_flush(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

    status = rfat_fat_cache_flush(volume);

    if (status == F_NO_ERROR)
    {
	if (volume->map_entries == RFAT_MAP_TABLE_OVERFLOW)
	{
	    if (volume->map_flags & RFAT_MAP_FLAG_MAP_DIRTY)
	    {
		if (volume->type != RFAT_VOLUME_TYPE_FAT32)
		{
		    /* For FAT12/FAT16 the map is stored in volume->map_table[]. There
		     * can be at most 256 FAT blocks for FAT32, which means 256 bits,
		     * which is 64 bytes. Hence it fits into volume->map_table[].
		     */
		    memcpy(volume->map_table, volume->map_cache.data, 64);
		}
		else
		{
		    status = rfat_volume_write(volume, volume->map_cache.blkno, volume->map_cache.data);
		}
	    }
	}

	if (status == F_NO_ERROR)
	{
	    volume->map_flags &= ~RFAT_MAP_FLAG_MAP_DIRTY;

#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	    if ((volume->map_flags & RFAT_MAP_FLAG_MAP_CHANGED) &&
		(volume->fsinfo_blkofs != 0) &&
		(volume->flags & RFAT_VOLUME_FLAG_FSINFO_VALID) &&
		(volume->flags & RFAT_VOLUME_FLAG_FSINFO_DIRTY))
	    {
		volume->map_flags |= RFAT_MAP_FLAG_MAP_FSINFO;
	    }
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
	}
    }

    return status;
}

static int rfat_map_cache_resolve(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;
    uint16_t *map_table, *map_table_e;
    uint32_t blkno, blkno_n, mask, page;
    uint32_t *map, *map_e;
    uint8_t *data;
    rfat_cache_entry_t *entry;

    status = rfat_dir_cache_flush(volume);

    if (status == F_NO_ERROR)
    {
	if (volume->map_entries != RFAT_MAP_TABLE_OVERFLOW)
	{
	    map_table = &volume->map_table[0];
	    map_table_e = &volume->map_table[volume->map_entries];

	    do
	    {
		blkno = volume->fat1_blkno + *map_table++;

#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
		if (blkno == volume->dir_cache.blkno)
		{
		    data = volume->dir_cache.data;
		}
#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1)
		if (blkno == volume->fat_cache.blkno)
		{
		    data = volume->fat_cache.data;
		}
#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
		if (blkno == volume->fat_cache[0].blkno)
		{
		    data = volume->fat_cache[0].data;
		}
		else if (blkno == volume->fat_cache[1].blkno)
		{
		    data = volume->fat_cache[1].data;
		}
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
		else
		{
		    status = rfat_dir_cache_read(volume, blkno + volume->fat_blkcnt, &entry);

		    if (status == F_NO_ERROR)
		    {
			entry->blkno = blkno;

			data = entry->data;
		    }
		}

		if (status == F_NO_ERROR)
		{
		    status = rfat_volume_write(volume, blkno, data);
		}
	    }
	    while ((status == F_NO_ERROR) && (map_table < map_table_e));
	}
	else
	{
	    do
	    {
		blkno = volume->fat1_blkno;

		if (volume->type != RFAT_VOLUME_TYPE_FAT32)
		{
		    /* For FAT12/FAT16 the map is stored in volume->map_table[]. There
		     * can be at most 256 FAT blocks for FAT32, which means 256 bits,
		     * which is 64 bytes. Hence it fits into volume->map_table[].
		     */
		    page = 0;

		    map = (uint32_t*)((void*)volume->map_table);
		    map_e = (uint32_t*)((void*)((uint8_t*)volume->map_table + 64));
		}
		else
		{
		    if (volume->map_flags & (RFAT_MAP_FLAG_MAP_0_CHANGED << (volume->map_cache.blkno - volume->map_blkno)))
		    {
			page = (volume->map_cache.blkno - volume->map_blkno);
		    }
		    else
		    {
			if (volume->map_flags & RFAT_MAP_FLAG_MAP_0_CHANGED)
			{
			    page = 0;
			}
			else
			{
			    page = 1;
			}

			if (volume->map_cache.blkno != (volume->map_blkno + page))
			{
			    status = rfat_map_cache_fill(volume, page);
			}
		    }

		    if (page == 1)
		    {
			blkno += (RFAT_BLK_SIZE * 8);
		    }
		    
		    map = (uint32_t*)((void*)volume->map_cache.data);
		    map_e = (uint32_t*)((void*)(volume->map_cache.data + RFAT_BLK_SIZE));
		}

		while ((status == F_NO_ERROR) && (map < map_e))
		{
		    mask = *map++;

		    blkno_n  = blkno + 32;
		    
		    while (mask)
		    {
			if (mask & 1)
			{
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
			    if (blkno == volume->dir_cache.blkno)
			    {
				data = volume->dir_cache.data;
			    }
#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1)
			    if (blkno == volume->fat_cache.blkno)
			    {
				data = volume->fat_cache.data;
			    }
#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
			    if (blkno == volume->fat_cache[0].blkno)
			    {
				data = volume->fat_cache[0].data;
			    }
			    else if (blkno == volume->fat_cache[1].blkno)
			    {
				data = volume->fat_cache[1].data;
			    }
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
			    else
			    {
				status = rfat_dir_cache_read(volume, blkno + volume->fat_blkcnt, &entry);

				if (status == F_NO_ERROR)
				{
				    entry->blkno = blkno;

				    data = entry->data;
				}
			    }
			    
			    if (status == F_NO_ERROR)
			    {
				status = rfat_volume_write(volume, blkno, data);
			    }
			}

			mask >>= 1;
			blkno++;
		    }

		    blkno = blkno_n;
		}

		if (status == F_NO_ERROR)
		{
		    volume->map_flags &= ~(RFAT_MAP_FLAG_MAP_0_CHANGED << page);
		}
	    }
	    while ((status == F_NO_ERROR) && (volume->map_flags & RFAT_MAP_FLAG_MAP_CHANGED));
	}

	if (status == F_NO_ERROR)
	{
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	    if (volume->map_flags & RFAT_MAP_FLAG_MAP_FSINFO)
	    {
		volume->flags |= (RFAT_VOLUME_FLAG_FSINFO_VALID | RFAT_VOLUME_FLAG_FSINFO_DIRTY);
	    }
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */

	    volume->map_flags = 0;
	    volume->map_entries = 0;

	    volume->map_cache.blkno = RFAT_BLKNO_INVALID;

	    if (volume->type != RFAT_VOLUME_TYPE_FAT32)
	    {
		/* For FAT12/FAT16 the map is stored in volume->map_table[]. There
		 * can be at most 256 FAT blocks for FAT32, which means 256 bits,
		 * which is 64 bytes. Hence it fits into volume->map_table[].
		 */
		memset(volume->map_cache.data, 0, 64);
	    }

	}
    }

    return status;
}

#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

/***********************************************************************************************************************/

#if (RFAT_CONFIG_FAT_CACHE_ENTRIES != 0)
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1)

static int rfat_fat_cache_write(rfat_volume_t *volume, rfat_cache_entry_t *entry)
{
    int status = F_NO_ERROR;

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
    status = rfat_map_cache_write(volume, volume->fat_cache.blkno, volume->fat_cache.data);
#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
    status = rfat_volume_write(volume, volume->fat_cache.blkno, volume->fat_cache.data);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
                
    if (status == F_NO_ERROR)
    {
#if (RFAT_CONFIG_2NDFAT_SUPPORTED == 1) && (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) 
	if (volume->fat2_blkno)
	{
	    status = rfat_volume_write(volume, volume->fat_cache.blkno + volume->fat_blkcnt, volume->fat_cache.data);
	}

	if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_2NDFAT_SUPPORTED == 1) && (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
	{
	    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_write);

	    volume->flags &= ~RFAT_VOLUME_FLAG_FAT_DIRTY;
	}
    }

    return status;
}

static int rfat_fat_cache_fill(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    if (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
    {
	status = rfat_fat_cache_write(volume, &volume->fat_cache);
    }

    if (status == F_NO_ERROR)
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_miss);

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
	status = rfat_map_cache_read(volume, blkno, volume->fat_cache.data);
#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
	status = rfat_volume_read(volume, blkno, volume->fat_cache.data);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
	
	if (status == F_NO_ERROR)
	{
	    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_read);

	    volume->fat_cache.blkno = blkno;
	}
    }

    *p_entry = &volume->fat_cache;

    return status;
}

static int rfat_fat_cache_read(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    if (volume->fat_cache.blkno != blkno)
    {
	status = rfat_fat_cache_fill(volume, blkno, p_entry);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_hit);

	*p_entry = &volume->fat_cache;
    }

    return status;
}

static inline void rfat_fat_cache_modify(rfat_volume_t *volume, rfat_cache_entry_t *entry)
{
    volume->flags |= RFAT_VOLUME_FLAG_FAT_DIRTY;
}

static int rfat_fat_cache_flush(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

    if (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_flush);

	status = rfat_fat_cache_write(volume, &volume->fat_cache);
    }

    return status;
}

#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */

static int rfat_fat_cache_write(rfat_volume_t *volume, rfat_cache_entry_t *entry)
{
    int status = F_NO_ERROR;

    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_write);

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
    status = rfat_map_cache_write(volume, entry->blkno, entry->data);
#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
    status = rfat_volume_write(volume, entry->blkno, entry->data);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

    if (status == F_NO_ERROR)
    {
#if (RFAT_CONFIG_2NDFAT_SUPPORTED == 1) && (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
	if (volume->fat2_blkno)
	{
	    status = rfat_volume_write(volume, entry->blkno + volume->fat_blkcnt, entry->data);
	}

	if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_2NDFAT_SUPPORTED == 1) && (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
	{
	    volume->flags &= ((entry == &volume->fat_cache[0]) ? ~RFAT_VOLUME_FLAG_FAT_0_DIRTY : ~RFAT_VOLUME_FLAG_FAT_1_DIRTY);
	}
    }

    return status;
}

static int rfat_fat_cache_fill(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;
    unsigned int index;

    index = (volume->flags & RFAT_VOLUME_FLAG_FAT_INDEX_CURRENT) ^ RFAT_VOLUME_FLAG_FAT_INDEX_CURRENT;

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
    /* For a non-TRANSACTION_SAFE setup, the write backs from the cache
     * have to be in sequence. That means, if there is any cache entry
     * dirty it have to be written back, before either a new one gets 
     * read or the non least recently used one is used.
     */
    if (volume->flags & (RFAT_VOLUME_FLAG_FAT_0_DIRTY | RFAT_VOLUME_FLAG_FAT_1_DIRTY))
    {
	status = rfat_fat_cache_write(volume, &volume->fat_cache[index ^ RFAT_VOLUME_FLAG_FAT_INDEX_CURRENT]);
    }

    if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
    {
	if (volume->fat_cache[index].blkno != blkno)
	{
	    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_miss);
	    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_read);
	    
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
	    if (volume->flags & (RFAT_VOLUME_FLAG_FAT_0_DIRTY << index))
	    {
		status = rfat_fat_cache_write(volume, &volume->fat_cache[index]);
	    }
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

	    if (status == F_NO_ERROR)
	    {
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
		status = rfat_map_cache_read(volume, blkno, volume->fat_cache[index].data);
#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
		status = rfat_volume_read(volume, blkno, volume->fat_cache[index].data);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
	    
		if (status == F_NO_ERROR)
		{
		    volume->flags ^= RFAT_VOLUME_FLAG_FAT_INDEX_CURRENT;
		
		    volume->fat_cache[index].blkno = blkno;
		}
	    }
	}
	else
	{
	    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_hit);

	    volume->flags ^= RFAT_VOLUME_FLAG_FAT_INDEX_CURRENT;
	}
    }

    *p_entry = &volume->fat_cache[index];

    return status;
}

static int rfat_fat_cache_read(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;
    unsigned int index;

    index = volume->flags & RFAT_VOLUME_FLAG_FAT_INDEX_CURRENT;

    if (volume->fat_cache[index].blkno != blkno)
    {
	status = rfat_fat_cache_fill(volume, blkno, p_entry);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_hit);

	*p_entry = &volume->fat_cache[index];
    }

    return status;
}

static inline void rfat_fat_cache_modify(rfat_volume_t *volume, rfat_cache_entry_t *entry)
{
    volume->flags |= ((entry == &volume->fat_cache[0]) ? RFAT_VOLUME_FLAG_FAT_0_DIRTY : RFAT_VOLUME_FLAG_FAT_1_DIRTY);
}

static int rfat_fat_cache_flush(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

    if (volume->flags & RFAT_VOLUME_FLAG_FAT_0_DIRTY)
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_flush);

	status = rfat_fat_cache_write(volume, &volume->fat_cache[0]);
    }

    if ((status == F_NO_ERROR) && (volume->flags & RFAT_VOLUME_FLAG_FAT_1_DIRTY))
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_flush);

	status = rfat_fat_cache_write(volume, &volume->fat_cache[1]);
    }

    return status;
}

#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 1) */

#else /* (RFAT_CONFIG_FAT_CACHE_ENTRIES != 0) */

static inline int rfat_fat_cache_write(rfat_volume_t *volume, rfat_cache_entry_t *entry)
{
    return rfat_dir_cache_write(volume);
}

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)

static int rfat_fat_cache_fill(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    if ((volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY) 
#if (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0)
	|| volume->data_file
#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES == 0) */
	)
    {
	status = rfat_dir_cache_write(volume);
    }

    if (status == F_NO_ERROR)
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_miss);

	status = rfat_map_cache_read(volume, blkno, volume->dir_cache.data);
	
	if (status == F_NO_ERROR)
	{
	    RFAT_VOLUME_STATISTICS_COUNT(fat_cache_read);

	    volume->dir_cache.blkno = blkno;
	}
    }

    *p_entry = &volume->dir_cache;

    return status;
}

static int rfat_fat_cache_read(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    if (volume->dir_cache.blkno != blkno)
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_miss);
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_read);

	status = rfat_fat_cache_fill(volume, blkno, p_entry);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_hit);

	*p_entry = &volume->dir_cache;
    }

    return status;
}

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

static int rfat_fat_cache_fill(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    status = rfat_dir_cache_fill(volume, blkno, FALSE);

    *p_entry = &volume->dir_cache;

    return status;
}

#if (RFAT_CONFIG_STATISTICS == 1)

static int rfat_fat_cache_read(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    int status = F_NO_ERROR;

    if (volume->dir_cache.blkno != blkno)
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_miss);
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_read);

	status = rfat_dir_cache_fill(volume, blkno, FALSE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_hit);
    }

    *p_entry = &volume->dir_cache;

    return status;
}

#else /* (RFAT_CONFIG_STATISTICS == 1) */

static inline int rfat_fat_cache_read(rfat_volume_t *volume, uint32_t blkno, rfat_cache_entry_t **p_entry)
{
    return rfat_dir_cache_read(volume, blkno, p_entry);
}

#endif /* (RFAT_CONFIG_STATISTICS == 1) */

#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

static inline void rfat_fat_cache_modify(rfat_volume_t *volume, rfat_cache_entry_t *entry)
{
    volume->flags |= RFAT_VOLUME_FLAG_FAT_DIRTY;
}

static int rfat_fat_cache_flush(rfat_volume_t *volume)
{
    int status = F_NO_ERROR;

    if (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
    {
	RFAT_VOLUME_STATISTICS_COUNT(fat_cache_flush);

	status = rfat_dir_cache_write(volume);
    }

    return status;
}

#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES != 0) */

/***********************************************************************************************************************/

#if (RFAT_CONFIG_DATA_CACHE_ENTRIES != 0)
#if (RFAT_CONFIG_FILE_DATA_CACHE == 1)

static int rfat_data_cache_write(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    status = rfat_disk_write_sequential(volume->disk, file->data_cache.blkno, 1, file->data_cache.data, &file->status);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
    if (status == F_ERR_INVALIDSECTOR)
    {
	volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

    if (status == F_NO_ERROR)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_write);
    }

    /* Unconditionally clean the dirty condition, or
     * otherwise the while system would get stuck.
     */
    file->flags &= ~RFAT_FILE_FLAG_DATA_DIRTY;

    /* After any data related disk operation, "file->status" needs
     * to be checked asynchronously for a previous error.
     */

    if (file->status != F_NO_ERROR)
    {
	status = file->status;
    }

    return status;
}

static int rfat_data_cache_fill(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, int zero)
{
    int status = F_NO_ERROR;

    if (file->flags & RFAT_FILE_FLAG_DATA_DIRTY)
    {
	status = rfat_data_cache_write(volume, file);
    }

    if (status == F_NO_ERROR)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_miss);

	if (zero)
	{
	    memset(file->data_cache.data, 0, RFAT_BLK_SIZE);

	    RFAT_VOLUME_STATISTICS_COUNT(data_cache_zero);

	    file->data_cache.blkno = blkno;
	}
	else
	{
            status = rfat_disk_read_sequential(volume->disk, blkno, 1, file->data_cache.data);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
	    if (status == F_ERR_INVALIDSECTOR)
	    {
		volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
	    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */
            
            if (status == F_NO_ERROR)
            {
		RFAT_VOLUME_STATISTICS_COUNT(data_cache_read);

                file->data_cache.blkno = blkno;
            }

	    /* After any data related disk operation, "file->status" needs
	     * to be checked asynchronously for a previous error.
	     */
	    if (file->status != F_NO_ERROR)
	    {
		status = file->status;
	    }
	}
    }

    return status;
}

static int rfat_data_cache_read(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, rfat_cache_entry_t ** p_entry)
{
    int status = F_NO_ERROR;

    if (file->data_cache.blkno != blkno)
    {
	status = rfat_data_cache_fill(volume, file, blkno, FALSE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_hit);
    }

    *p_entry = &file->data_cache;
    
    return status;
}

static int rfat_data_cache_zero(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, rfat_cache_entry_t ** p_entry)
{
    int status = F_NO_ERROR;

    if (file->data_cache.blkno != blkno)
    {
	status = rfat_data_cache_fill(volume, file, blkno, TRUE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_hit);
    }

    *p_entry = &file->data_cache;
    
    return status;
}

static inline void rfat_data_cache_modify(rfat_volume_t *volume, rfat_file_t *file)
{
    file->flags |= RFAT_FILE_FLAG_DATA_DIRTY;
}

static int rfat_data_cache_flush(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    if (file->flags & RFAT_FILE_FLAG_DATA_DIRTY)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_flush);

	status = rfat_data_cache_write(volume, file);
    }

    return status;
}

static int rfat_data_cache_invalidate(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, uint32_t blkcnt)
{
    int status = F_NO_ERROR;

    if ((blkno <= file->data_cache.blkno) && (file->data_cache.blkno < (blkno + blkcnt)))
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_invalidate);

	file->data_cache.blkno = RFAT_BLKNO_INVALID;

	file->flags &= ~RFAT_FILE_FLAG_DATA_DIRTY;
    }

    return status;
}

#else /* (RFAT_CONFIG_FILE_DATA_CACHE == 1) */

static int rfat_data_cache_write(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    status = rfat_disk_write_sequential(volume->disk, volume->data_cache.blkno, 1, volume->data_cache.data, &file->status);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
    if (status == F_ERR_INVALIDSECTOR)
    {
	volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

    if (status == F_NO_ERROR)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_write);
    }

    /* Unconditionally clean the dirty condition, or
     * otherwise the while system would get stuck.
     */
    volume->data_file = NULL;

    /* After any data related disk operation, "file->status" needs
     * to be checked asynchronously for a previous error.
     */
    if (file->status != F_NO_ERROR)
    {
	status = file->status;
    }

    return status;
}

static int rfat_data_cache_fill(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, int zero)
{
    int status = F_NO_ERROR;

    if (volume->data_file)
    {
	status = rfat_data_cache_write(volume, file);
    }

    if (status == F_NO_ERROR)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_miss);

	if (zero)
	{
	    memset(volume->data_cache.data, 0, RFAT_BLK_SIZE);

	    RFAT_VOLUME_STATISTICS_COUNT(data_cache_zero);

	    volume->data_cache.blkno = blkno;
	}
	else
	{
            status = rfat_disk_read_sequential(volume->disk, blkno, 1, volume->data_cache.data);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
	    if (status == F_ERR_INVALIDSECTOR)
	    {
		volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
	    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */
            
            if (status == F_NO_ERROR)
            {
		RFAT_VOLUME_STATISTICS_COUNT(data_cache_read);

                volume->data_cache.blkno = blkno;
            }

	    /* After any data related disk operation, "file->status" needs
	     * to be checked asynchronously for a previous error.
	     */
	    if (file->status != F_NO_ERROR)
	    {
		status = file->status;
	    }
	}
    }

    return status;
}

static int rfat_data_cache_read(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, rfat_cache_entry_t ** p_entry)
{
    int status = F_NO_ERROR;

    if (volume->data_cache.blkno != blkno)
    {
	status = rfat_data_cache_fill(volume, file, blkno, FALSE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_hit);
    }

    *p_entry = &volume->data_cache;
    
    return status;
}

static int rfat_data_cache_zero(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, rfat_cache_entry_t ** p_entry)
{
    int status = F_NO_ERROR;

    if (volume->data_cache.blkno != blkno)
    {
	status = rfat_data_cache_fill(volume, file, blkno, TRUE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_hit);
    }

    *p_entry = &volume->data_cache;
    
    return status;
}

static inline void rfat_data_cache_modify(rfat_volume_t *volume, rfat_file_t *file)
{
    volume->data_file = file;
}

static int rfat_data_cache_flush(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    if (volume->data_file == file)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_flush);

	status = rfat_data_cache_write(volume, file);
    }

    return status;
}

static int rfat_data_cache_invalidate(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, uint32_t blkcnt)
{
    int status = F_NO_ERROR;

    if ((blkno <= volume->data_cache.blkno) && (volume->data_cache.blkno < (blkno + blkcnt)))
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_invalidate);

	volume->data_cache.blkno = RFAT_BLKNO_INVALID;

	volume->data_file = NULL;
    }

    return status;
}

#endif /* (RFAT_CONFIG_FILE_DATA_CACHE == 1) */

#else /* (RFAT_CONFIG_DATA_CACHE_ENTRIES != 0) */

static int rfat_data_cache_write(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    status = rfat_dir_cache_write(volume);

    /* After any data related disk operation, "file->status" needs
     * to be checked asynchronously for a previous error.
     */
    if (file->status != F_NO_ERROR)
    {
	status = file->status;
    }

    return status;
}

static int rfat_data_cache_fill(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, int zero)
{
    int status = F_NO_ERROR;

    if (volume->data_file
#if (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0)
	|| (volume->flags & RFAT_VOLUME_FLAG_FAT_DIRTY)
#endif /* (RFAT_CONFIG_FAT_CACHE_ENTRIES == 0) */
	)
    {
	status = rfat_dir_cache_write(volume);
    }

    if (status == F_NO_ERROR)
    {
	if (zero)
	{
	    memset(volume->dir_cache.data, 0, RFAT_BLK_SIZE);

	    volume->dir_cache.blkno = blkno;
	}
	else
	{
            status = rfat_disk_read_sequential(volume->disk, blkno, 1, volume->dir_cache.data);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
	    if (status == F_ERR_INVALIDSECTOR)
	    {
		volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
	    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */
            
            if (status == F_NO_ERROR)
            {
                volume->dir_cache.blkno = blkno;
            }
	}
    }

    /* After any data related disk operation, "file->status" needs
     * to be checked asynchronously for a previous error.
     */
    if (file->status != F_NO_ERROR)
    {
	status = file->status;
    }

    return status;
}

static int rfat_data_cache_read(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, rfat_cache_entry_t ** p_entry)
{
    int status = F_NO_ERROR;

    if (volume->dir_cache.blkno != blkno)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_miss);
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_read);

	status = rfat_data_cache_fill(volume, file, blkno, FALSE);
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_hit);
    }

    *p_entry = &volume->dir_cache;
    
    return status;
}

static int rfat_data_cache_zero(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, rfat_cache_entry_t ** p_entry)
{
    int status = F_NO_ERROR;

    if (volume->dir_cache.blkno != blkno)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_miss);
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_zero);

	status = rfat_data_cache_fill(volume, file, blkno, TRUE);
    }

    *p_entry = &volume->dir_cache;
    
    return status;
}

static inline void rfat_data_cache_modify(rfat_volume_t *volume, rfat_file_t *file)
{
    volume->data_file = file;
}

static int rfat_data_cache_flush(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    if (volume->data_file == file)
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_flush);

	status = rfat_data_cache_write(volume, file);
    }

    return status;
}

static int rfat_data_cache_invalidate(rfat_volume_t *volume, rfat_file_t *file, uint32_t blkno, uint32_t blkcnt)
{
    int status = F_NO_ERROR;


    if ((blkno <= volume->dir_cache.blkno) && (volume->dir_cache.blkno < (blkno + blkcnt)))
    {
	RFAT_VOLUME_STATISTICS_COUNT(data_cache_invalidate);

	volume->dir_cache.blkno = RFAT_BLKNO_INVALID;

	volume->data_file = NULL;
    }

    return status;
}

#endif /* (RFAT_CONFIG_DATA_CACHE_ENTRIES != 0) */

/***********************************************************************************************************************/

static int rfat_cluster_read_uncached(rfat_volume_t *volume, uint32_t clsno, uint32_t *p_clsdata)
{
    int status = F_NO_ERROR;
    uint32_t offset, blkno, clsdata;
    rfat_cache_entry_t *entry;

#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
    if (volume->type == RFAT_VOLUME_TYPE_FAT12)
    {
	uint8_t *fat_data;

	offset = clsno + (clsno >> 1);
	blkno = volume->fat1_blkno + (offset >> RFAT_BLK_SHIFT);

	status = rfat_fat_cache_read(volume, blkno, &entry);
	
	if (status == F_NO_ERROR)
	{
	    fat_data = (uint8_t*)(entry->data + (offset & RFAT_BLK_MASK));
	    
	    if (clsno & 1)
	    {
		clsdata = ((uint32_t)(*fat_data) >> 4);
	    }
	    else
	    {
		clsdata = ((uint32_t)(*fat_data));
	    }
	    
	    if ((offset & RFAT_BLK_MASK) == RFAT_BLK_MASK)
	    {
		status = rfat_fat_cache_read(volume, (blkno+1), &entry);
		
		if (status == F_NO_ERROR)
		{
		    fat_data = (uint8_t*)(entry->data + 0);
		}
	    }
	    else
	    {
		fat_data++;
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		if (clsno & 1)
		{
		    clsdata |= ((uint32_t)(*fat_data) << 4);
		}
		else
		{
		    clsdata |= (((uint32_t)(*fat_data) & 0x0000000fu) << 8);
		}
	    
		if (clsdata >= RFAT_CLSNO_RESERVED12)
		{
		    clsdata += (RFAT_CLSNO_RESERVED32 - RFAT_CLSNO_RESERVED12);
		}

		*p_clsdata = clsdata;
	    }
	}
    }
    else
#endif /* RFAT_CONFIG_FAT12_SUPPORTED == 1 */
    {
	offset = clsno << volume->type;
	blkno = volume->fat1_blkno + (offset >> RFAT_BLK_SHIFT);

	status = rfat_fat_cache_read(volume, blkno, &entry);
	    
	if (status == F_NO_ERROR)
	{
	    if (volume->type == RFAT_VOLUME_TYPE_FAT16)
	    {
		uint16_t *fat_data;

		fat_data = (uint16_t*)((void*)(entry->data + (offset & RFAT_BLK_MASK)));
		
		clsdata = RFAT_FTOHS(*fat_data);
		
		if (clsdata >= RFAT_CLSNO_RESERVED16)
		{
		    clsdata += (RFAT_CLSNO_RESERVED32 - RFAT_CLSNO_RESERVED16);
		}
	    }
	    else
	    {
		uint32_t *fat_data;
	    
		fat_data = (uint32_t*)((void*)(entry->data + (offset & RFAT_BLK_MASK)));
		
		clsdata = RFAT_FTOHL(*fat_data) & 0x0fffffff;
	    }

	    *p_clsdata = clsdata;
	}
    }

    return status;
}

#if (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0)

static int rfat_cluster_read(rfat_volume_t *volume, uint32_t clsno, uint32_t *p_clsdata)
{
    int status = F_NO_ERROR;
    uint32_t clsdata;
    unsigned int index;

    index = clsno % RFAT_CONFIG_CLUSTER_CACHE_ENTRIES;

    if (volume->cluster_cache[index].clsno != clsno)
    {
	RFAT_VOLUME_STATISTICS_COUNT(cluster_cache_miss);

	status = rfat_cluster_read_uncached(volume, clsno, &clsdata);

	volume->cluster_cache[index].clsno = clsno;
	volume->cluster_cache[index].clsdata = clsdata;
    }
    else
    {
	RFAT_VOLUME_STATISTICS_COUNT(cluster_cache_hit);
    }

    *p_clsdata = volume->cluster_cache[index].clsdata;

    return status;
}

#else /* (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0) */

static inline int rfat_cluster_read(rfat_volume_t *volume, uint32_t clsno, uint32_t *p_clsdata)
{
    return rfat_cluster_read_uncached(volume, clsno, p_clsdata);
}

#endif /* (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0) */

static int rfat_cluster_write(rfat_volume_t *volume, uint32_t clsno, uint32_t clsdata, int allocate)
{
    int status = F_NO_ERROR;
    uint32_t offset, blkno;
    rfat_cache_entry_t *entry;

#if (RFAT_CONFIG_FAT12_SUPPORTED == 1)
    if (volume->type == RFAT_VOLUME_TYPE_FAT12)
    {
	uint8_t *fat_data;

	offset = clsno + (clsno >> 1);
	blkno = volume->fat1_blkno + (offset >> RFAT_BLK_SHIFT);
	
	status = rfat_fat_cache_read(volume, blkno, &entry);

	if (status == F_NO_ERROR)
	{
	    fat_data = (uint8_t*)(entry->data + (offset & RFAT_BLK_MASK));
	    
	    if (clsno & 1)
	    {
		*fat_data = (*fat_data & 0x0f) | (clsdata << 4);
	    }
	    else
	    {
		*fat_data = clsdata;
	    }
	    
	    if ((offset & RFAT_BLK_MASK) == RFAT_BLK_MASK)
	    {
		rfat_fat_cache_modify(volume, entry);
		
		status = rfat_fat_cache_read(volume, (blkno+1), &entry);
		
		if (status == F_NO_ERROR)
		{
		    fat_data = (uint8_t*)(entry->data + 0);
		}
	    }
	    else
	    {
		fat_data++;
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		if (clsno & 1)
		{
		    *fat_data = clsdata >> 4;
		}
		else
		{
		    *fat_data = (*fat_data & 0xf0) | ((clsdata >> 8) & 0x0f);
		}
		
		rfat_fat_cache_modify(volume, entry);
	    }
	}
    }
    else
#endif /* (RFAT_CONFIG_FAT12_SUPPORTED == 1) */
    {
	offset = clsno << volume->type;
	blkno = volume->fat1_blkno + (offset >> RFAT_BLK_SHIFT);

	status = rfat_fat_cache_read(volume, blkno, &entry);
	    
	if (status == F_NO_ERROR)
	{
	    if (volume->type == RFAT_VOLUME_TYPE_FAT16)
	    {
		uint16_t *fat_data;

		fat_data = (uint16_t*)((void*)(entry->data + (offset & RFAT_BLK_MASK)));

		*fat_data = RFAT_HTOFS(clsdata);
	    }
	    else
	    {
		uint32_t *fat_data;

		fat_data = (uint32_t*)((void*)(entry->data + (offset & RFAT_BLK_MASK)));

		*fat_data = (*fat_data & 0xf0000000) | (RFAT_HTOFL(clsdata) & 0x0fffffff);
	    }

	    rfat_fat_cache_modify(volume, entry);
	}
    }

#if (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0)
    if (status == F_NO_ERROR)
    {
	unsigned int index;

	index = clsno % RFAT_CONFIG_CLUSTER_CACHE_ENTRIES;
	
	if (allocate || (volume->cluster_cache[index].clsno == clsno))
	{
	    volume->cluster_cache[index].clsno = clsno;
	    volume->cluster_cache[index].clsdata = clsdata;
	}
    }

#endif /* (RFAT_CONFIG_CLUSTER_CACHE_ENTRIES != 0) */

    return status;
}


static int rfat_cluster_chain_seek(rfat_volume_t *volume, uint32_t clsno, uint32_t clscnt, uint32_t *p_clsno)
{
    int status = F_NO_ERROR;
    uint32_t clsdata;

    do
    {
	status = rfat_cluster_read(volume, clsno, &clsdata);
	
	if (status == F_NO_ERROR)
	{
	    if ((clsdata >= 2) && (clsdata <= volume->last_clsno))
	    {
		clsno = clsdata;
		clscnt--;
	    }
	    else
	    {
		status = F_ERR_EOF;
	    }
	}
    }
    while ((status == F_NO_ERROR) && (clscnt != 0));
    
    if (status == F_NO_ERROR)
    {
	*p_clsno = clsno;
    }

    return status;
}


/* In order to guarantee file system fault tolerance the chain allocation is done iteratively.
 * free entry is found, it's marked as END_OF_CHAIN, and then the previous entry in the chain
 * is linked to it. This ensures that there is not broken chain anywhere. The is only the
 * possibility that there is a lost END_OF_CHAIN somewhere. Given that the fat cache writes
 * in sequence, the link process happens AFTER the END_OF_CHAIN has been written. 
 * 
 * If the allocation fails, then the chain is first split at the original create point,
 * and then freed. Hence again no inconsistent state is possible.
 */

static int rfat_cluster_chain_create(rfat_volume_t *volume, uint32_t clsno, uint32_t clscnt, uint32_t *p_clsno_a, uint32_t *p_clsno_l)
{
    int status = F_NO_ERROR;
    uint32_t clsno_a, clsno_f, clsno_n, clsno_l, clscnt_a, clsdata;

    clsno_f = volume->next_clsno;
    clsno_a = RFAT_CLSNO_NONE;
    clsno_l = RFAT_CLSNO_NONE;
    clsno_n = clsno_f;

    clscnt_a = 0;

    do
    {
	/* Bypass cluster cache on read while searching.
	 */
	status = rfat_cluster_read_uncached(volume, clsno_n, &clsdata);
	
	if (status == F_NO_ERROR)
	{
	    if (clsdata == RFAT_CLSNO_FREE)
	    {
		status = rfat_cluster_write(volume, clsno_n, RFAT_CLSNO_END_OF_CHAIN, TRUE);
		
		if (status == F_NO_ERROR)
		{
		    if (clsno_a == RFAT_CLSNO_NONE)
		    {
			clsno_a = clsno_n;
		    }
		    
		    if (clsno_l != RFAT_CLSNO_NONE)
		    {
			status = rfat_cluster_write(volume, clsno_l, clsno_n, TRUE);
		    }
		    
		    clsno_l = clsno_n;
		    clscnt_a++;
		}
	    }
	    
	    clsno_n++;

	    if (clsno_n > volume->last_clsno)
	    {
		clsno_n = 2; 
	    }

	    if (clsno_n == clsno_f)
	    {
		status = F_ERR_NOMOREENTRY;
	    }
	}
    }
    while ((status == F_NO_ERROR) && (clscnt != clscnt_a));

    if (status == F_NO_ERROR)
    {
	if (clsno != RFAT_CLSNO_NONE)
	{
	    status = rfat_cluster_write(volume, clsno, clsno_a, TRUE);
	}
	
	if (status == F_NO_ERROR)
	{
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	    volume->flags |= RFAT_VOLUME_FLAG_FSINFO_DIRTY;
	    
	    volume->free_clscnt -= clscnt_a;
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
	    
	    volume->next_clsno = clsno_n;

	    *p_clsno_a = clsno_a;
	    
	    if (p_clsno_l)
	    {
		*p_clsno_l = clsno_l;
	    }
	}
    }
    else
    {
	if (status == F_ERR_NOMOREENTRY)
	{
	    if (clsno_a != RFAT_CLSNO_NONE)
	    {
		status = rfat_cluster_chain_destroy(volume, clsno_a, RFAT_CLSNO_FREE);
	    }
	    
	    if (status == F_NO_ERROR)
	    {
		status = F_ERR_NOMOREENTRY;
	    }
	}
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	else
	{
	    volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_VALID;
	}
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
    }

    return status;
}

#if (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1)

static int rfat_cluster_chain_create_sequential(rfat_volume_t *volume, uint32_t clsno, uint32_t clscnt, uint32_t *p_clsno_a, uint32_t *p_clsno_l)
{
    int status = F_NO_ERROR;
    uint32_t clsno_a, clsno_b, clsno_t, clsno_n, clsno_l, clsno_s, clscnt_a, clsdata;

    clsno_b = volume->base_clsno;
    clsno_t = volume->limit_clsno;
    clsno_n = volume->free_clsno;

    clsno_a = RFAT_CLSNO_NONE;
    clsno_l = RFAT_CLSNO_NONE;

    clscnt_a = 0;

    do
    {
	if (clsno_n == clsno_t)
	{
	    do
	    {
		if (clsno_b == volume->start_clsno)
		{
		    clsno_t = volume->end_clsno;
		    clsno_b = clsno_t - (volume->blk_unit_size >> volume->cls_blk_shift);
		}
		else
		{
		    clsno_t = clsno_b;
		    clsno_b = clsno_t - (volume->blk_unit_size >> volume->cls_blk_shift);
		}

		if (clsno_b == volume->base_clsno)
		{
		    status = F_ERR_NOMOREENTRY;
		}
		else
		{
		    clsno_n = clsno_t;

		    do 
		    {
			clsno_s = clsno_n -1;

			status = rfat_cluster_read_uncached(volume, clsno_s, &clsdata);

			if (status == F_NO_ERROR)
			{
			    if (clsdata == RFAT_CLSNO_FREE)
			    {
				clsno_n = clsno_s;
			    }
			}
		    }
		    while ((status == F_NO_ERROR) && (clsdata == RFAT_CLSNO_FREE) && (clsno_n != clsno_b));
		}
	    }
	    while ((status == F_NO_ERROR) && (clsno_n == clsno_t));
	}

	if (status == F_NO_ERROR)
	{
	    status = rfat_cluster_write(volume, clsno_n, RFAT_CLSNO_END_OF_CHAIN, TRUE);
		
	    if (status == F_NO_ERROR)
	    {
		if (clsno_a == RFAT_CLSNO_NONE)
		{
		    clsno_a = clsno_n;
		}
		    
		if (clsno_l != RFAT_CLSNO_NONE)
		{
		    status = rfat_cluster_write(volume, clsno_l, clsno_n, TRUE);
		}
		
		clsno_l = clsno_n;
		clscnt_a++;

		clsno_n++;
	    }
	}
    }
    while ((status == F_NO_ERROR) && (clscnt != clscnt_a));

    if (status == F_NO_ERROR)
    {
	if (clsno != RFAT_CLSNO_NONE)
	{
	    status = rfat_cluster_write(volume, clsno, clsno_a, TRUE);
	}
	
	if (status == F_NO_ERROR)
	{
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	    volume->flags |= RFAT_VOLUME_FLAG_FSINFO_DIRTY;
	    
	    volume->free_clscnt -= clscnt_a;
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
	    
	    volume->base_clsno = clsno_b;
	    volume->limit_clsno = clsno_t;
	    volume->free_clsno = clsno_n;

	    *p_clsno_a = clsno_a;
	    
	    if (p_clsno_l)
	    {
		*p_clsno_l = clsno_l;
	    }
	}
    }
    else
    {
	if (status == F_ERR_NOMOREENTRY)
	{
	    if (clsno_a != RFAT_CLSNO_NONE)
	    {
		status = rfat_cluster_chain_destroy(volume, clsno_a, RFAT_CLSNO_FREE);
	    }

	    if ((status == F_NO_ERROR) || (status == F_ERR_NOMOREENTRY))
	    {
		status = rfat_cluster_chain_create(volume, clsno, clscnt, p_clsno_a, p_clsno_l);
	    }
	}
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	else
	{
	    volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_VALID;
	}
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
    }

    return status;
}

#endif /* (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1) */


#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)

/* A sequential cluster chain is created by first finding a free sequentual, and then hooking it up back to front. 
 * Hence there is no inconsistent file system state.
 */

static int rfat_cluster_chain_create_contiguous(rfat_volume_t *volume, uint32_t clscnt, uint32_t *p_clsno_a)
{
    int status = F_NO_ERROR;
    uint32_t clsno_a, clsno_n, clscnt_a, clsdata;

    clsno_a = volume->end_clsno;
    clscnt_a = 0;

    do
    {
	clsno_a--;

	/* Bypass cluster cache on read while searching.
	 */
	status = rfat_cluster_read_uncached(volume, clsno_a, &clsdata);
	
	if (status == F_NO_ERROR)
	{
	    if (clsdata == RFAT_CLSNO_FREE)
	    {
		clscnt_a++;
	    }
	    else
	    {
		clsno_a = (((((clsno_a << volume->cls_blk_shift) + volume->cls_blk_offset) / volume->blk_unit_size) * volume->blk_unit_size) - volume->cls_blk_offset) >> volume->cls_blk_shift;
	    }
	}
    }
    while ((status == F_NO_ERROR) && (clscnt != clscnt_a) && (clsno_a != volume->start_clsno));

    if (status == F_NO_ERROR)
    {
	if (clscnt == clscnt_a)
	{
	    clsno_n = clsno_a + clscnt_a;
	    clsdata = RFAT_CLSNO_END_OF_CHAIN;

	    do
	    {
		clsno_n--;

		status = rfat_cluster_write(volume, clsno_n, clsdata, FALSE);

		clsdata = clsno_n;
	    }
	    while ((status == F_NO_ERROR) && (clsno_n != clsno_a));

#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	    if (status == F_NO_ERROR)
	    {
		volume->flags |= RFAT_VOLUME_FLAG_FSINFO_DIRTY;

		volume->free_clscnt -= clscnt_a;
	    }
	    else
	    {
		volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_VALID;
	    }
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
	}
	else
	{
	    status = F_ERR_NOMOREENTRY;
	}
    }

    *p_clsno_a = clsno_a;

    return status;
}

#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */


/* The chain destroy process walks along the chain and frees each entry front to back.
 * Hence it's possible to have a lost chain, but no file system corruption.
 */

static int rfat_cluster_chain_destroy(rfat_volume_t *volume, uint32_t clsno, uint32_t clsdata)
{
    int status = F_NO_ERROR;
    uint32_t clsno_n;

    do
    {
	/* Bypass cluster cache on read while destroying.
	 */
	status = rfat_cluster_read_uncached(volume, clsno, &clsno_n);

	if (status == F_NO_ERROR)
	{
	    status = rfat_cluster_write(volume, clsno, clsdata, FALSE);

	    if (status == F_NO_ERROR)
	    {
		if (clsdata == RFAT_CLSNO_FREE)
		{
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
		    volume->free_clscnt++;
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
		}

		if ((clsno_n >= 2) && (clsno_n <= volume->last_clsno))
		{
		    clsno = clsno_n;
		    clsdata = RFAT_CLSNO_FREE;
		}
		else
		{
		    if (clsno_n < RFAT_CLSNO_LAST)
		    {
			status = F_ERR_EOF;
		    }
		}
	    }
	}
    }
    while ((status == F_NO_ERROR) && (clsno_n < RFAT_CLSNO_LAST));

#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
    if (status == F_NO_ERROR)
    {
	volume->flags |= RFAT_VOLUME_FLAG_FSINFO_DIRTY;
    }
    else
    {
	volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_VALID;
    }
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */

    return status;
}

/***********************************************************************************************************************/

static inline unsigned int rfat_name_ascii_upcase(unsigned int cc)
{
    unsigned int uc;

    if ((cc <= 0x1f) || (cc > 0x7f))
    {
	uc = 0x0000;
    }
    else
    {
	if ((cc >= 'a') && (cc <= 'z'))
	{
	    uc = cc - ('a' - 'A');
	}
	else
	{
	    uc = cc;
	}
    }

    return uc;
}

#if (RFAT_CONFIG_VFAT_SUPPORTED == 0)

static const char *rfat_name_cstring_to_dosname(const char *cstring, uint8_t *dosname)
{
    unsigned int cc, n, n_e;

    memset(dosname, ' ', 11);

    n = 0;
    n_e = 8;

    cc = *cstring++;

    if (cc == ' ')
    {
	cstring = NULL;
    }
    else
    {
	/* A leading sequence of "." or ".." is handled varbatim.
	 */
    
	if (cc == '.')
	{
	    dosname[n++] = cc;

	    cc = *cstring++;

	    if (cc == '.')
	    {
		dosname[n++] = cc;

		cc = *cstring++;
	    }

	    if ((cc != '/') && (cc != '\\') && (cc != '\0'))
	    {
		cstring = NULL;
	    }
	}

	while ((cstring != NULL) && (cc != '/') && (cc != '\\') && (cc != '\0'))
	{
	    if (cc == '.')
	    {
		if (n_e == 11)
		{
		    cstring = NULL;
		}
		else
		{
		    n = 8;
		    n_e = 11;
			
		    cc = *cstring++;
		}
	    }
	    else
	    {
		if (n == n_e)
		{
		    cstring = NULL;
		}
		else
		{
		    if ((cc <= 0x001f) ||
			(cc >= 0x0080) ||
			(cc == '"') ||
			(cc == '*') ||
			(cc == '+') ||
			(cc == ',') ||
			(cc == '/') ||
			(cc == ':') ||
			(cc == ';') ||
			(cc == '<') ||
			(cc == '=') ||
			(cc == '>') ||
			(cc == '?') ||
			(cc == '[') ||
			(cc == '\\') ||
			(cc == ']') ||
			(cc == '|'))
		    {
			cstring = NULL;
		    }
		    else
		    {
			dosname[n++] = rfat_name_ascii_upcase(cc);
			    
			cc = *cstring++;
		    }
		}
	    }
	}

	if (n == 0)
	{
	    cstring = NULL;
	}
    }

    return cstring;
}

static const char *rfat_name_cstring_to_pattern(const char *cstring, char *pattern)
{
    unsigned int cc, n, n_e;

    memset(pattern, ' ', 11);

    n = 0;
    n_e = 8;

    cc = *cstring++;

    if (cc == ' ')
    {
	cstring = NULL;
    }
    else
    {
	/* A leading sequence of "." or ".." is handled varbatim.
	 */
    
	if (cc == '.')
	{
	    pattern[n++] = cc;

	    cc = *cstring++;

	    if (cc == '.')
	    {
		pattern[n++] = cc;

		cc = *cstring++;
	    }
	}

	while ((cstring != NULL) && (cc != '\0'))
	{
	    if (cc == '.')
	    {
		if (n_e == 11)
		{
		    cstring = NULL;
		}
		else
		{
		    n = 8;
		    n_e = 11;
		    
		    cc = *cstring++;
		}
	    }
	    else
	    {
		if (n == n_e)
		{
		    cstring = NULL;
		}
		else
		{
		    if (cc == '*')
		    {
			while (n != n_e)
			{
			    pattern[n++] = '?';
			}

			cc = *cstring++;
		    }
		    else
		    {
			if ((cc <= 0x001f) ||
			    (cc >= 0x0080) ||
			    (cc == '"') ||
			    (cc == '+') ||
			    (cc == ',') ||
			    (cc == '/') ||
			    (cc == ':') ||
			    (cc == ';') ||
			    (cc == '<') ||
			    (cc == '=') ||
			    (cc == '>') ||
			    (cc == '[') ||
			    (cc == '\\') ||
			    (cc == ']') ||
			    (cc == '|'))
			{
			    cstring = NULL;
			}
			else
			{
			    pattern[n++] = rfat_name_ascii_upcase(cc);
			    
			    cc = *cstring++;
			}
		    }
		}
	    }
	}
    }

    return cstring;
}

#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

static const char *rfat_name_cstring_to_label(const char *cstring, uint8_t *label)
{
    unsigned int cc, n, n_e;

    memset(label, ' ', 11);

    n = 0;
    n_e = 11;

    cc = *cstring++;

    if (cc == ' ')
    {
	cstring = NULL;
    }
    else
    {
	do
	{
	    if (n == n_e)
	    {
		cstring = NULL;
	    }
	    else
	    {
		if ((cc <= 0x001f) ||
		    (cc >= 0x0080) ||
		    (cc == '"') ||
		    (cc == '*') ||
		    (cc == '+') ||
		    (cc == ',') ||
		    (cc == '.') ||
		    (cc == '/') ||
		    (cc == ':') ||
		    (cc == ';') ||
		    (cc == '<') ||
		    (cc == '=') ||
		    (cc == '>') ||
		    (cc == '?') ||
		    (cc == '[') ||
		    (cc == '\\') ||
		    (cc == ']') ||
		    (cc == '|'))
		{
		    cstring = NULL;
		}
		else
		{
		    label[n++] = rfat_name_ascii_upcase(cc);
			    
		    cc = *cstring++;
		}
	    }
	}
	while ((cstring != NULL) && (cc != '\0'));
    }
    
    return cstring;
}

static char *rfat_name_dosname_to_cstring(const uint8_t *dosname, unsigned int doscase, char *cstring, char *cstring_e)
{
    unsigned int i, n;

    for (n = 7; n != 0; n--)
    {
        if (dosname[n] != ' ')
        {
            break;
        }
    }

    if (cstring + (n +1) <= cstring_e)
    {
	for (i = 0; i < (n +1); i++)
	{
	    if ((dosname[i] <= 0x1f) || (dosname[i] >= 0x80))
	    {
		*cstring++ = '_';
	    }
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	    else if ((doscase & RFAT_DIR_TYPE_LCASE_NAME) && (dosname[i] >= 'A') && (dosname[i] <= 'Z'))
	    {
		*cstring++ = (dosname[i] + ('a' - 'A'));
	    }
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    else
	    {
		*cstring++ = dosname[i];
	    }
	}

	for (n = 10; n != 7; n--)
	{
	    if (dosname[n] != ' ')
	    {
		break;
	    }
	}

	if (n != 7)
	{
	    if (cstring < cstring_e)
	    {
		*cstring++ = '.';
		
		if (cstring + ((n -8) +1) <= cstring_e)
		{
		    for (i = 8; i < (n +1); i++)
		    {
			if ((dosname[i] <= 0x1f) || (dosname[i] >= 0x80))
			{
			    *cstring++ = '_';
			}
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
			else if ((doscase & RFAT_DIR_TYPE_LCASE_EXT) && (dosname[i] >= 'A') && (dosname[i] <= 'Z'))
			{
			    *cstring++ = (dosname[i] + ('a' - 'A'));
			}
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
			else
			{
			    *cstring++ = dosname[i];
			}
		    }
		}
	    }
	}

	if (cstring != NULL)
	{
	    if (cstring < cstring_e)
	    {
		*cstring++ = '\0';
	    }
	    else
	    {
		cstring = NULL;
	    }
	}
    }
    else
    {
	cstring = NULL;
    }

    return cstring;
}

static char *rfat_name_label_to_cstring(const uint8_t *label, char *cstring, char *cstring_e)
{
    unsigned int i, n;

    for (n = 10; n != 0; n--)
    {
        if (label[n] != ' ')
        {
            break;
        }
    }

    if (cstring + (n +1) <= cstring_e)
    {
	for (i = 0; i < (n +1); i++)
	{
	    if ((label[i] <= 0x1f) || (label[i] >= 0x80))
	    {
		*cstring++ = '_';
	    }
	    else
	    {
		*cstring++ = label[i];
	    }
	}

	if (cstring < cstring_e)
	{
	    *cstring++ = '\0';
	}
	else
	{
	    cstring = NULL;
	}
    }
    else
    {
	cstring = NULL;
    }

    return cstring;
}

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)

#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)

static inline unsigned int rfat_name_cstring_to_unicode(const char *cstring, const char **p_cstring)
{
    unsigned int cc, c;
    const uint8_t *utf8;

    utf8 = (const uint8_t*)cstring;

    c = *utf8++;

    if (c <= 0x7f)                /* 0XXX XXXX one byte    */
    {
	cc = c;
    }
    else if ((c & 0xe0) == 0xc0)  /* 110X XXXX two bytes   */
    {
	cc = c & 31;

	c = *utf8++;

	if ((c & 0xc0) == 0x80)
	{
	    cc = (cc << 6) | (c & 31);
	}
	else
	{
	    cc = 0x001f; /* error */
	}
    }
    else if ((c & 0xf0) == 0xe0)  /* 1110 XXXX three bytes */
    {
	cc = c & 15;

	c = *utf8++;

	if ((c & 0xc0) == 0x80)
	{
	    cc = (cc << 6) | (c & 31);

	    c = *utf8++;

	    if ((c & 0xc0) == 0x80)
	    {
		cc = (cc << 6) | (c & 31);
	    }
	    else
	    {
		cc = 0x001f; /* error */
	    }
	}
	else
	{
	    cc = 0x001f; /* error */
	}
    }
    else
    {
	cc = 0x001f;
    }

    if (p_cstring)
    {
	*p_cstring = (const char*)utf8;
    }

    return cc;
}

static inline char * rfat_name_unicode_to_cstring(unsigned int cc, char *cstring, char *cstring_e)
{
    uint8_t *utf8;

    utf8 = (uint8_t*)cstring;

    if (cc <= 0x007f)      /* 0XXX XXXX one byte */
    {
	if ((utf8 + 1) >= (uint8_t*)cstring_e)
	{
	    utf8 = NULL;
	}
	else
	{
	    *utf8++ = cc;
	}
    }
    else if (cc <= 0x07ff)  /* 110X XXXX two bytes */
    {
	if ((utf8 + 2) >= (uint8_t*)cstring_e)
	{
	    utf8 = NULL;
	}
	else
	{
	    *utf8++ = 0xc0 | (cc >> 6);
	    *utf8++ = 0x80 | (cc & 31);
	}
    }
    else                   /* 1110 XXXX three bytes */
    {
	if ((utf8 + 3) >= (uint8_t*)cstring_e)
	{
	    utf8 = NULL;
	}
	else
	{
	    *utf8++ = 0xe0 | (cc >> 12);
	    *utf8++ = 0x80 | ((cc >> 6) & 31);
	    *utf8++ = 0x80 | (cc & 31);
	}
    }

    return (char*)utf8;
}

static const uint8_t rfat_name_unicode_upcase_index_table[2048] = {
    0, 0, 0, 1, 0, 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 
    3, 11, 12, 13, 14, 0, 0, 0, 0, 0, 0, 15, 0, 16, 17, 18, 
    0, 19, 20, 3, 21, 3, 22, 3, 23, 0, 0, 24, 25, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0, 
    3, 3, 3, 3, 27, 3, 3, 28, 29, 30, 31, 32, 30, 33, 34, 35, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 36, 37, 38, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 39, 40, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 41, 42, 43, 3, 3, 3, 44, 45, 46, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 
};

static const int16_t rfat_name_unicode_upcase_delta_table[1504] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, 
    -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, 0, 0, 0, 0, 0, 
    -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, 
    -32, -32, -32, -32, -32, -32, -32, 0, -32, -32, -32, -32, -32, -32, -32, 121, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, 0, 0, -1, 0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0, 
    -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0, 
    195, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, 
    0, 0, -1, 0, 0, 97, 0, 0, 0, -1, 163, 0, 0, 0, 130, 0, 
    0, -1, 0, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 
    -1, 0, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, 0, -1, 0, 56, 
    0, 0, 0, 0, 0, 0, -2, 0, 0, -2, 0, 0, -2, 0, -1, 0, 
    -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, -79, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, 0, 0, -2, 0, -1, 0, 0, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, 0, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 10795, 0, -1, 0, 10792, 0, 
    0, 0, -1, 0, 0, 0, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, 0, 0, -210, -206, 0, -205, -205, 0, -202, 0, -203, 0, 0, 0, 0, 
    -205, 0, 0, -207, 0, 0, 0, 0, -209, -211, 0, 10743, 0, 0, 0, -211, 
    0, 0, -213, 0, 0, -214, 0, 0, 0, 0, 0, 0, 0, 10727, 0, 0, 
    -218, 0, 0, -218, 0, 0, 0, 0, -218, -69, -217, -217, -71, 0, 0, 0, 
    0, 0, -219, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 130, 130, 130, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -38, -37, -37, -37, 
    0, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, 
    -32, -32, -31, -32, -32, -32, -32, -32, -32, -32, -32, -32, -64, -63, -63, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, 0, 7, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, 
    -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, -32, 
    -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, 
    0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, -15, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, 
    -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, 
    -48, -48, -48, -48, -48, -48, -48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3814, 0, 0, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 
    0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 
    8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 
    8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 
    8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 
    8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 8, 0, 8, 0, 8, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 
    8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 
    74, 74, 86, 86, 86, 86, 100, 100, 128, 128, 112, 112, 126, 126, 0, 0, 
    8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 
    8, 8, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -9, 0, 0, 0, 
    8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    8, 8, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -9, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -28, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, 
    0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, 
    -26, -26, -26, -26, -26, -26, -26, -26, -26, -26, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, 
    -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, 
    -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, -48, 0, 
    0, -1, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, -1, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, 
    -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, -7264, 
    -7264, -7264, -7264, -7264, -7264, -7264, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

static inline unsigned int rfat_name_unicode_upcase(unsigned int cc)
{
    return ((cc & 0xffff) + rfat_name_unicode_upcase_delta_table[(rfat_name_unicode_upcase_index_table[(cc & 0xffff) >> 5] << 5) + (cc & 31)]);
}

#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */

static const uint8_t rfat_name_nibble_to_char_table[16] = "0123456789ABCDEF";

static const char *rfat_name_cstring_to_uniname(const char *cstring, rfat_unicode_t *uniname, uint8_t *p_unicount, uint8_t *dosname, uint8_t *p_doscase)
{
    unsigned int cc, i, n, n_e, doscase;

    doscase = 0;

    memset(dosname, ' ', 11);

    n = 0;
    n_e = 8;
    i = 0;

#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
    cc = rfat_name_cstring_to_unicode(cstring, &cstring);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
    cc = *cstring++;
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */

    if (cc == ' ')
    {
	cstring = NULL;
    }
    else
    {
	/* A leading sequence of "." and ".." is handled varbatim.
	 */
    
	if (cc == '.')
	{
	    dosname[n++] = cc;
	    uniname[i++] = cc;

#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
	    cc = rfat_name_cstring_to_unicode(cstring, &cstring);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
	    cc = *cstring++;
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */

	    if (cc == '.')
	    {
		dosname[n++] = cc;
		uniname[i++] = cc;
	    
#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
		cc = rfat_name_cstring_to_unicode(cstring, &cstring);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
		cc = *cstring++;
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
	    }

	    if ((cstring != NULL) && (cc != '/') && (cc != '\\') && (cc != '\0'))
	    {
		dosname[0] = '\0';
	    }
	}

	while ((cstring != NULL) && (cc != '/') && (cc != '\\') && (cc != '\0'))
	{
	    if (dosname[0] != '\0')
	    {
		if (cc == '.')
		{
		    if (n_e == 11)
		    {
			dosname[0] = '\0';
		    }
		    else
		    {
			n = 8;
			n_e = 11;
		    }
		}
		else
		{
		    if (n == n_e)
		    {
			dosname[0] = '\0';
		    }
		    else
		    {
			if ((cc <= 0x001f) ||
			    (cc >= 0x0080) || 
			    (cc == '"') ||
			    (cc == '*') ||
			    (cc == '+') ||
			    (cc == ',') ||
			    (cc == '.') ||
			    (cc == '/') ||
			    (cc == ':') ||
			    (cc == ';') ||
			    (cc == '<') ||
			    (cc == '=') ||
			    (cc == '>') ||
			    (cc == '?') ||
			    (cc == '[') ||
			    (cc == '\\') ||
			    (cc == ']') ||
			    (cc == '|'))
			{
			    dosname[0] = '\0';
			}
			else
			{
			    dosname[n++] = rfat_name_ascii_upcase(cc);

			    if ((cc >= 'a') && (cc <= 'z'))
			    {
				doscase |= ((n_e == 8) ? RFAT_DIR_TYPE_LCASE_NAME : RFAT_DIR_TYPE_LCASE_EXT);
			    }

			    if ((cc >= 'A') && (cc <= 'Z'))
			    {
				doscase |= ((n_e == 8) ? RFAT_DIR_TYPE_UCASE_NAME : RFAT_DIR_TYPE_UCASE_EXT);
			    }
			}
		    }
		}
	    }

	    if (i == 255)
	    {
		cstring = NULL;
	    }
	    else
	    {
		if ((cc <= 0x001f) ||
#if (RFAT_CONFIG_UTF8_SUPPORTED == 0)
		    (cc >= 0x0080) || 
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 0) */
		    (cc == '"') ||
		    (cc == '*') ||
		    (cc == '/') ||
		    (cc == ':') ||
		    (cc == '<') ||
		    (cc == '>') ||
		    (cc == '?') ||
		    (cc == '\\') ||
		    (cc == '|'))
		{
		    cstring = NULL;
		}
		else
		{
		    uniname[i++] = cc;

#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
		    cc = rfat_name_cstring_to_unicode(cstring, &cstring);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
		    cc = *cstring++;
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
		}
	    }
	}

	if (cstring != NULL)
	{
	    /* Strip trailing ' ' and '.'. This will force "." and ".."
	     * to be a dosname only.
	     */
	    while (i != 0)
	    {
		if ((uniname[i -1] != ' ') && (uniname[i -1] != '.'))
		{
		    break;
		}

		i--;
	    }

	    if (dosname[0] != '\0')
	    {
		if (((doscase & (RFAT_DIR_TYPE_LCASE_NAME | RFAT_DIR_TYPE_UCASE_NAME)) == (RFAT_DIR_TYPE_LCASE_NAME | RFAT_DIR_TYPE_UCASE_NAME)) ||
		    ((doscase & (RFAT_DIR_TYPE_LCASE_EXT  | RFAT_DIR_TYPE_UCASE_EXT )) == (RFAT_DIR_TYPE_LCASE_EXT  | RFAT_DIR_TYPE_UCASE_EXT)))
		{
		    /* Have seen both lower case and upper case characters, hence a lossy dosname mapping.
		     */

		    doscase = RFAT_DIR_TYPE_LOSSY;
		}
		else
		{
		    /* A fully legel dosname, hence no uniname mapping.
		     */
		    doscase &= (RFAT_DIR_TYPE_LCASE_NAME | RFAT_DIR_TYPE_LCASE_EXT);
		}
	    }
	    else
	    {
		/* If there is no valid dosname, there has to be a valid uniname.
		 */
		doscase = 0x00;

		if (i == 0)
		{
		    cstring = NULL;
		}
	    }
	}
    }

    *p_unicount = i;
    *p_doscase = doscase;

    return cstring;
}

static char *rfat_name_uniname_to_cstring(const rfat_unicode_t *uniname, unsigned int unicount, char *cstring, char *cstring_e)
{
    unsigned int cc, n;

    for (n = 0; n < unicount; n++)
    {
	cc = *uniname++;

#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
	cstring = rfat_name_unicode_to_cstring(cc, cstring, cstring_e);

	if (cstring == NULL)
	{
	    break;
	}
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */

	if (cstring < cstring_e)
	{
	    if ((cc <= 0x1f) || (cc >= 0x80))
	    {
		*cstring++ = '_';
	    }
	    else
	    {
		*cstring++ = cc;
	    }
	}
	else
	{
	    cstring = NULL;

	    break;
	}
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
    }

    if (cstring != NULL)
    {
	if (cstring < cstring_e)
	{
	    *cstring++ = '\0';
	}
	else
	{
	    cstring = NULL;
	}
    }

    return cstring;
}

static void rfat_name_uniname_to_dosname(const rfat_unicode_t *uniname, unsigned int unicount, uint8_t *dosname, unsigned int *p_dosprefix)
{
    unsigned int offset, prefix, i, n, n_e, cc;

    memset(dosname, ' ', 11);

    /* Compute the offset of the last '.' in uniname.
     */
    
    for (offset = unicount, i = 0; i < unicount; i++)
    {
	if (uniname[i] == '.')
	{
	    offset = i;
	}
    }

    prefix = 0;
    n = 0;
    n_e = 8;
    i = 0;

    do
    {
	cc = uniname[i];

	if (cc == '.')
	{
	    if (i <= offset)
	    {
		if ((n_e == 11) || (i != offset))
		{
		    if (n != n_e)
		    {
			dosname[n++] = '_';
		    }
		}
		else
		{
		    prefix = n;
		    n = 8;
		    n_e = 11;
		}
	    }
	    else
	    {
		/* ignore */
	    }
	}
	else
	{
	    if (cc == ' ')
	    {
		/* ignore */
	    }
	    else
	    {
		if (n != n_e)
		{
		    if ((cc <= 0x001f) ||
			(cc >= 0x0080) ||
			(cc == '+') ||
			(cc == ',') ||
			(cc == ';') ||
			(cc == '=') ||
			(cc == '[') ||
			(cc == ']'))
		    {
			dosname[n++] = '_';
		    }
		    else
		    {
			dosname[n++] = rfat_name_ascii_upcase(cc);
		    }
		}
		else
		{
		    /* ignore */
		}
	    }
	}

	i++;
    }
    while ((n != 11) && (i < unicount));

    if (n_e == 8)
    {
	prefix = n;
    }

    *p_dosprefix = prefix;
}

static uint8_t rfat_name_checksum_dosname(const uint8_t *dosname)
{
    unsigned int n, chksum;

    for (chksum = 0, n = 0; n < 11; n++)
    {
	chksum = ((chksum >> 1) | (chksum << 7)) + *dosname++;
    }

    return chksum & 0xff;
}

static uint16_t rfat_name_checksum_uniname(const rfat_unicode_t *uniname, unsigned int unicount)
{
    unsigned int n, chksum;

    for (chksum = 0, n = 0; n < unicount; n++)
    {
	chksum = ((chksum >> 1) | (chksum << 15)) + *uniname++;
    }

    return chksum & 0xffff;
}

#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

/***********************************************************************************************************************/

static int rfat_path_convert_filename(rfat_volume_t *volume, const char *filename, const char **p_filename)
{
    int status = F_NO_ERROR;

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1) 

    filename = rfat_name_cstring_to_uniname(filename, volume->lfn_name, &volume->lfn_count, volume->dir.dir_name, &volume->dir.dir_nt_reserved);

    volume->dir_entries = volume->lfn_count ? ((volume->lfn_count +12) / 13) : 0;

#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

    filename = rfat_name_cstring_to_dosname(filename, volume->dir.dir_name);

    volume->dir.dir_nt_reserved = 0;

#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

    if (filename != NULL)
    {
	if (p_filename)
	{
	    *p_filename = filename;
	}
    }
    else
    {
	status = F_ERR_INVALIDNAME;
    }

    return status;
}

#if (RFAT_CONFIG_VFAT_SUPPORTED == 0)

static int rfat_path_find_callback_empty(rfat_volume_t *volume, void *private, rfat_dir_t *dir)
{
    return TRUE;
}

static int rfat_path_find_callback_volume(rfat_volume_t *volume, void *private, rfat_dir_t *dir)
{
    return (dir->dir_attr & RFAT_DIR_ATTR_VOLUME_ID);
}

static int rfat_path_find_callback_directory(rfat_volume_t *volume, void *private, rfat_dir_t *dir)
{
    if (!(dir->dir_attr & RFAT_DIR_ATTR_DIRECTORY))
    {
	return FALSE;
    }
    else
    {
	uint32_t clsno, clsno_d;

	clsno = (uint32_t)private;

	if (volume->type == RFAT_VOLUME_TYPE_FAT32)
	{
	    clsno_d = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
	}
	else
	{
	    clsno_d = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
	}

	return (clsno == clsno_d);
    }
}

static int rfat_path_find_callback_name(rfat_volume_t *volume, void *private, rfat_dir_t *dir)
{
    return (!(dir->dir_attr & RFAT_DIR_ATTR_VOLUME_ID) && !memcmp(dir->dir_name, volume->dir.dir_name, sizeof(dir->dir_name)));
}

static int rfat_path_find_callback_pattern(rfat_volume_t *volume, void *private, rfat_dir_t *dir)
{
    const uint8_t *pattern = (const uint8_t*)private;
    int match;
    unsigned int n;

    if (!(dir->dir_attr & RFAT_DIR_ATTR_VOLUME_ID))
    {
	match = TRUE;

	for (n = 0; n < 11; n++)
	{
	    if (!((pattern[n] == '?') || (dir->dir_name[n] == pattern[n])))
	    {
		match = FALSE;

		break;
	    }
	}
    }
    else
    {
	match = FALSE;
    }

    return match;
}

#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 0) */

static void rfat_path_convert_ldir_entry(rfat_volume_t *volume, rfat_dir_t *dir, unsigned int sequence)
{
    unsigned int s, i, offset, cc;

    if (sequence & RFAT_LDIR_SEQUENCE_FIRST)
    {
	volume->dir.dir_name[0] = '\0';
	volume->dir_entries = (sequence & RFAT_LDIR_SEQUENCE_INDEX);
	volume->lfn_count = (sequence & RFAT_LDIR_SEQUENCE_INDEX) * 13;
    }

    s = 0;
    i = ((sequence & RFAT_LDIR_SEQUENCE_INDEX) -1) * 13;

    do
    {
	offset = rfat_path_ldir_name_table[s++];
	
	cc = (((uint8_t*)dir)[offset +1] << 8) | ((uint8_t*)dir)[offset +0];
	
	if (cc == 0x0000)
	{
	    volume->lfn_count = i;
	}
	else if (i < 255)
	{
#if (RFAT_CONFIG_UTF8_SUPPORTED == 0)
	    /* If UTF8 is not supported, then the name space is ASCII. In
	     * that case remap illegal values to 0x001f.
	     */
	    if (cc >= 0x0080)
	    {
		volume->lfn_name[i++] = 0x001f;
	    }
	    else
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 0) */
	    {
		volume->lfn_name[i++] = cc;
	    }
	}
    }
    while ((s < 13) && (cc != 0x0000));
}


static int rfat_path_find_callback_empty(rfat_volume_t *volume, void *private, rfat_dir_t *dir, unsigned int sequence)
{
    return TRUE;
}

static int rfat_path_find_callback_volume(rfat_volume_t *volume, void *private, rfat_dir_t *dir, unsigned int sequence)
{
    int match;

    if (sequence & RFAT_LDIR_SEQUENCE_INDEX)
    {
	match = FALSE;
    }
    else
    {
	if (dir->dir_attr & RFAT_DIR_ATTR_VOLUME_ID)
	{
	    match = (sequence != RFAT_LDIR_SEQUENCE_LAST);
	}
	else
	{
	    match = FALSE;
	}
    }

    return match;
}

static int rfat_path_find_callback_directory(rfat_volume_t *volume, void *private, rfat_dir_t *dir, unsigned int sequence)
{
    uint32_t clsno, clsno_d;
    int match;

    if (sequence & RFAT_LDIR_SEQUENCE_INDEX)
    {
	rfat_path_convert_ldir_entry(volume, dir, sequence);

	match = TRUE;
    }
    else
    {
	if (dir->dir_attr & RFAT_DIR_ATTR_DIRECTORY)
	{
	    clsno = (uint32_t)private;

	    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
	    {
		clsno_d = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
	    }
	    else
	    {
		clsno_d = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
	    }

	    if (clsno_d == clsno)
	    {
		if (sequence != RFAT_LDIR_SEQUENCE_LAST)
		{
		    volume->dir_entries = 0;
		    volume->lfn_count = 0;
		}

		match = TRUE;
	    }
	    else
	    {
		match = FALSE;
	    }
	}
	else
	{
	    match = FALSE;
	}
    }

    return match;
}

static int rfat_path_find_callback_name(rfat_volume_t *volume, void *private, rfat_dir_t *dir, unsigned int sequence)
{
    unsigned int offset, i, s, cc, lc;
    int match;

    if (sequence & RFAT_LDIR_SEQUENCE_INDEX)
    {
	match = TRUE;

	if ((sequence & RFAT_LDIR_SEQUENCE_FIRST) && ((sequence & RFAT_LDIR_SEQUENCE_INDEX) != volume->dir_entries))
	{
	    match = FALSE;
	}
	else
	{
	    s = 0;
	    i = ((sequence & RFAT_LDIR_SEQUENCE_INDEX) -1) * 13;
		
	    do
	    {
		offset = rfat_path_ldir_name_table[s++];
		    
		cc = (((uint8_t*)dir)[offset +1] << 8) | ((uint8_t*)dir)[offset +0];
		    
		if (i < volume->lfn_count)
		{
		    lc = volume->lfn_name[i];

#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
		    cc = rfat_name_unicode_upcase(cc);
		    lc = rfat_name_unicode_upcase(lc);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
		    /* That's a tad non-intuitive here. "volume->lfn_name[]" does contain
		     * only ASCII characters. Hence only the ASCII portion of the UNICODE
		     * name space has to be upcased. Anything outside needs properly mismatch.
		     */
		    if (cc < 0x80)
		    {
			cc = rfat_name_ascii_upcase(cc);
		    }
		    lc = rfat_name_ascii_upcase(lc);
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
		  
		    if (lc != cc)
		    {
			match = FALSE;
		    }
		}
		else 
		{
		    if (i == volume->lfn_count)
		    {
			if (cc != 0x0000)
			{
			    match = FALSE;
			}
		    }
		    else
		    {
			if (cc != 0xffff)
			{
			    match = FALSE;
			}
		    }
		}

		i++;
	    }
	    while (match && (s < 13));
	}
    }
    else
    {
	if (!(dir->dir_attr & RFAT_DIR_ATTR_VOLUME_ID))
	{
	    if (sequence != RFAT_LDIR_SEQUENCE_LAST)
	    {
		if ((volume->dir.dir_name[0] != '\0') && !memcmp(dir->dir_name, volume->dir.dir_name, sizeof(dir->dir_name)))
		{
		    /* Set volume->dir_entries to 0 on a SFN match.
		     */
		    volume->dir_entries = 0;
		    
		    match = TRUE;
		}
		else
		{
		    match = FALSE;
		}
	    }
	    else
	    {
		/* A LFN name had been checked already while looking at the LDIR entries.
		 * If we get here it's a match, otherwise the code would not be called.
		 */
		match = TRUE;
	    }
	}
	else
	{
	    match = FALSE;
	}
    }

    return match;
}

static int rfat_path_find_callback_unique(rfat_volume_t *volume, void *private, rfat_dir_t *dir, unsigned int sequence)
{
    unsigned int cc, index;
    unsigned int *p_mask;
    int match;
    
    if (sequence & RFAT_LDIR_SEQUENCE_INDEX)
    {
	match = TRUE;
    }
    else
    {
	if (!(dir->dir_attr & RFAT_DIR_ATTR_VOLUME_ID))
	{
	    p_mask = (unsigned int*)private;

	    if (p_mask != NULL)
	    {
		/* The pivot index is stored in the upper 4 bits, while the
		 * pivot result is store in the lower 9 bit. After the 
		 * unique search, the lower 9 bit contain which similar
		 * names with '1' to '9' at the privot index were encountered.
		 */

		index = (*p_mask) >> 9;

		cc = dir->dir_name[index];

		if ((cc >= '1') && (cc <= '9'))
		{
		    volume->dir.dir_name[index] = cc;
		    
		    if (!memcmp(dir->dir_name, volume->dir.dir_name, sizeof(dir->dir_name)))
		    {
			*p_mask |= (1 << (cc - '1'));
		    }
		}

		match = FALSE;
	    }
	    else
	    {
		match = !memcmp(dir->dir_name, volume->dir.dir_name, sizeof(dir->dir_name));
	    }
	}

	match = FALSE;
    }

    return match;
}

static int rfat_path_find_callback_pattern(rfat_volume_t *volume, void *private, rfat_dir_t *dir, unsigned int sequence)
{
    const char *pattern = (const char*)private;
    unsigned int offset, i, n, n_e, cc, lc;
    int match, skip;

    if (sequence & RFAT_LDIR_SEQUENCE_INDEX)
    {
	rfat_path_convert_ldir_entry(volume, dir, sequence);

	match = TRUE;
    }
    else
    {
	if (!(dir->dir_attr & RFAT_DIR_ATTR_VOLUME_ID))
	{
	    if (sequence != RFAT_LDIR_SEQUENCE_LAST)
	    {
		/* A SFN is converted into a LFN before pattern matching.
		 * Otherwise, the matching would be inconsistent.
		 */

		i = 0;

		for (n_e = 7; n_e != 0; n_e--)
		{
		    if (dir->dir_name[n_e] != ' ')
		    {
			break;
		    }
		}
		
		for (n = 0; n <= n_e; n++)
		{
		    /* Matching is always done case insensitive. However when the name is converted
		     * back into a cstring, it needs to be case sensitive.
		     */
		    if ((dir->dir_nt_reserved & RFAT_DIR_TYPE_LCASE_NAME) && (dir->dir_name[n] >= 'A') && (dir->dir_name[n] <= 'Z'))
		    {
			volume->lfn_name[i++] = (dir->dir_name[n] + ('a' - 'A'));
		    }
		    else
		    {
			volume->lfn_name[i++] = dir->dir_name[n];
		    }
		}

		offset = i;

		for (n_e = 10; n_e != 7; n_e--)
		{
		    if (dir->dir_name[n_e] != ' ')
		    {
			break;
		    }
		}
		
		if (n_e != 7)
		{
		    volume->lfn_name[i++] = '.';

		    for (n = 8; n <= n_e; n++)
		    {
			/* Matching is always done case insensitive. However when the name is converted
			 * back into a cstring, it needs to be case sensitive.
			 */
			if ((dir->dir_nt_reserved & RFAT_DIR_TYPE_LCASE_EXT) && (dir->dir_name[n] >= 'A') && (dir->dir_name[n] <= 'Z'))
			{
			    volume->lfn_name[i++] = (dir->dir_name[n] + ('a' - 'A'));
			}
			else
			{
			    volume->lfn_name[i++] = dir->dir_name[n];
			}
		    }
		}

		volume->dir_entries = 0;
		volume->lfn_count = i;
	    }
	    else
	    {
		/* Compute the offset of the last '.' in the LFN.
		 */

		for (offset = volume->lfn_count, i = 0; i < volume->lfn_count; i++)
		{
		    if (volume->lfn_name[i] == '.')
		    {
			offset = i;
		    }
		}
	    }

	    /* The matcher below uses WINNT semantics:
	     *
	     * '.' (DOS_DOT)          matches either a '.' or zero characters beyond the name string
	     * '?' (DOS_QM)           matches a single character or, upon encountering a '.' or end of name string, advances the expression to the end of the set of contiguous DOS_QMs
	     * '*' (DOS_STAR)         matches zero or more characters until encountering and matching the final '.' in the name
	     *
	     * Derived sequences:
	     *
	     * '*' EOF                matches to the end of the name
	     * '*' '.'                matches either to the next '.' or zero characters beyond the name string
	     * '*' '?'                same as '?' (zero length '*' match)
	     * '*' '*'                same as '*' (zero length '*' match)
	     */

	    skip = FALSE;
	    match = TRUE;

	    i = 0;
	    cc = 0;

	    do
	    {
		if (!skip)
		{
#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
		    cc = rfat_name_cstring_to_unicode(pattern, &pattern);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
		    cc = *pattern++;
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
		}

		skip = FALSE;

		if (i < volume->lfn_count)
		{
		    if (cc == '\0')
		    {
			match = FALSE;
		    }
		    else if (cc == '*')                         /* asterisk      */
		    {
			if (i <= offset)
			{
			    i = offset;
			}
			else
			{
			    i = volume->lfn_count;
			}
		    }
		    else if (cc == '?')                         /* question mask  */
		    {
			if (i == offset)
			{
			    do
			    {
#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
				cc = rfat_name_cstring_to_unicode(pattern, &pattern);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
				cc = *pattern++;
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
			    }
			    while (cc == '?');

			    skip = TRUE;
			}

			i++;
		    }
		    else                                        /* <character>   */
		    {
			lc = volume->lfn_name[i++];

#if (RFAT_CONFIG_UTF8_SUPPORTED == 1)
			cc = rfat_name_unicode_upcase(cc);
			lc = rfat_name_unicode_upcase(lc);
#else /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */
			/* That's a tad non-intuitive here. "volume->lfn_name[]" does contain
			 * only ASCII characters. Hence only the ASCII portion of the UNICODE
			 * name space has to be upcased. Anything outside needs properly mismatch.
			 */
			cc = rfat_name_ascii_upcase(cc);
			lc = rfat_name_ascii_upcase(lc);
#endif /* (RFAT_CONFIG_UTF8_SUPPORTED == 1) */

			if (lc != cc)
			{
			    match = FALSE;
			}
		    }
		}
		else
		{
		    if ((cc == '\0') ||                         /* EOF           */
			(cc == '.') ||                          /* dot           */
		        (cc == '?') ||                          /* question mark */
		        (cc == '*'))                            /* start         */
		    {
		    }
		    else                                        /* <character>   */
		    {
			match = FALSE;
		    }
		}
	    }
	    while (match && (cc != '\0'));

	    /* Set volume->dir_entries to 0 on a SFN match.
	     */
	    if (match && (sequence != RFAT_LDIR_SEQUENCE_LAST))
	    {
		volume->dir_entries = 0;
	    }
	}
	else
	{
	    match = FALSE;
	}
    }

    return match;
}

#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 0) */


/*
 * p_dir != NULL:
 *
 *   p_clsno    clsno of first primary/secondary dir entry
 *   p_index    index within dirctory of first primary/secondary dir entry
 *   p_dir      dir cache entry containg the primary dir entry

 * p_dir == NULL:
 *
 *   p_clsno    clsno of first free dir entry
 *   p_index    index within directory of first free dir entry,
 *              or (index | 0x00020000) if a new cluster is needed,
 *              or 0x00010000 if there is no free entry anymore.
 *
 * The number of secondary entries is contained within name->lfn_entries. 
 */

static int rfat_path_find_entry(rfat_volume_t *volume, uint32_t clsno, uint32_t index, uint32_t count, rfat_find_callback_t callback, void *private, uint32_t *p_clsno, uint32_t *p_index, rfat_dir_t **p_dir)
{
    int status = F_NO_ERROR;
    int done;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    unsigned int sequence, chksum, ordinal;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    uint32_t clsno_f, clsno_m, clsdata, blkno, blkno_e, index_f, index_m, count_f;
    rfat_cache_entry_t *entry;
    rfat_dir_t *dir, *dir_e;

    done = FALSE;
    dir = NULL;

    if (clsno == RFAT_CLSNO_NONE)
    {
	clsno = volume->root_clsno;
	blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index);
	blkno_e = volume->root_blkno + volume->root_blkcnt;
    }
    else
    {
	/* rfat_path_find_entry reports back clsno/index for the current set of
	 * directory entries. Hence rfat_path_find_next has to set to the next
	 * entry by adding the composite size to "index", which of course
	 * can cross then a cluster boundary.
	 */

	if (index && !((index << RFAT_DIR_SHIFT) & volume->cls_mask))
	{
	    blkno = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
	    blkno_e = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
	}
	else
	{
	    blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_INDEX_TO_BLKCNT(index);
	    blkno_e = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
	}
    }

    clsno_f = RFAT_CLSNO_NONE;
    index_f = 0;
    count_f = 0;

    clsno_m = RFAT_CLSNO_NONE;
    index_m = 0;

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    sequence = 0;
    chksum = 0;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    
    while ((status == F_NO_ERROR) && !done && (index < 0x10000))
    {
	if (blkno == blkno_e)
	{
	    if (clsno == RFAT_CLSNO_NONE)
	    {
		done = TRUE;
		dir = NULL;
	    }
	    else
	    {
		status = rfat_cluster_read(volume, clsno, &clsdata);
		
		if (status == F_NO_ERROR)
		{
		    if (clsdata >= RFAT_CLSNO_LAST)
		    {
			done = TRUE;
			dir = NULL;
		    }
		    else
		    {
			if ((clsdata >= 2) && (clsdata <= volume->last_clsno))
			{
			    clsno = clsdata;
			    blkno = RFAT_CLSNO_TO_BLKNO(clsno);
			    blkno_e = blkno + volume->cls_blk_size;
			}
			else
			{
			    status = F_ERR_EOF;
			}
		    }
		}
	    }
	}

	if (status == F_NO_ERROR)
	{
	    if (!done)
	    {
		status = rfat_dir_cache_read(volume, blkno, &entry);
                
		if (status == F_NO_ERROR)
		{
		    dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index)));
		    dir_e = (rfat_dir_t*)((void*)(entry->data + RFAT_BLK_SIZE));
                
		    do
		    {
			if (dir->dir_name[0] == 0x00)
			{
			    /* A 0x00 in dir_name[0] says it's a free entry, and all subsequent entries
			     * are also free. So we can stop searching.
			     */

			    done = TRUE;
			    dir = NULL;
			}
			else if (dir->dir_name[0] == 0xe5)
			{
			    /* A 0xe5 in dir_name[0] says it's a free entry.
			     */

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
			    if (count_f != count)
			    {
				if ((count_f == 0) || !((clsno_f == clsno) && ((index_f + count_f) == index)))
				{
				    clsno_f = clsno;
				    index_f = index;
				    count_f = 1;
				}
				else
				{
				    count_f++;
				}
			    }
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
			    if (clsno_f == RFAT_CLSNO_NONE)
			    {
				clsno_f = clsno;
				index_f = index;
			    }
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

			    dir++;
			    index++;
			}
			else
			{ 
			    if ((dir->dir_attr & RFAT_DIR_ATTR_LONG_NAME_MASK) & RFAT_DIR_ATTR_LONG_NAME)
			    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
				ordinal = (dir->dir_name[0] & 0x1f); /* LDIR_Ord    */
				
				if (ordinal == 0)
				{
				    sequence = 0; /* mark as bad sequence */
				}
				else
				{
				    if (dir->dir_name[0] & 0x40)
				    {
					clsno_m = clsno;
					index_m = index;

					sequence = ordinal | RFAT_LDIR_SEQUENCE_FIRST;
					chksum   = dir->dir_crt_time_tenth;     /* LDIR_ChkSum */
				    }
				    else
				    {
					if (((sequence & ~RFAT_LDIR_SEQUENCE_INDEX) == (ordinal +1)) && (chksum == dir->dir_crt_time_tenth))
					{
					    sequence = (sequence & RFAT_LDIR_SEQUENCE_MISMATCH) | ordinal;
					}
					else
					{
					    sequence = 0; /* mark as bad sequence */
					}
				    }
				}

				if ((sequence != 0) && !(sequence & RFAT_LDIR_SEQUENCE_MISMATCH))
				{
				    if (!(*callback)(volume, private, dir, sequence))
				    {
					sequence |= RFAT_LDIR_SEQUENCE_MISMATCH;
				    }
				}
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

				dir++;
				index++;
			    }
			    else
			    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 0)
				if ((*callback)(volume, private, dir))
				{
				    clsno_m = clsno;
				    index_m = index;

				    done = TRUE;
				}
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 0) */

				if ((sequence & RFAT_LDIR_SEQUENCE_INDEX) == 1)
				{
				    if (chksum == rfat_name_checksum_dosname(dir->dir_name))
				    {
					sequence = (sequence & RFAT_LDIR_SEQUENCE_MISMATCH) | RFAT_LDIR_SEQUENCE_LAST;
				    }
				    else
				    {
					sequence = 0;
				    }
				}

				if (!(sequence & RFAT_LDIR_SEQUENCE_MISMATCH))
				{
				    if ((*callback)(volume, private, dir, sequence))
				    {
					if (sequence == 0)
					{
					    clsno_m = clsno;
					    index_m = index;
					}

					done = TRUE;
				    }
				}

				sequence = 0;

#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 0) */

				if (!done)
				{
				    dir++;
				    index++;
				}
			    }
			}
		    }
		    while ((status == F_NO_ERROR) && !done && (dir != dir_e));

		    if ((status == F_NO_ERROR) && !done)
		    {
			blkno++;
		    }
		}
	    }
	}
    }

    if (status == F_NO_ERROR)
    {
	if (p_clsno)
	{
	    if (dir != NULL)
	    {
		*p_clsno = clsno_m;
		*p_index = index_m;
	    }
	    else
	    {
		if (count != 0)
		{
		    if (count_f == count)
		    {
			/* Found a matching set of entries ...
			 */
			*p_clsno = clsno_f;
			*p_index = index_f;
		    }
		    else
		    {
			/* Need to allocate possibly a new cluster.
			 */
			    
			if ((clsno == RFAT_CLSNO_NONE) && (volume->type != RFAT_VOLUME_TYPE_FAT32))
			{
			    if (((index + count) << RFAT_DIR_SHIFT) <= (volume->root_blkcnt * RFAT_BLK_SIZE))
			    {
				*p_clsno = RFAT_CLSNO_NONE;
				*p_index = index;
			    }
			    else
			    {
				*p_clsno = RFAT_CLSNO_NONE;
				*p_index = 0x00010000;
			    }
			}
			else
			{
			    if (((index << RFAT_DIR_SHIFT) & ~volume->cls_mask) == (((index + count) << RFAT_DIR_SHIFT) & ~volume->cls_mask))
			    {
				*p_clsno = clsno;
				*p_index = index;
			    }
			    else
			    {
				if ((index + count) <= 0x00010000)
				{
				    *p_clsno = clsno;
				    *p_index = index | 0x00020000;
				}
				else
				{
				    *p_clsno = RFAT_CLSNO_NONE;
				    *p_index = 0x00010000;
				}
			    }
			}
		    }
		}
	    }
	}

	if (p_dir)
	{
	    *p_dir = dir;
	}
    }

    return status;
}

/*
 * filename   incoming full path
 * p_filename last path element 
 * p_clsno    clsno of parent directory of last path element
 */

static int rfat_path_find_directory(rfat_volume_t *volume, const char *filename, const char **p_filename, uint32_t *p_clsno)
{
    int status = F_NO_ERROR;
    unsigned int cc;
    const char *filename_e;
    uint32_t clsno;
    rfat_dir_t *dir;

    if (*filename == '\0')
    {
	status = F_ERR_INVALIDNAME;
    }
    else if ((*filename == '/') || (*filename == '\\'))
    {
	clsno = RFAT_CLSNO_NONE;

        filename++;
    }
    else
    {
	if (volume->cwd_clsno != RFAT_CLSNO_END_OF_CHAIN)
	{
	    clsno = volume->cwd_clsno;
	}
	else
	{
	    status = F_ERR_INVALIDDIR;
	}
    }

    if (status == F_NO_ERROR)
    {
	do
	{
	    /* Scan ahead to see whether there is a directory left.
	     */
	    filename_e = filename;
	    
	    do
	    {
		cc = *filename_e++;
	    }
	    while ((cc != '/') && (cc != '\\') && (cc != '\0'));
	    
	    if ((cc == '/') || (cc == '\\'))
	    {
		status = rfat_path_convert_filename(volume, filename, &filename);

		if (status == F_NO_ERROR)
		{
		    if ((volume->dir.dir_name[0] == '.') && (volume->dir.dir_name[1] == ' '))
		    {
			/* This is a special case here, as there is no "." or ".." entry
			 * in the root directory.
			 */
		    }
		    else
		    {
			status = rfat_path_find_entry(volume, clsno, 0, 0, rfat_path_find_callback_name, NULL, NULL, NULL, &dir);

			if (status == F_NO_ERROR)
			{
			    if (dir != NULL)
			    {
				if (dir->dir_attr & RFAT_DIR_ATTR_DIRECTORY)
				{
				    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
				    {
					clsno = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				    }
				    else
				    { 
					clsno = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				    }
				}
				else
				{
				    status = F_ERR_INVALIDDIR;
				}
			    }
			    else
			    {
				status = F_ERR_INVALIDDIR;
			    }
			}
		    }
		}
	    }
	}
	while ((status == F_NO_ERROR) && ((cc == '/') || (cc == '\\')));
    }

    if (status == F_NO_ERROR)
    {
	*p_filename = filename;
        *p_clsno = clsno;
    }

    return status;
}

static int rfat_path_find_file(rfat_volume_t *volume, const char *filename, uint32_t *p_clsno, uint32_t *p_index, rfat_dir_t **p_dir)
{
    int status = F_NO_ERROR;
    uint32_t clsno_d;

    status = rfat_path_find_directory(volume, filename, &filename, &clsno_d);

    if (status == F_NO_ERROR)
    {
	status = rfat_path_convert_filename(volume, filename, NULL);

	if (status == F_NO_ERROR)
	{
	    status = rfat_path_find_entry(volume, clsno_d, 0, 0, rfat_path_find_callback_name, NULL, p_clsno, p_index, p_dir);
	    
	    if (status == F_NO_ERROR)
	    {
		if (*p_dir == NULL)
		{
		    status = F_ERR_NOTFOUND;
		}
	    }
	}
    }

    return status;
}

static int rfat_path_find_next(rfat_volume_t *volume, F_FIND *find)
{
    int status = F_NO_ERROR;
    char *filename;
    uint32_t clsno, index;
    rfat_dir_t *dir;

    status = rfat_path_find_entry(volume, find->find_clsno, find->find_index, 0, rfat_path_find_callback_pattern, find->find_pattern, &clsno, &index, &dir);

    if (status == F_NO_ERROR)
    {
	if (dir != NULL)
	{
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	    /* It's possible to have a sfn_name only ("." and ".." for example).
	     */
	    if (volume->lfn_count)
	    {
		filename = rfat_name_uniname_to_cstring(volume->lfn_name, volume->lfn_count, find->filename, (find->filename + sizeof(find->filename)));
	    }
	    else
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    {
		filename = rfat_name_dosname_to_cstring(dir->dir_name, dir->dir_nt_reserved, find->filename, (find->filename + sizeof(find->filename)));
	    }

	    if (filename != NULL)
	    {
		memcpy(&find->name[0], &dir->dir_name[0], (F_MAXNAME+F_MAXEXT));
		
		find->attr = dir->dir_attr;
		find->ctime = RFAT_FTOHS(dir->dir_wrt_time);
		find->cdate = RFAT_FTOHS(dir->dir_wrt_date);
		find->filesize = RFAT_FTOHL(dir->dir_file_size);

		if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		{
		    find->cluster = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
		}
		else
		{
		    find->cluster = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
		}

		find->find_clsno = clsno;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		find->find_index = index + volume->dir_entries +1;
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
		find->find_index = index +1;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	    }
	    else
	    {
		status = F_ERR_TOOLONGNAME;
	    }
        }
        else
        {
            status = F_ERR_NOTFOUND;
        }
    }

    return status;
}

static void rfat_path_setup_entry(rfat_volume_t *volume, const char *dosname, uint8_t attr, uint32_t first_clsno, uint16_t ctime, uint16_t cdate, rfat_dir_t *dir)
{
    if (dosname)
    {
	memcpy(dir->dir_name, dosname, 11);
	
	dir->dir_nt_reserved = 0x00;
    }
    else
    {
	if (dir != &volume->dir)
	{
	    memcpy(dir->dir_name, volume->dir.dir_name, 11);
	
	    dir->dir_nt_reserved = volume->dir.dir_nt_reserved;
	}
    }

    dir->dir_attr = attr;
    
    if (volume->type != RFAT_VOLUME_TYPE_FAT32)
    {
	dir->dir_crt_time_tenth = 0;
	dir->dir_crt_time = 0;
	dir->dir_crt_date = 0;
	dir->dir_acc_date = 0;
	dir->dir_clsno_hi = 0;
    }
    else
    {
	dir->dir_crt_time_tenth = 0;
	dir->dir_crt_time = ctime;
	dir->dir_crt_date = cdate;
	dir->dir_acc_date = 0;
	dir->dir_clsno_hi = RFAT_HTOFS(first_clsno >> 16);
    }
    
    dir->dir_wrt_time = ctime;
    dir->dir_wrt_date = cdate;
    dir->dir_clsno_lo = RFAT_HTOFS(first_clsno & 0xffff);
    dir->dir_file_size = RFAT_HTOFL(0);
}

static int rfat_path_create_entry(rfat_volume_t *volume, uint32_t clsno_d, uint32_t clsno, uint32_t index, const char *dosname, uint8_t attr, uint32_t first_clsno, uint16_t ctime, uint16_t cdate)
{
    int status = F_NO_ERROR;
    uint32_t blkno, blkno_s, clsno_s;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    unsigned int chksum, mask, prefix;
    rfat_dir_t *dir;
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
    uint32_t blkno_e;
    unsigned int sequence, offset, i, s, cc;
    rfat_dir_t *dir_e;
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
#else  /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
    rfat_dir_t *dir;
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    rfat_cache_entry_t *entry;

    if (index == 0x00010000)
    {
	status = F_ERR_NOMOREENTRY;
    }
    else
    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	if (!dosname && ((volume->dir.dir_name[0] == '\0') || (volume->dir.dir_nt_reserved & RFAT_DIR_TYPE_LOSSY)))
	{
	    rfat_name_uniname_to_dosname(volume->lfn_name, volume->lfn_count, volume->dir.dir_name, &prefix);

	    volume->dir.dir_nt_reserved = 0;

	    if (prefix)
	    {
		if (prefix > 6)
		{
		    prefix = 6;
		}
		
		/* First check unique entries of the <name>~[1-9].<ext> form.
		 */
		
		volume->dir.dir_name[prefix +0] = '~';
		
		mask = ((prefix +1) << 9);
		
		status = rfat_path_find_entry(volume, clsno_d, 0, 0, rfat_path_find_callback_unique, &mask, NULL, NULL, NULL);

		if (status == F_NO_ERROR)
		{
		    mask &= 0x000001ff;
		
		    if (mask != 0x000001ff)
		    {
			volume->dir.dir_name[prefix +1] = '1';
			
			while (mask & 1)
			{
			    volume->dir.dir_name[prefix +1] += 1;
			    mask >>= 1;
			}
		    }
		}
	    }
	    else
	    {
		mask = 0x000001ff;
	    }

	    if (status == F_NO_ERROR)
	    {
		if (mask & 1)
		{
		    /* Then check unique entries of the <name><checksum>~1.<ext> form.
		     * Loop till a vaild unique name was found by stepping linear over
		     * "chksum".
		     */

		    if (prefix > 2)
		    {
			prefix = 2;
		    }

		    chksum = rfat_name_checksum_uniname(volume->lfn_name, volume->lfn_count);

		    do
		    {
			volume->dir.dir_name[prefix +0] = rfat_name_nibble_to_char_table[(chksum >> 12) & 15];
			volume->dir.dir_name[prefix +1] = rfat_name_nibble_to_char_table[(chksum >>  8) & 15];
			volume->dir.dir_name[prefix +2] = rfat_name_nibble_to_char_table[(chksum >>  4) & 15];
			volume->dir.dir_name[prefix +3] = rfat_name_nibble_to_char_table[(chksum >>  0) & 15];
			volume->dir.dir_name[prefix +4] = '~';
			volume->dir.dir_name[prefix +5] = '1';
			
			status = rfat_path_find_entry(volume, clsno_d, 0, 0, rfat_path_find_callback_unique, NULL, NULL, NULL, &dir);
			
			if (status == F_NO_ERROR) 
			{
			    if (dir != NULL)
			    {
				chksum += 1;
			    }
			}
		    }
		    while ((status == F_NO_ERROR) && (dir != NULL));
		}
	    }
	}
	else
	{
	    volume->dir_entries = 0;
	}

	if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	{
	    if (index & 0x00020000)
	    {
		/* The appending of a new cluster to a directory is a tad tricky. One has to allocate
		 * a cluster first, zero out it's contents, link the cluster to the end of the chain.
		 * Also the fat cache has to be flushed as it's the final operation of a sequence (see f_mkdir).
		 * The issue at hand is that if a new cluster is linked in that is not zeroed out, the
		 * directory is invalid. One the other hand, we cannot write a directory entry to a directory
		 * that has uncommited clusters in the fat cache.
		 */
		
		status = rfat_cluster_chain_create(volume, RFAT_CLSNO_NONE, 1, &clsno_s, NULL);
		
		if (status == F_NO_ERROR)
		{
		    blkno_s = RFAT_CLSNO_TO_BLKNO(clsno_s);

		    status = rfat_volume_zero(volume, blkno_s, volume->cls_blk_size, NULL);

		    if (status == F_NO_ERROR)
		    {
			status = rfat_cluster_write(volume, clsno, clsno_s, TRUE);

			if (status == F_NO_ERROR)
			{
			    index &= ~0x00020000;

			    if (!((index << RFAT_DIR_SHIFT) & volume->cls_mask))
			    {
				clsno = clsno_s;

				/* If "index" is the first one of a new cluster,
				 * one might as well avoid having to read the 
				 * dir_cache entry.
				 */
				status = rfat_dir_cache_zero(volume, blkno_s, &entry);
			    }
			}
		    }
		}
	    }

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
	    /* For TRANSACTION_SAFE all of this happens throu rfat_volume_record().
	     */
	
	    /* The fat cache needs to be flushed before any dir entry update.
	     */
	    status = rfat_fat_cache_flush(volume);

	    if (status == F_NO_ERROR)
	    {
		if (clsno == RFAT_CLSNO_NONE)
		{
		    clsno = volume->root_clsno;
		    blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		    blkno_e = volume->root_blkno + volume->root_blkcnt;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
		}
		else
		{
		    blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_INDEX_TO_BLKCNT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		    blkno_e = blkno + volume->cls_blk_size;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
		}
		
		status = rfat_dir_cache_read(volume, blkno, &entry);

		if (status == F_NO_ERROR)
		{
		    dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index)));

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		    if (volume->dir_entries != 0)
		    {
			dir_e = (rfat_dir_t*)((void*)(entry->data + RFAT_BLK_SIZE));

			sequence = 0x40 | volume->dir_entries;
			chksum = rfat_name_checksum_dosname(volume->dir.dir_name);

			do
			{
			    s = 0;
			    i = ((sequence & 0x1f) - 1) * 13;
		
			    dir->dir_name[0] = sequence;
			    dir->dir_attr = RFAT_DIR_ATTR_LONG_NAME;
			    dir->dir_nt_reserved = 0x00;
			    dir->dir_crt_time_tenth = chksum;
			    dir->dir_clsno_lo = 0x0000;
		
			    do
			    {
				offset = rfat_path_ldir_name_table[s++];
		    
				if (i < volume->lfn_count)
				{
				    cc = volume->lfn_name[i];
			
				    ((uint8_t*)dir)[offset +0] = cc;
				    ((uint8_t*)dir)[offset +1] = cc >> 8;
				}
				else
				{
				    if (i == volume->lfn_count)
				    {
					((uint8_t*)dir)[offset +0] = 0x00;
					((uint8_t*)dir)[offset +1] = 0x00;
				    }
				    else
				    {
					((uint8_t*)dir)[offset +0] = 0xff;
					((uint8_t*)dir)[offset +1] = 0xff;
				    }
				}

				i++;
			    }
			    while (s < 13);
		
			    dir++;

			    if (dir == dir_e)
			    {
				status = rfat_dir_cache_write(volume);

				blkno++;

				if (blkno == blkno_e)
				{
				    status = rfat_cluster_read(volume, clsno, &clsno);
		
				    if (status == F_NO_ERROR)
				    {
					blkno = RFAT_CLSNO_TO_BLKNO(clsno);
					blkno_e = blkno + volume->cls_blk_size;
				    }
				}

				if (status == F_NO_ERROR)
				{
				    status = rfat_dir_cache_read(volume, blkno, &entry);
				}
			    }

			    sequence = (sequence & 0x1f) -1;
			}
			while ((status == F_NO_ERROR) && (sequence != 0));
		    }

		    if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
		    {
			/* Special semantics if "first_clsno" == RFAT_CLUSTER_END_OF_CHAIN. In that
			 * case volume->dir contains a valid template from f_rename, and
			 * "attr" gets merged with volume->dir.dir_nt_reserved (as this
			 * gets blown away by the name conversion).
			 */
			if (first_clsno == RFAT_CLSNO_END_OF_CHAIN)
			{
			    memcpy(dir, &volume->dir, sizeof(rfat_dir_t));

			    dir->dir_nt_reserved = ((volume->dir.dir_nt_reserved & (RFAT_DIR_TYPE_LCASE_NAME | RFAT_DIR_TYPE_LCASE_EXT)) |
						    (attr & ~(RFAT_DIR_TYPE_LCASE_NAME | RFAT_DIR_TYPE_LCASE_EXT)));
			}
			else
			{
			    rfat_path_setup_entry(volume, dosname, attr, first_clsno, ctime, cdate, dir);
			}

			status = rfat_dir_cache_write(volume);
		    }
		}
	    }

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
	
	    if (status == F_NO_ERROR)
	    {
		/* Special semantics if "first_clsno" == RFAT_CLUSTER_END_OF_CHAIN. In that
		 * case volume->dir contains a valid template from f_rename, and
		 * "attr" gets merged with volume->dir.dir_nt_reserved (as this
		 * gets blown away by the name conversion).
		 */
		if (first_clsno == RFAT_CLSNO_END_OF_CHAIN)
		{
		    volume->dir.dir_nt_reserved = ((volume->dir.dir_nt_reserved & (RFAT_DIR_TYPE_LCASE_NAME | RFAT_DIR_TYPE_LCASE_EXT)) |
						   (attr & ~(RFAT_DIR_TYPE_LCASE_NAME | RFAT_DIR_TYPE_LCASE_EXT)));
		}
		else
		{
		    rfat_path_setup_entry(volume, dosname, attr, first_clsno, ctime, cdate, &volume->dir);
		}

		volume->dir_flags |= RFAT_DIR_FLAG_CREATE_ENTRY;
		volume->dir_clsno = clsno;
		volume->dir_index = index;

		status = rfat_volume_record(volume);
	    }
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
	}
    }

    return status;
}

/* To destroy a directory/file, first the directory entry is deleted. Then the assocociated
 * cluster chain is freed.
 */

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
static int rfat_path_destroy_entry(rfat_volume_t *volume, uint32_t clsno, uint32_t index, uint32_t entries, uint32_t first_clsno)
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
static int rfat_path_destroy_entry(rfat_volume_t *volume, uint32_t clsno, uint32_t index, uint32_t first_clsno)
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
{
    int status = F_NO_ERROR;
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    unsigned int sequence;
    uint32_t blkno, blkno_e;
    rfat_dir_t *dir, *dir_e;
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    uint32_t blkno;
    rfat_dir_t *dir;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    rfat_cache_entry_t *entry;

    if (clsno == RFAT_CLSNO_NONE)
    {
	clsno = volume->root_clsno;
	blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	blkno_e = volume->root_blkno + volume->root_blkcnt;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    }
    else
    {
	blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_INDEX_TO_BLKCNT(index);
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	blkno_e = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    }
    
    status = rfat_dir_cache_read(volume, blkno, &entry);

    if (status == F_NO_ERROR)
    {
        dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index)));

#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	if (entries != 0)
	{
	    sequence = entries;

	    dir_e = (rfat_dir_t*)((void*)(entry->data + RFAT_BLK_SIZE));

	    do
	    {
		dir->dir_name[0] = 0xe5;
		
		dir++;
		
		if (dir == dir_e)
		{
		    status = rfat_dir_cache_write(volume);

		    blkno++;

		    if (blkno == blkno_e)
		    {
			status = rfat_cluster_read(volume, clsno, &clsno);
		
			if (status == F_NO_ERROR)
			{
			    blkno = RFAT_CLSNO_TO_BLKNO(clsno);
			    blkno_e = blkno + volume->cls_blk_size;
			}
		    }

		    if (status == F_NO_ERROR)
		    {
			status = rfat_dir_cache_read(volume, blkno, &entry);
		    }
		}
		
		sequence--;
	    }
	    while ((status == F_NO_ERROR) && (sequence != 0));
	}

	if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
	{
	    dir->dir_name[0] = 0xe5;
        
	    status = rfat_dir_cache_write(volume);
        
	    if (status == F_NO_ERROR)
	    {
		if (first_clsno != RFAT_CLSNO_NONE)
		{
		    status = rfat_cluster_chain_destroy(volume, first_clsno, RFAT_CLSNO_FREE);
		}
	    }
	}


	if (status == F_NO_ERROR)
	{
	    /* The post condition for rfat_path_destroy_entry is that the fat cache
	     * is flushed, so unconditionally do it here.
	     */
	    status = rfat_fat_cache_flush(volume);
	}
    }

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */

    if (first_clsno != RFAT_CLSNO_NONE)
    {
	status = rfat_cluster_chain_destroy(volume, first_clsno, RFAT_CLSNO_FREE);
    }

    if (status == F_NO_ERROR)
    {
	volume->dir_flags |= RFAT_DIR_FLAG_DESTROY_ENTRY;
	volume->del_clsno = clsno;
	volume->del_index = index;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
	volume->del_entries = entries;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

	status = rfat_volume_record(volume);
    }

#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */


    return status;
}


/*****************************************************************************************************************************************/


static rfat_file_t * rfat_file_enumerate(rfat_volume_t *volume, rfat_file_t *file, uint32_t clsno, uint32_t index)
{
#if (RFAT_CONFIG_MAX_FILES == 1)

    rfat_file_t *file_s;

    if (file == NULL)
    {
	file_s = &volume->file_table[0];

	if (file_s->mode && (file_s->dir_clsno == clsno) && (file_s->dir_index == index))
	{
	    file = file_s;
	}
    }

#else /* (RFAT_CONFIG_MAX_FILES == 1) */

    rfat_file_t *file_s, *file_e;

    file_s = (file == NULL) ? &volume->file_table[0] : NULL;
    file_e = &volume->file_table[RFAT_CONFIG_MAX_FILES];

    file = NULL;

    do
    {
	if (file_s->mode && (file_s->dir_clsno == clsno) && (file_s->dir_index == index))
	{
	    file = file_s;
	    break;
	}

	file_s++;
    }
    while (file_s < file_e);
#endif /* (RFAT_CONFIG_MAX_FILES == 1) */

    return file;
}

static int rfat_file_sync(rfat_volume_t *volume, rfat_file_t *file, int access, int modify, uint32_t first_clsno, uint32_t length)
{
    int status = F_NO_ERROR;
    uint16_t ctime, cdate;
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
    uint32_t clsno, blkno, index;
    rfat_cache_entry_t *entry;
    rfat_dir_t *dir;
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)

    volume->dir.dir_clsno_lo = RFAT_HTOFS(first_clsno & 0xffff);
    volume->dir.dir_file_size = RFAT_HTOFL(length);
    
    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
    {
	volume->dir.dir_clsno_hi = RFAT_HTOFS(first_clsno >> 16);
    }
    
    volume->dir_flags |= RFAT_DIR_FLAG_SYNC_ENTRY;
    
    if (access || modify)
    {
#if defined(RFAT_PORT_CORE_TIMEDATE)
	RFAT_PORT_CORE_TIMEDATE(&ctime, &cdate);
#else /* RFAT_PORT_CORE_TIMEDATE */
	ctime = 0;
	cdate = 0;
#endif /* RFAT_PORT_CORE_TIMEDATE */

	if (access)
	{
	    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
	    {
		volume->dir.dir_acc_date = RFAT_HTOFS(cdate);
		    
		volume->dir_flags |= RFAT_DIR_FLAG_ACCESS_ENTRY;
	    }
	}

	if (modify)
	{
	    volume->dir.dir_wrt_time = RFAT_HTOFS(ctime);
	    volume->dir.dir_wrt_date = RFAT_HTOFS(cdate);

	    volume->dir_flags |= RFAT_DIR_FLAG_MODIFY_ENTRY;
	}
    }
	
    volume->dir_clsno = file->dir_clsno;
    volume->dir_index = file->dir_index;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    volume->dir_entries = 0;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

    status = rfat_volume_record(volume);

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

    status = rfat_fat_cache_flush(volume);

    if (status == F_NO_ERROR)
    {
	clsno = file->dir_clsno;
	index = file->dir_index;

	if (clsno == RFAT_CLSNO_NONE)
	{
	    blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index);
	}
	else
	{
	    blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_INDEX_TO_BLKCNT(index);
	}
    
	status = rfat_dir_cache_read(volume, blkno, &entry);
    
	if (status == F_NO_ERROR)
	{
	    dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index)));

	    dir->dir_clsno_lo = RFAT_HTOFS(first_clsno & 0xffff);
	    dir->dir_file_size = RFAT_HTOFL(length);
	
	    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
	    {
		dir->dir_clsno_hi = RFAT_HTOFS(first_clsno >> 16);
	    }
	
	    if (access || modify)
	    {
#if defined(RFAT_PORT_CORE_TIMEDATE)
		RFAT_PORT_CORE_TIMEDATE(&ctime, &cdate);
#else /* RFAT_PORT_CORE_TIMEDATE */
		ctime = 0;
		cdate = 0;
#endif /* RFAT_PORT_CORE_TIMEDATE */

		if (access)
		{
		    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		    {
			dir->dir_acc_date = RFAT_HTOFS(cdate);
		    }
		}

		if (modify)
		{
		    dir->dir_attr |= RFAT_DIR_ATTR_ARCHIVE;
		    dir->dir_wrt_time = RFAT_HTOFS(ctime);
		    dir->dir_wrt_date = RFAT_HTOFS(cdate);
		}
	    }

	    status = rfat_dir_cache_write(volume);

	    if (status == F_NO_ERROR)
	    {
		status = rfat_volume_clean(volume, status);
	    }
	}
    }
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

    return status;
}


static int rfat_file_flush(rfat_volume_t *volume, rfat_file_t *file, int close)
{
    int status = F_NO_ERROR;

    if (volume->state == RFAT_VOLUME_STATE_MOUNTED)
    {
	/* The data cache is first flushed and outstanding disk operations
	 * are finished. Then however the fat cache flush and  the directory updates
	 * are only performed uncoditionally irregardless of whether there were
	 * outstanding errors.
	 */

	status = rfat_data_cache_flush(volume, file);

	if (status == F_NO_ERROR)
	{
	    status = rfat_disk_sync(volume->disk, &file->status);
	}

	if (file->status == F_NO_ERROR)
	{
	    file->status = status;
	}

	if (close || (file->flags & (RFAT_FILE_FLAG_DIR_MODIFIED | RFAT_FILE_FLAG_DATA_MODIFIED)))
	{
	    status = rfat_file_sync(volume, file, close, (file->flags & (RFAT_FILE_FLAG_DIR_MODIFIED | RFAT_FILE_FLAG_DATA_MODIFIED)), file->first_clsno, file->length);
	}
    }

    if (status == F_NO_ERROR)
    {
	file->flags &= ~(RFAT_FILE_FLAG_DATA_MODIFIED | RFAT_FILE_FLAG_DIR_MODIFIED);
    }

    if (file->status != F_NO_ERROR)
    {
	status = file->status;
    }

    return status;
}

static int rfat_file_seek(rfat_volume_t *volume, rfat_file_t *file, uint32_t position)
{
    int status = F_NO_ERROR;
    uint32_t clsno, clscnt, offset;

    if ((file->mode & RFAT_FILE_MODE_WRITE) && ((file->position & ~RFAT_BLK_MASK) != (position & ~RFAT_BLK_MASK)))
    {
	status = rfat_data_cache_flush(volume, file);
    }

    if (status == F_NO_ERROR)
    {
	if (position == 0)
	{
	    /* A seek to position 0 (i.e. rewind) always succeeds.
	     */
	    if (file->first_clsno != RFAT_CLSNO_NONE)
	    {
		file->clsno = file->first_clsno;
		file->blkno = RFAT_CLSNO_TO_BLKNO(file->clsno);
		file->blkno_e = file->blkno + volume->cls_blk_size;
	    }
	
	    file->position = 0;
	}
	else
	{
	    if (file->position != position)
	    {
		if (file->length == 0)
		{
		    /* Any seek for file->length == 0 always succeeds.
		     */
		    file->position = position;
		}
		else
		{
		    if (file->first_clsno == RFAT_CLSNO_NONE)
		    {
			/* A seek to any non-zero position with no
			 * cluster in the chain is an error.
			 */
			status = F_ERR_EOF;
		    }
		    else
		    {
			if (position < file->length)
			{
			    offset = position;
			}
			else
			{
			    offset = file->length;
			}

			if (status == F_NO_ERROR)
			{
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
			    if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
			    {
				clsno = file->first_clsno + RFAT_OFFSET_TO_CLSCNT(offset -1);
			    }
			    else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
			    {
				if ((offset == file->length) && (file->last_clsno != RFAT_CLSNO_NONE))
				{
				    clsno = file->last_clsno;
				}
				else
				{
				    if ((file->position == 0) || (file->position > offset))
				    {
					clsno = file->first_clsno;
					clscnt = RFAT_OFFSET_TO_CLSCNT(offset -1);
				    }
				    else
				    {
					clsno = file->clsno;
					clscnt = RFAT_OFFSET_TO_CLSCNT(offset -1) - RFAT_OFFSET_TO_CLSCNT(file->position -1);
				    }

				    if (clscnt != 0)
				    {
					status = rfat_cluster_chain_seek(volume, clsno, clscnt, &clsno);
				    }
				}
			    }
			
			    if (status == F_NO_ERROR)
			    {
				file->position = position;
				file->clsno = clsno;

				if (!(offset & volume->cls_mask))
				{
				    file->blkno = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
				    file->blkno_e = file->blkno;
				}
				else
				{
				    file->blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_OFFSET_TO_BLKCNT(offset);
				    file->blkno_e = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;
				}

				if (offset == file->length)
				{
				    file->last_clsno = clsno;
				}
			    }
			}
		    }
		}
	    }
	}
    }

    return status;
}

static int rfat_file_shrink(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    if (file->position == 0)
    {
	if (file->first_clsno != RFAT_CLSNO_NONE)
	{
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)

	    /* Defer the deletion of file->first_clsno to the next rfat_file_flush(),
	     * so that file->first_clsno ends up on the disk before the cluster is made
	     * available again.
	     */
		   
	    status = rfat_cluster_chain_destroy(volume, file->first_clsno, RFAT_CLSNO_FREE);

	    if (status == F_NO_ERROR)
	    {
		status = rfat_file_sync(volume, file, FALSE, FALSE, RFAT_CLSNO_NONE, file->length);
	    }

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

	    /* To be on the safe side, the directory entry on the disk is updated first,
	     * so that "file->length" is always correct, and in case the whole chain gets
	     * deleted "file->first_clsno" on disk is properly zeroed out.
	     */

	    status = rfat_file_sync(volume, file, FALSE, FALSE, RFAT_CLSNO_NONE, file->length);

	    if (status == F_NO_ERROR)
	    {
		status = rfat_volume_dirty(volume);

		if (status == F_NO_ERROR)
		{
		    status = rfat_cluster_chain_destroy(volume, file->first_clsno, RFAT_CLSNO_FREE);
		}
	    }
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
	}

	if (status == F_NO_ERROR)
	{
	    file->first_clsno = RFAT_CLSNO_NONE;
	    file->last_clsno = RFAT_CLSNO_NONE;
		
	    /* file->position is 0 here, but clsno/blkno/blkno_e
	     * point to the first cluster, which just got deleted.
	     */
	    file->clsno = RFAT_CLSNO_NONE;
	    file->blkno = RFAT_BLKNO_INVALID;
	    file->blkno_e = RFAT_BLKNO_INVALID;
	}
    }
    else
    {
	if (file->clsno != RFAT_CLSNO_NONE)
	{
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
	    status = rfat_volume_dirty(volume);

	    if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
	    {
		status = rfat_cluster_chain_destroy(volume, file->clsno, RFAT_CLSNO_END_OF_CHAIN);
		
		if (status == F_NO_ERROR)
		{
		    file->last_clsno = file->clsno;
		}
	    }
	}
	else
	{
	    status = F_ERR_EOF;
	}
    }

    if (status == F_NO_ERROR)
    {
	if (file->length != file->position)
	{
	    file->flags |= RFAT_FILE_FLAG_DIR_MODIFIED;
	    file->length = file->position;
	}

	file->flags |= RFAT_FILE_FLAG_END_OF_CHAIN;

#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
	if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
	{
	    if (file->length >= (RFAT_FILE_SIZE_MAX & ~volume->cls_mask))
	    {
		file->size = RFAT_FILE_SIZE_MAX;
	    }
	    else
	    {
		file->size = (file->length + volume->cls_mask) & ~volume->cls_mask;
	    }
	}
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
    }

    if (file->status == F_NO_ERROR)
    {
	file->status = status;
    }

    return status;
}

static int rfat_file_extend(rfat_volume_t *volume, rfat_file_t *file, uint32_t length)
{
    int status = F_NO_ERROR;
    uint32_t clsno, clscnt, clsno_a, clsno_l, clsno_n, clsdata, blkno, blkno_e, blkcnt, count, size, position, offset, length_o;
    rfat_cache_entry_t *entry;

    /* Compute below:
     *
     * clsno      file->last_clsno or RFAT_CLSNO_NONE (corresponding to file->length)
     * clsno_l    file->last_clsno (corresponding to length)
     * clsno_n    next cluster after clsno (or first allocated)
     */

    length_o = file->length;

#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
    if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
    {
	if (length_o == 0)
	{
	    clsno = file->first_clsno;
	    clsno_n = clsno;
	}
	else
	{
	    clsno = file->first_clsno + RFAT_OFFSET_TO_CLSCNT(length_o -1);
	    clsno_n = clsno +1;
	}

	clsno_l = file->first_clsno + RFAT_OFFSET_TO_CLSCNT(length -1);
    }
    else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
    {
	if (length_o == 0)
	{
	    clsno = file->first_clsno;
	}
	else
	{
	    if (file->first_clsno == RFAT_CLSNO_NONE)
	    {
		clsno = RFAT_CLSNO_NONE;
		length_o = 0;
	    }
	    else
	    {
		if (file->last_clsno != RFAT_CLSNO_NONE)
		{
		    clsno = file->last_clsno;
		}
		else
		{
		    if (!file->position || (file->position > length_o))
		    {
			clsno = file->first_clsno;
			clscnt = RFAT_OFFSET_TO_CLSCNT(length_o -1);
		    }
		    else
		    {
			clsno = file->clsno;
			clscnt = RFAT_OFFSET_TO_CLSCNT(length_o -1) - RFAT_OFFSET_TO_CLSCNT(file->position -1);
		    }
		    
		    if (clscnt != 0)
		    {
			status = rfat_cluster_chain_seek(volume, clsno, clscnt, &clsno);
		    }
		}

		if (status == F_NO_ERROR)
		{
		    file->last_clsno = clsno;
		}
	    }
	}

	if (status == F_NO_ERROR)
	{
	    if ((length_o == 0) || (clsno == RFAT_CLSNO_NONE))
	    {
		clscnt = RFAT_SIZE_TO_CLSCNT(length);
	    }
	    else
	    {
		clscnt = RFAT_SIZE_TO_CLSCNT(length) - RFAT_SIZE_TO_CLSCNT(length_o);
	    }
	    
	    clsno_l = clsno;
	    clsno_n = RFAT_CLSNO_NONE;

	    if (clscnt)
	    {
		/* While the last cluster had not been seen yet, step throu the trailing 
		 * list and make use of what had been there already.
		 *
		 * Normally this should only set RFAT_FILE_FLAG_END_OF_CHAIN, but there might
		 * be dangling, orphaned entries which one might make use of to begin with.
		 *
		 * N.b. file_first_clsno == RFAT_CLSNO_NONE implies RFAT_FILE_FLAG_END_OF_CHAIN 
		 * set.
		 */

		if (!(file->flags & RFAT_FILE_FLAG_END_OF_CHAIN))
		{
		    do
		    {
			status = rfat_cluster_read(volume, clsno_l, &clsdata);
			
			if (status == F_NO_ERROR)
			{
			    if (clsdata >= RFAT_CLSNO_LAST)
			    {
				file->flags |= RFAT_FILE_FLAG_END_OF_CHAIN;
			    }
			    else
			    {
				if ((clsdata >= 2) && (clsdata <= volume->last_clsno))
				{
				    if (clsno_n == RFAT_CLSNO_NONE)
				    {
					clsno_n = clsdata;
				    }

				    clsno_l = clsdata;
				    clscnt--;
				}
				else
				{
				    status = F_ERR_EOF;
				}
			    }
			}
		    }
		    while ((status == F_NO_ERROR) && !(file->flags & RFAT_FILE_FLAG_END_OF_CHAIN) && (clscnt != 0));
		}

		if (status == F_NO_ERROR)
		{
		    if (clscnt)
		    {
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
			status = rfat_volume_dirty(volume);
			
			if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
			{
#if (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1)
			  if (1 || (file->mode & RFAT_FILE_MODE_SEQUENTIAL))
			    {
				status = rfat_cluster_chain_create_sequential(volume, clsno_l, clscnt, &clsno_a, &clsno_l);
			    }
			    else
#endif /* (RFAT_CONFIG_SEQUENTIAL_SUPPORTED == 1) */
			    {
				status = rfat_cluster_chain_create(volume, clsno_l, clscnt, &clsno_a, &clsno_l);
			    }

			    if (clsno_n == RFAT_CLSNO_NONE)
			    {
				clsno_n = clsno_a;
			    }
			}
		    }
		}

		if (status == F_NO_ERROR)
		{
		    if (file->first_clsno == RFAT_CLSNO_NONE)
		    {
			file->first_clsno = clsno_n;
			
			file->clsno = clsno_n;
			file->blkno = RFAT_CLSNO_TO_BLKNO(clsno_n);
			file->blkno_e = file->blkno + volume->cls_blk_size;

			/* If a new cluster chain gets created, the dir entry needs to be updated to
			 * not lose the cluster chain.
			 */
			
			status = rfat_file_sync(volume, file, FALSE, FALSE, file->first_clsno, file->length);
		    }
		}
	    }
	}
    }

    if (status == F_NO_ERROR)
    {
	file->flags |= RFAT_FILE_FLAG_DIR_MODIFIED;
	file->last_clsno = clsno_l;
	file->length = length;

	if (file->position > length_o)
	{
	    position = length_o;
	    offset = (length_o + RFAT_BLK_MASK) & ~RFAT_BLK_MASK;
	    count  = file->position - length_o;

	    if (!(position & volume->cls_mask))
	    {
		/* If the old file->length was on a cluster boundary, then the 
		 * old cached file data should be flushed to get a nice sequential
		 * write sequence.
		 */
		status = rfat_data_cache_flush(volume, file);

		if (status == F_NO_ERROR)
		{
		    clsno = clsno_n;
		}
	    }

	    if (status == F_NO_ERROR)
	    {
		blkno = RFAT_CLSNO_TO_BLKNO(clsno) + RFAT_OFFSET_TO_BLKCNT(position);
		blkno_e = RFAT_CLSNO_TO_BLKNO(clsno) + volume->cls_blk_size;

		if (((position & RFAT_BLK_MASK) + count) < RFAT_BLK_SIZE)
		{
		    /* All data is within one block, simply go throu the cache.
		     * If the block is beyond the old file->length, is has to 
		     * be all zeros. Otherwise got throu the normal cache.
		     */
		
		    if (position >= offset)
		    {
			status = rfat_data_cache_zero(volume, file, blkno, &entry);
		    }
		    else
		    {
			status = rfat_data_cache_read(volume, file, blkno, &entry);
		    
			if (status == F_NO_ERROR)
			{
			    memset(entry->data + (position & RFAT_BLK_MASK), 0, count);
			}
		    }
		
		    if (status == F_NO_ERROR)
		    {
			rfat_data_cache_modify(volume, file);
		    
			position += count;
			count = 0;
		    
			if (!(position & RFAT_BLK_MASK))
			{
			    blkno++;
			}
		    }
		}
		else
		{
		    if (position & RFAT_BLK_MASK)
		    {
			size = (RFAT_BLK_SIZE - (position & RFAT_BLK_MASK));

			if (position >= offset)
			{
			    status = rfat_data_cache_zero(volume, file, blkno, &entry);
			}
			else
			{ 
			    status = rfat_data_cache_read(volume, file, blkno, &entry);
			
			    if (status == F_NO_ERROR)
			    {
				memset(entry->data + (RFAT_BLK_SIZE - size), 0, size);
			    }
			}
		    
			if (status == F_NO_ERROR)
			{
			    rfat_data_cache_modify(volume, file);

			    status = rfat_data_cache_write(volume, file);
			
			    if (status == F_NO_ERROR)
			    {
				position += size;
				count -= size;
                            
				blkno++;
			    }
			}
		    }

		    while ((status == F_NO_ERROR) && (count != 0))
		    {
			if (blkno == blkno_e)
			{
			    status = rfat_data_cache_flush(volume, file);

			    if (status == F_NO_ERROR)
			    {
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
				if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
				{
				    clsno++;
				    blkno_e += volume->cls_blk_size;
				}
				else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
				{
				    status = rfat_cluster_chain_seek(volume, clsno, 1, &clsno);
					
				    if (status == F_NO_ERROR)
				    {
					blkno = RFAT_CLSNO_TO_BLKNO(clsno);
					blkno_e = blkno + volume->cls_blk_size;
				    }
				}
			    }
			}
			
			if (status == F_NO_ERROR)
			{
			    if (count < RFAT_BLK_SIZE)
			    {
				status = rfat_data_cache_zero(volume, file, blkno, &entry);
			    
				if (status == F_NO_ERROR)
				{
				    rfat_data_cache_modify(volume, file);
				
				    position += count;
				    count = 0;
				}
			    }
			    else
			    {
				size = volume->cls_size - (position & volume->cls_mask);
			    
				if (size > count)
				{
				    size = count & ~RFAT_BLK_MASK;
				}
			    
				blkcnt = size >> RFAT_BLK_SHIFT;
			    
				status = rfat_data_cache_invalidate(volume, file, blkno, blkcnt);
			    
				if (status == F_NO_ERROR)
				{
				    status = rfat_volume_zero(volume, blkno, blkcnt, &file->status);

				    if (status == F_NO_ERROR)
				    {
					position += size;
					count -= size;
                                    
					blkno += blkcnt;
				    }

				    /* After any data related disk operation, "file->status" needs
				     * to be checked asynchronously for a previous error.
				     */
				    if (file->status != F_NO_ERROR)
				    {
					status = file->status;
				    }
				}
			    }
			}
		    }
		}
	    }

	    if (status == F_NO_ERROR)
	    {
		file->clsno = clsno;
		file->blkno = blkno;
		file->blkno_e = blkno_e;
	    }
	}
    }

    if (file->status == F_NO_ERROR)
    {
	file->status = status;
    }

    return status;
}

#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
static int rfat_file_reserve(rfat_volume_t *volume, rfat_file_t *file, uint32_t size)
{
    int status = F_NO_ERROR;
    uint32_t clsno_a, clscnt;

    clscnt = RFAT_SIZE_TO_CLSCNT(size);
    clscnt = ((((clscnt << volume->cls_blk_shift) + (volume->blk_unit_size -1)) / volume->blk_unit_size) * volume->blk_unit_size) >> volume->cls_blk_shift;

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
    status = rfat_volume_dirty(volume);

    if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
    {
	status = rfat_cluster_chain_create_contiguous(volume, clscnt, &clsno_a);

	if (status == F_NO_ERROR)
	{
	    file->flags |= RFAT_FILE_FLAG_CONTIGUOUS;
	    file->size = size;
	    file->first_clsno = clsno_a;
	
	    file->clsno = clsno_a;
	    file->blkno = RFAT_CLSNO_TO_BLKNO(clsno_a);
	    file->blkno_e = file->blkno + volume->cls_blk_size;

	    /* If a new cluster chain gets created, the dir entry needs to be updated to
	     * not lose the cluster chain.
	     */
	    
	    status = rfat_file_sync(volume, file, FALSE, FALSE, file->first_clsno, file->length);
	}
    }

    return status;
}

#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */

static int rfat_file_open(rfat_volume_t *volume, const char *filename, uint32_t mode, uint32_t size, rfat_file_t **p_file)
{
    int status = F_NO_ERROR;
    uint32_t clsno, clsno_d, index, count;
    uint16_t ctime, cdate;
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
    uint32_t clscnt, clsdata;
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
#if (RFAT_CONFIG_MAX_FILES == 1)
    rfat_file_t *file;
#else /* (RFAT_CONFIG_MAX_FILES == 1) */
    rfat_file_t *file, *file_s, *file_e, *file_o;
#endif /* (RFAT_CONFIG_MAX_FILES == 1) */
    rfat_dir_t *dir;

    file = NULL;

#if (RFAT_CONFIG_MAX_FILES == 1)

    if (!volume->file_table[0].mode)
    {
	file = &volume->file_table[0];
    }
#else /* (RFAT_CONFIG_MAX_FILES == 1) */

    file_s = &volume->file_table[0];
    file_e = &volume->file_table[RFAT_CONFIG_MAX_FILES];

    do
    {
	if (!file_s->mode)
        {
            file = file_s;
            break;
        }

	file++;
    }
    while (file_s < file_e);

#endif /* (RFAT_CONFIG_MAX_FILES == 1) */

    if (file)
    {
	file->flags = 0;

	if ((mode & RFAT_FILE_MODE_WRITE) && (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED))
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    status = rfat_path_find_directory(volume, filename, &filename, &clsno_d);
	
	    if (status == F_NO_ERROR)
	    {
		status = rfat_path_convert_filename(volume, filename, NULL);
	    
		if (status == F_NO_ERROR)
		{
		    if (mode & RFAT_FILE_MODE_CREATE)
		    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
			if ((volume->dir.dir_name[0] == '\0') || (volume->dir.dir_nt_reserved & RFAT_DIR_TYPE_LOSSY))
			{
			    count = 1 + volume->dir_entries;
			}
			else
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
			{
			    count = 1;
			}
		    }
		    else
		    {
			count = 0;
		    }

		    status = rfat_path_find_entry(volume, clsno_d, 0, count, rfat_path_find_callback_name, NULL, &clsno, &index, &dir);

		    if (status == F_NO_ERROR)
		    {
			if (dir != NULL)
			{
			    if (dir->dir_attr & RFAT_DIR_ATTR_DIRECTORY)
			    {
				status = F_ERR_INVALIDDIR;
			    }
			    else
			    {
				if ((mode & RFAT_FILE_MODE_WRITE) && (dir->dir_attr & RFAT_DIR_ATTR_READ_ONLY))
				{
				    status = F_ERR_ACCESSDENIED;
				}
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
				else
				{
				    /* Advance to primary dir entry. */
				    index += volume->dir_entries;
				}
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

				file->dir_clsno = clsno;
				file->dir_index = index;
				file->length = RFAT_FTOHL(dir->dir_file_size);
			    
				if (volume->type == RFAT_VOLUME_TYPE_FAT32)
				{
				    file->first_clsno = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				}
				else
				{
				    file->first_clsno = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				}
			    }
			}
			else
			{
			    if (mode & RFAT_FILE_MODE_CREATE)
			    {
#if defined(RFAT_PORT_CORE_TIMEDATE)
				RFAT_PORT_CORE_TIMEDATE(&ctime, &cdate);
#else /* RFAT_PORT_CORE_TIMEDATE */
				ctime = 0;
				cdate = 0;
#endif /* RFAT_PORT_CORE_TIMEDATE */
			    
				status = rfat_path_create_entry(volume, clsno_d, clsno, index, NULL, 0, RFAT_CLSNO_NONE, ctime, cdate);
			    
				if (status == F_NO_ERROR)
				{
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
				    /* Stip out allocations bits, and advance to primary dir entry. */
				    index = (index & 0x0000ffff) + (count -1);
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
				    index = (index & 0x0000ffff);
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

				    file->dir_clsno = clsno;
				    file->dir_index = index;
				    file->length = 0;
				    file->first_clsno = RFAT_CLSNO_NONE;
				}
			    }
			    else
			    {
				status = F_ERR_NOTFOUND;
			    }
			}

			if (status == F_NO_ERROR)
			{

#if (RFAT_CONFIG_MAX_FILES != 1)
			    file_o = NULL;

			    do
			    {
				file_o = rfat_file_enumerate(volume, file_o, clsno, index);

				if (file_o != NULL)
				{
				    if (mode & RFAT_FILE_MODE_WRITE)
				    {
					status = F_ERR_LOCKED;
				    }
				    else
				    {
					if (file_o->mode & RFAT_FILE_MODE_WRITE)
					{
					    status = F_ERR_LOCKED;
					}
				    }
				}
			    }
			    while ((status == F_NO_ERROR) && (file_o != NULL));
#endif /* (RFAT_CONFIG_MAX_FILES != 1) */

			    if (status == F_NO_ERROR)
			    {
				file->status = F_NO_ERROR;
				file->position = 0;
				file->last_clsno = RFAT_CLSNO_NONE;

				if (file->first_clsno == RFAT_CLSNO_NONE)
				{
				    file->flags |= RFAT_FILE_FLAG_END_OF_CHAIN;
				    file->clsno = RFAT_CLSNO_NONE;
				    file->blkno = RFAT_BLKNO_INVALID;
				    file->blkno_e = RFAT_BLKNO_INVALID;
				}
				else
				{
				    file->clsno = file->first_clsno;
				    file->blkno = RFAT_CLSNO_TO_BLKNO(file->clsno);
				    file->blkno_e = file->blkno + volume->cls_blk_size;
				}

				if (mode & RFAT_FILE_MODE_TRUNCATE)
				{
				    file->length = 0;

				    status = rfat_file_shrink(volume, file);
				}

#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
				if (size)
				{
				    if (file->first_clsno == RFAT_CLSNO_NONE)
				    {
					status = rfat_file_reserve(volume, file, size);
				    }
				    else
				    {
					clsno = file->first_clsno;
					clscnt = 0;

					do
					{
					    status = rfat_cluster_read(volume, clsno, &clsdata);
					    
					    if (status == F_NO_ERROR)
					    {
						if ((clsdata >= 2) && (clsdata <= volume->last_clsno))
						{
						    if ((clsno +1) == clsdata)
						    {
							clsno++;
							clscnt++;
						    }
						    else
						    {
							status = F_ERR_EOF;
						    }

						}
						else
						{
						    if (clsdata < RFAT_CLSNO_LAST)
						    {
							status = F_ERR_EOF;
						    }
						}
					    }
					}
					while ((status == F_NO_ERROR) && (clsdata < RFAT_CLSNO_LAST));

					if (status == F_NO_ERROR)
					{
					    if (clscnt == RFAT_SIZE_TO_CLSCNT(size))
					    {
						file->flags |= (RFAT_FILE_FLAG_CONTIGUOUS | RFAT_FILE_FLAG_END_OF_CHAIN);
						file->last_clsno = file->first_clsno + clscnt -1;

						if (size >= (RFAT_FILE_SIZE_MAX & ~volume->cls_mask))
						{
						    file->size = RFAT_FILE_SIZE_MAX;
						}
						else
						{
						    file->size = (size + volume->cls_mask) & ~volume->cls_mask;
						}
					    }
					    else
					    {
						status = F_ERR_EOF;
					    }
					}
				    }
				}
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
			    
				if (status == F_NO_ERROR)
				{
				    if (mode & RFAT_FILE_MODE_APPEND)
				    {
					status = rfat_file_seek(volume, file, file->length);
				    }
				
				    if (status == F_NO_ERROR)
				    {
#if (RFAT_CONFIG_FILE_DATA_CACHE == 1)
					file->data_cache.blkno = RFAT_BLKNO_INVALID;
#endif /* (RFAT_CONFIG_FILE_DATA_CACHE == 1) */

					file->mode = mode;
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }
    else
    {
	status = F_ERR_NOMOREENTRY;
    }

    *p_file = file;

    return status;
}

static int rfat_file_close(rfat_volume_t *volume, rfat_file_t *file)
{
    int status = F_NO_ERROR;

    if (file->mode & RFAT_FILE_MODE_WRITE)
    {
	status = rfat_file_flush(volume, file, TRUE);
    }

    file->mode = 0;
    file->dir_clsno = RFAT_CLSNO_NONE;
    file->dir_index = 0;

    return status;
}

static int rfat_file_read(rfat_volume_t *volume, rfat_file_t *file, uint8_t *data, uint32_t count, uint32_t *p_count)
{
    int status = F_NO_ERROR;
    uint32_t blkno, blkno_e, blkcnt, clsno, position, total, size;
    rfat_cache_entry_t *entry;

    *p_count = 0;

    if (file->position >= file->length)
    {
	count = 0;
    }
    else
    {
	if (count > (file->length - file->position))
	{
	    count = (file->length - file->position);
	}
    }

    if (count != 0)
    {
	total = count;

        position = file->position;
        clsno = file->clsno;
        blkno = file->blkno;
        blkno_e = file->blkno_e;

	/* Take care of the case where there is an empty cluster chain,
	 * but file->length is not 0.
	 */
	if (clsno == RFAT_CLSNO_NONE)
	{
	    status = F_ERR_EOF;
	}
        else
	{
	    if (blkno == blkno_e)
	    {
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
		if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
		{
		    clsno++;
		    blkno_e += volume->cls_blk_size;
		}
		else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
		{
		    status = rfat_cluster_chain_seek(volume, clsno, 1, &clsno);

		    if (status == F_NO_ERROR)
		    {
			blkno = RFAT_CLSNO_TO_BLKNO(clsno);
			blkno_e = blkno + volume->cls_blk_size;
		    }
		}
	    }
	}

        if (status == F_NO_ERROR)
        {
            if (((position & RFAT_BLK_MASK) + count) < RFAT_BLK_SIZE)
            {
                status = rfat_data_cache_read(volume, file, blkno, &entry);
	    
                if (status == F_NO_ERROR)
                {
                    memcpy(data, entry->data + (position & RFAT_BLK_MASK), count);

                    position += count;
                    count = 0;

                    if (!(position & RFAT_BLK_MASK))
                    {
                        blkno++;
                    }
                }
            }
            else
            {
                if (position & RFAT_BLK_MASK)
                {
                    status = rfat_data_cache_read(volume, file, blkno, &entry);
	    
                    if (status == F_NO_ERROR)
                    {
                        size = (RFAT_BLK_SIZE - (position & RFAT_BLK_MASK));

                        memcpy(data, entry->data + (RFAT_BLK_SIZE - size), size);

                        position += size;
                        data += size;
                        count -= size;

                        blkno++;
                    }
                }

                while ((status == F_NO_ERROR) && (count != 0))
                {
                    if (blkno == blkno_e)
                    {
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
			if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
			{
			    clsno++;
			    blkno_e += volume->cls_blk_size;
			}
			else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
			{
			    status = rfat_cluster_chain_seek(volume, clsno, 1, &clsno);
			    
			    if (status == F_NO_ERROR)
			    {
				blkno = RFAT_CLSNO_TO_BLKNO(clsno);
				blkno_e = blkno + volume->cls_blk_size;
			    }
			}
		    }
                
                    if (status == F_NO_ERROR)
                    {
                        if (count < RFAT_BLK_SIZE)
                        {
                            status = rfat_data_cache_read(volume, file, blkno, &entry);
                    
                            if (status == F_NO_ERROR)
                            {
                                memcpy(data, entry->data, count);

                                position += count;
                                data += count;
                                count = 0;
                            }
                        }
                        else
                        {
			    size = volume->cls_size - (position & volume->cls_mask);
                            
                            if (size > count)
                            {
                                size = count & ~RFAT_BLK_MASK;
                            }

                            blkcnt = size >> RFAT_BLK_SHIFT;

                            status = rfat_data_cache_flush(volume, file);

                            if (status == F_NO_ERROR)
                            {
                                status = rfat_disk_read_sequential(volume->disk, blkno, blkcnt, data);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
				if (status == F_ERR_INVALIDSECTOR)
				{
				    volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
				}
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

                                if (status == F_NO_ERROR)
                                {
                                    position += size;
                                    data += size;
                                    count -= size;

                                    blkno += blkcnt;
                                }

				/* After any data related disk operation, "file->status" needs
				 * to be checked asynchronously for a previous error.
				 */
				if (file->status != F_NO_ERROR)
				{
				    status = file->status;
				}
                            }
                        }
                    }
                }
            }
        
            if (status == F_NO_ERROR)
            {
                file->position = position;
                file->clsno = clsno;
                file->blkno = blkno;
                file->blkno_e = blkno_e;
            }
        }

	*p_count = total - count;
    }

    if (file->status == F_NO_ERROR)
    {
	file->status = status;
    }

    return status;
}

static int rfat_file_write(rfat_volume_t *volume, rfat_file_t *file, const uint8_t *data, uint32_t count, uint32_t *p_count)
{
    int status = F_NO_ERROR;
    uint32_t blkno, blkno_e, blkcnt, clsno, offset, position, length, total, size;
    rfat_cache_entry_t *entry;

    *p_count = 0;

    if ((file->mode & RFAT_FILE_MODE_APPEND) && (file->position != file->length))
    {
	status = rfat_file_seek(volume, file, file->length);
    }

    if (status == F_NO_ERROR)
    {
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
	if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
	{
	    if (file->size < file->position)
	    {
		status = F_ERR_EOF;
	    }
	    else
	    {
		if (count > (file->size - file->position))
		{
		    count = (file->size - file->position);
		}
	    }
	}
	else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
	{
	    if (count > (RFAT_FILE_SIZE_MAX - file->position))
	    {
		count = (RFAT_FILE_SIZE_MAX - file->position);
	    }
	}

	if (status == F_NO_ERROR)
	{
	    if (count != 0)
	    {
		file->flags |= RFAT_FILE_FLAG_DATA_MODIFIED;

		total  = count;
		offset = (file->length + RFAT_BLK_MASK) & ~RFAT_BLK_MASK;
		length = file->position + count;

		if (length > file->length)
		{
		    if ((file->length == 0) || (file->length != file->position) || (((file->length -1) & ~volume->cls_mask) != ((length -1) & ~volume->cls_mask)))
		    {
			status = rfat_file_extend(volume, file, length);
		    }
		    else
		    {
			file->length = length;
			file->flags |= RFAT_FILE_FLAG_DIR_MODIFIED;
		    }
		}

		if (status == F_NO_ERROR)
		{
		    position = file->position;
		    clsno = file->clsno;
		    blkno = file->blkno;
		    blkno_e = file->blkno_e;

		    /* Take care of the case where the cluster chain of a file was truncted,
		     * but the length was not adjusted on disk.
		     */
		    if (clsno == RFAT_CLSNO_NONE)
		    {
			status = F_ERR_EOF;
		    }
		    else
		    {
			if (blkno == blkno_e)
			{
			    status = rfat_data_cache_flush(volume, file);

			    if (status == F_NO_ERROR)
			    {
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
				if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
				{
				    clsno++;
				    blkno_e += volume->cls_blk_size;
				}
				else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
				{
				    status = rfat_cluster_chain_seek(volume, clsno, 1, &clsno);
			
				    if (status == F_NO_ERROR)
				    {
					blkno = RFAT_CLSNO_TO_BLKNO(clsno);
					blkno_e = blkno + volume->cls_blk_size;
				    }
				}
			    }
			}
		    }

		    if (status == F_NO_ERROR)
		    {
			if (((position & RFAT_BLK_MASK) + count) < RFAT_BLK_SIZE)
			{
			    /* All data is within one block, simply go throu the cache.
			     * If the block is beyond the old file->length, is has to 
			     * be all zeros. Otherwise got throu the normal cache.
			     */

			    if (position >= offset)
			    {
				status = rfat_data_cache_zero(volume, file, blkno, &entry);
			    }
			    else
			    {
				status = rfat_data_cache_read(volume, file, blkno, &entry);
			    }
	    
			    if (status == F_NO_ERROR)
			    {
				memcpy(entry->data + (position & RFAT_BLK_MASK), data, count);

				rfat_data_cache_modify(volume, file);

				position += count;
				count = 0;

				if (!(position & RFAT_BLK_MASK))
				{
				    blkno++;
				}
			    }
			}
			else
			{
			    if (position & RFAT_BLK_MASK)
			    {
				size = (RFAT_BLK_SIZE - (position & RFAT_BLK_MASK));

				if (position >= offset)
				{
				    status = rfat_data_cache_zero(volume, file, blkno, &entry);
				}
				else
				{ 
				    status = rfat_data_cache_read(volume, file, blkno, &entry);
				}

				if (status == F_NO_ERROR)
				{
				    memcpy(entry->data + (RFAT_BLK_SIZE - size), data, size);
				
				    rfat_data_cache_modify(volume, file);

				    if (size < count)
				    {
					status = rfat_data_cache_write(volume, file);
				    }

				    if (status == F_NO_ERROR)
				    {
					position += size;
					data += size;
					count -= size;
                                    
					blkno++;
				    }
				}
			    }

			    while ((status == F_NO_ERROR) && (count != 0))
			    {
				if (blkno == blkno_e)
				{
				    status = rfat_data_cache_flush(volume, file);

				    if (status == F_NO_ERROR)
				    {
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
					if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
					{
					    clsno++;
					    blkno_e += volume->cls_blk_size;
					}
					else
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
					{
					    status = rfat_cluster_chain_seek(volume, clsno, 1, &clsno);
					
					    if (status == F_NO_ERROR)
					    {
						blkno = RFAT_CLSNO_TO_BLKNO(clsno);
						blkno_e = blkno + volume->cls_blk_size;
					    }
					}
				    }
				}

				if (status == F_NO_ERROR)
				{
				    if (count < RFAT_BLK_SIZE)
				    {
					if (position >= offset)
					{
					    status = rfat_data_cache_zero(volume, file, blkno, &entry);
					}
					else
					{
					    status = rfat_data_cache_read(volume, file, blkno, &entry);
					}

					if (status == F_NO_ERROR)
					{
					    memcpy(entry->data, data, count);

					    rfat_data_cache_modify(volume, file);

					    if (status == F_NO_ERROR)
					    {
						position += count;
						data += count;
						count = 0;
					    }
					}
				    }
				    else
				    {
					size = volume->cls_size - (position & volume->cls_mask);
			    
					if (size > count)
					{
					    size = count & ~RFAT_BLK_MASK;
					}

					blkcnt = size >> RFAT_BLK_SHIFT;

					status = rfat_data_cache_invalidate(volume, file, blkno, blkcnt);

					if (status == F_NO_ERROR)
					{
					    status = rfat_disk_write_sequential(volume->disk, blkno, blkcnt, data, &file->status);

#if (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1)
					    if (status == F_ERR_INVALIDSECTOR)
					    {
						volume->flags |= RFAT_VOLUME_FLAG_MEDIA_FAILURE;
					    }
#endif /* (RFAT_CONFIG_MEDIA_FAILURE_SUPPORTED == 1) */

					    if (status == F_NO_ERROR)
					    {
						position += size;
						data += size;
						count -= size;
                                            
						blkno += blkcnt;
					    }

					    /* After any data related disk operation, "file->status" needs
					     * to be checked asynchronously for a previous error.
					     */
					    if (file->status != F_NO_ERROR)
					    {
						status = file->status;
					    }
					}
				    }
				}
			    }
			}
		    }

		    if (status == F_NO_ERROR)
		    {
			if (file->mode & RFAT_FILE_MODE_COMMIT)
			{
			    status = rfat_disk_sync(volume->disk, &file->status);
			}

			if (status == F_NO_ERROR)
			{
			    file->position = position;
			    file->clsno = clsno;
			    file->blkno = blkno;
			    file->blkno_e = blkno_e;
			}
		    }
		}

		*p_count = total - count;
	    }
	}
    }

    if (file->status == F_NO_ERROR)
    {
	file->status = status;
    }

    return status;
}


/***********************************************************************************************************************/

const char * f_getversion(void)
{
    return RFAT_VERSION_STRING;
}

int f_initvolume(void)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();

    status = rfat_volume_lock_noinit(volume);
    
    if (status == F_NO_ERROR)
    {
        status = rfat_volume_init(volume);
        
	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_delvolume(void)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();

    status = rfat_volume_lock_nomount(volume);
    
    if (status == F_NO_ERROR)
    {
	status = rfat_volume_unmount(volume);

	if (status == F_NO_ERROR)
	{
	    status = rfat_disk_release(volume->disk);

	    if (status == F_NO_ERROR)
	    {
		volume->state = RFAT_VOLUME_STATE_INITIALIZED;
		volume->disk = NULL;
	    }
	}
        
	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_format(int fattype)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    status = rfat_volume_unmount(volume);

	    if (status == F_NO_ERROR)
	    {
		status = rfat_volume_erase(volume);
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}


#if !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD)

int f_hardformat(int fattype)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();

    status = rfat_volume_lock_nomount(volume);
    
    if (status == F_NO_ERROR)
    {
	status = rfat_volume_unmount(volume);

	if (status == F_NO_ERROR)
	{
	    status = rfat_volume_format(volume);
        }
        
	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

#endif /* !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD) */


int f_getfreespace(F_SPACE *pspace)
{
    int status = F_NO_ERROR;
    uint32_t clsno, clsno_e, clscnt_total, clscnt_free, clsdata;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	if (volume->flags & RFAT_VOLUME_FLAG_FSINFO_VALID)
	{
	    clscnt_total = volume->last_clsno - 1;
	    clscnt_free = volume->free_clscnt;
	}
	else
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
	{
	    clscnt_total = volume->last_clsno - 1;
	    clscnt_free = 0;

	    for (clsno = 2, clsno_e = volume->last_clsno; ((status == F_NO_ERROR) && (clsno <= clsno_e)); clsno++)
	    {
		/* Bypass cluster cache on read while scanning.
		 */
		status = rfat_cluster_read_uncached(volume, clsno, &clsdata);
		
		if (status == F_NO_ERROR)
		{
		    if (clsdata == RFAT_CLSNO_FREE)
		    {
			clscnt_free++;
		    }
		}
	    }

#if (RFAT_CONFIG_FSINFO_SUPPORTED == 1)
	    if (status == F_NO_ERROR)
	    {
		volume->flags |= (RFAT_VOLUME_FLAG_FSINFO_VALID | RFAT_VOLUME_FLAG_FSINFO_DIRTY);
		volume->free_clscnt = clscnt_free;

		if ((volume->state == RFAT_VOLUME_STATE_MOUNTED) &&
		    (volume->fsinfo_blkofs != 0))
		{
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
		    /* If a map/fat entry has changed, this triggers a record/commit sequence.
		     * Otherwise the RFAT_VOLUME_FLAG_FSINFO_DIRTY setting will simply update
		     * the FSINFO on disk.
		     */
		    status = rfat_volume_record(volume);

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1)
		    /* volume->free_clscnt is only maintained in FSINFO if RFAT_VOLUME_FLAG_MOUNTED_DIRTY
		     * is not set. If RFAT_VOLUME_FLAG_VOLUME_DIRTY is set, then the volume is in a dirty
		     * state, where volume->free_clscnt cannot be updated. In this case wait for the update
		     * till the next rfat_volume_clean() gets called.
		     */
		    if (!(volume->flags & (RFAT_VOLUME_FLAG_VOLUME_DIRTY | RFAT_VOLUME_FLAG_MOUNTED_DIRTY)))
		    {
			status = rfat_fat_cache_flush(volume);
		    
			if (status == F_NO_ERROR)
			{
			    status = rfat_volume_fsinfo(volume, volume->free_clscnt, volume->next_clsno);
			    
			    if (status == F_NO_ERROR)
			    {
				volume->flags &= ~RFAT_VOLUME_FLAG_FSINFO_DIRTY;
			    }
			}
		    }
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) */
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
		}
	    }
#endif /* (RFAT_CONFIG_FSINFO_SUPPORTED == 1) */
	}

        if (status == F_NO_ERROR)
        {
            pspace->total      = clscnt_total << volume->cls_shift;
            pspace->free       = clscnt_free  << volume->cls_shift;
            pspace->used       = (clscnt_total - clscnt_free) << volume->cls_shift;
            pspace->bad        = 0;
            pspace->total_high = clscnt_total >> (32 - volume->cls_shift);
            pspace->free_high  = clscnt_free  >> (32 - volume->cls_shift);
            pspace->used_high  = (clscnt_total - clscnt_free) >> (32 - volume->cls_shift);
            pspace->bad_high   = 0;
        }

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_getserial(unsigned long *p_serial)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();
    
    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	*p_serial = volume->serial;

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}


#if !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD)

int f_setlabel(const char *volname)
{
    int status = F_NO_ERROR;
    uint32_t clsno, index;
    uint16_t ctime, cdate;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();
    
    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    volname = rfat_name_cstring_to_label(volname, volume->dir.dir_name);

	    if (volname != NULL)
	    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
		volume->dir.dir_nt_reserved = 0x00;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

		status = rfat_path_find_entry(volume, RFAT_CLSNO_NONE, 0, 1, rfat_path_find_callback_volume, NULL, &clsno, &index, &dir);

		if (status == F_NO_ERROR)
		{
#if defined(RFAT_PORT_CORE_TIMEDATE)
		    RFAT_PORT_CORE_TIMEDATE(&ctime, &cdate);
#else /* RFAT_PORT_CORE_TIMEDATE */
		    ctime = 0;
		    cdate = 0;
#endif /* RFAT_PORT_CORE_TIMEDATE */

		    if (dir == NULL)
		    {
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
			status = rfat_volume_dirty(volume);

			if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
			{
			    status = rfat_path_create_entry(volume, RFAT_CLSNO_NONE, clsno, index, (const char*)volume->dir.dir_name, RFAT_DIR_ATTR_VOLUME_ID, RFAT_CLSNO_NONE, ctime, cdate);
			
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
			    status = rfat_volume_clean(volume, status);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
			}
		    }
		    else
		    {
			memcpy(dir->dir_name, volume->dir.dir_name, sizeof(dir->dir_name));

			dir->dir_wrt_time = RFAT_HTOFS(ctime);
			dir->dir_wrt_date = RFAT_HTOFS(cdate);

			status = rfat_dir_cache_write(volume);
		    }
		}
	    }
	    else
	    {
		status = F_ERR_INVALIDNAME;
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_getlabel(char *volname, int length)
{
    int status = F_NO_ERROR;
    const uint8_t *dosname;
    char *volname_e;
    rfat_dir_t *dir;
    rfat_boot_t *boot;
    rfat_volume_t *volume;

    volume = RFAT_DEFAULT_VOLUME();
    
    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	dosname = NULL;

	status = rfat_path_find_entry(volume, RFAT_CLSNO_NONE, 0, 0, rfat_path_find_callback_volume, NULL, NULL, NULL, &dir);

	if (status == F_NO_ERROR)
	{
	    volname_e = volname + length;

	    if (dir == NULL)
	    {
#if (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) || (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
		boot = (rfat_boot_t*)((void*)&volume->bs_data[0]);
#else /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) || (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */

		boot = (rfat_boot_t*)((void*)volume->dir_cache.data);

		status = rfat_dir_cache_flush(volume);
		
		if (status == F_NO_ERROR)
		{
		    status = rfat_volume_read(volume, volume->boot_blkno, (uint8_t*)boot);
		}

		if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_VOLUME_DIRTY_SUPPORTED == 1) || (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
		{
		    if (volume->type != RFAT_VOLUME_TYPE_FAT32)
		    {
			if (boot->bpb40.bs_boot_sig == 0x29)
			{
			    dosname = (const uint8_t*)boot->bpb40.bs_vol_lab;
			}
		    }
		    else
		    {
			if (boot->bpb71.bs_boot_sig == 0x29)
			{
			    dosname = (const uint8_t*)boot->bpb71.bs_vol_lab;
			}
		    }
		}
	    }
	    else
	    {
		dosname = (const uint8_t*)dir->dir_name;
	    }
	}

	if (status == F_NO_ERROR)
	{
	    if (dosname != NULL)
	    {
		volname = rfat_name_label_to_cstring(dosname, volname, volname_e);

		if (volname == NULL)
		{
		    status = F_ERR_TOOLONGNAME;
		}
	    }
	    else
	    {
		status = F_ERR_NOTFOUND;
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

#endif /* !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD) */

int f_mkdir(const char *dirname)
{
    int status = F_NO_ERROR;
    uint16_t ctime, cdate;
    uint32_t clsno, clsno_d, clsno_s, blkno_s, index, count;
    rfat_dir_t *dir;
    rfat_cache_entry_t *entry;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(dirname);
    
    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    if (((dirname[0] == '/') || (dirname[0] == '\\')) && (dirname[1] == '\0'))
	    {
		status = F_ERR_INVALIDDIR;
	    }
	    else
	    {
		status = rfat_path_find_directory(volume, dirname, &dirname, &clsno_d);
            
		if (status == F_NO_ERROR)
		{
		    status = rfat_path_convert_filename(volume, dirname, NULL);

		    if (status == F_NO_ERROR)
		    {
			if (volume->dir.dir_name[0] == '.')
			{
			    status = F_ERR_NOTFOUND;
			}
			else
			{
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
			    if ((volume->dir.dir_name[0] == '\0') || (volume->dir.dir_nt_reserved & RFAT_DIR_TYPE_LOSSY))
			    {
				count = 1 + volume->dir_entries;
			    }
			    else
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
			    {
				count = 1;
			    }

			    status = rfat_path_find_entry(volume, clsno_d, 0, count, rfat_path_find_callback_name, NULL, &clsno, &index, &dir);
			
			    if (status == F_NO_ERROR)
			    {
				if (dir == NULL)
				{
				    /* No conflicting entry found, hence start out creating the subdirectory.
				     *
				     * The strategy is to first allocate the cluster for the subdirectory, populate it
				     * and then create the new directory entry in the parent. This way there is no
				     * inconsistent file system state along the way.
				     */

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
				    status = rfat_volume_dirty(volume);

				    if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
				    {
					status = rfat_cluster_chain_create(volume, RFAT_CLSNO_NONE, 1, &clsno_s, NULL);
				
					if (status == F_NO_ERROR)
					{
					    blkno_s = RFAT_CLSNO_TO_BLKNO(clsno_s);
				    
					    status = rfat_volume_zero(volume, blkno_s, volume->cls_blk_size, NULL);

					    if (status == F_NO_ERROR)
					    {
						status = rfat_dir_cache_zero(volume, blkno_s, &entry);

						if (status == F_NO_ERROR)
						{
#if defined(RFAT_PORT_CORE_TIMEDATE)
						    RFAT_PORT_CORE_TIMEDATE(&ctime, &cdate);
#else /* RFAT_PORT_CORE_TIMEDATE */
						    ctime = 0;
						    cdate = 0;
#endif /* RFAT_PORT_CORE_TIMEDATE */
						
						    rfat_path_setup_entry(volume, rfat_dirname_dot, RFAT_DIR_ATTR_DIRECTORY, clsno_s, ctime, cdate, ((rfat_dir_t*)((void*)entry->data)));
						    rfat_path_setup_entry(volume, rfat_dirname_dotdot, RFAT_DIR_ATTR_DIRECTORY, clsno_d, ctime, cdate, ((rfat_dir_t*)((void*)(entry->data + sizeof(rfat_dir_t)))));
						
						    status = rfat_dir_cache_write(volume);
						
						    if (status == F_NO_ERROR)
						    {
							/* Subdirectory has been created, so back to hooking up the directory entry.
							 */
						    
							status = rfat_path_create_entry(volume, clsno_d, clsno, index, NULL, RFAT_DIR_ATTR_DIRECTORY, clsno_s, ctime, cdate);

							if (status == F_ERR_NOMOREENTRY)
							{
							    rfat_cluster_chain_destroy(volume, clsno_s, RFAT_CLSNO_FREE);
							}
						    }
						}
					    }
					}

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
					status = rfat_volume_clean(volume, status);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
				    }
				}
				else
				{
				    status = F_ERR_DUPLICATED;
				}
			    }
			}
		    }
		}
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_rmdir(const char *dirname)
{
    int status = F_NO_ERROR;
    uint32_t clsno, clsno_d, index, first_clsno;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    unsigned int entries;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(dirname);
    
    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    if (((dirname[0] == '/') || (dirname[0] == '\\')) && (dirname[1] == '\0'))
	    {
		status = F_ERR_INVALIDDIR;
	    }
	    else
	    {
		status = rfat_path_find_directory(volume, dirname, &dirname, &clsno_d);
            
		if (status == F_NO_ERROR)
		{
		    status = rfat_path_convert_filename(volume, dirname, NULL);

		    if (status == F_NO_ERROR)
		    {
			if (volume->dir.dir_name[0] == '.')
			{
			    status = F_ERR_NOTFOUND;
			}
			else
			{
			    // ### FIXME ... filter out "/"
			    status = rfat_path_find_entry(volume, clsno_d, 0, 0, rfat_path_find_callback_name, NULL, &clsno, &index, &dir);
			
			    if (status == F_NO_ERROR)
			    {
				if (dir != NULL)
				{
				    if (dir->dir_attr & RFAT_DIR_ATTR_DIRECTORY)
				    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
					/* rfat_path_find_callback_name() fills in the proper volume->dir_entries on a match.
					 */
					entries = volume->dir_entries;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

					if (volume->type == RFAT_VOLUME_TYPE_FAT32)
					{
					    first_clsno = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
					}
					else
					{ 
					    first_clsno = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
					}

					/* Skip "." and ".."
					 */
					status = rfat_path_find_entry(volume, first_clsno, 2, 0, rfat_path_find_callback_empty, NULL, NULL, NULL, &dir);

					if (status == F_NO_ERROR)
					{
					    if (dir == NULL)
					    {
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
						status = rfat_volume_dirty(volume);

						if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
						{
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
						    status = rfat_path_destroy_entry(volume, clsno, index, entries, first_clsno);
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
						    status = rfat_path_destroy_entry(volume, clsno, index, first_clsno);
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
					    
						    if (status == F_NO_ERROR)
						    {
							if (volume->cwd_clsno == first_clsno)
							{
							    volume->cwd_clsno = RFAT_CLSNO_END_OF_CHAIN;
							}
						    }

					    
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
						    status = rfat_volume_clean(volume, status);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
						}
					    }
					    else
					    {
						status = F_ERR_NOTEMPTY;
					    }
					}
				    }
				    else
				    {
					status = F_ERR_INVALIDDIR;
				    }
				}
				else
				{
				    status = F_ERR_NOTFOUND;
				}
			    }
			}
		    }
		}
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_chdir(const char *dirname)
{
    int status = F_NO_ERROR;
    uint32_t clsno_d, blkno_d;
    rfat_cache_entry_t *entry;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(dirname);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
        if (((dirname[0] == '/') || (dirname[0] == '\\')) && (dirname[1] == '\0'))
        {
            /* rfat_path_find_directory cannot return a vaild "dir" for the "/",
             * so special code this here.
             */
            volume->cwd_clsno = RFAT_CLSNO_NONE;
        }
        else
        {
            status = rfat_path_find_directory(volume, dirname, &dirname, &clsno_d);
            
            if (status == F_NO_ERROR)
            {
		status = rfat_path_convert_filename(volume, dirname, NULL);
		
		if (status == F_NO_ERROR)
		{
		    if (volume->dir.dir_name[0] == '.')
		    {
			if (volume->dir.dir_name[1] == ' ')
			{
			    /* "." */
			    volume->cwd_clsno = clsno_d;
			}
			else
			{
			    /* ".." */

			    if (clsno_d != RFAT_CLSNO_NONE)
			    {
				blkno_d = RFAT_CLSNO_TO_BLKNO(clsno_d);

				status = rfat_dir_cache_read(volume, blkno_d, &entry);
                                                
				if (status == F_NO_ERROR)
				{
				    dir = (rfat_dir_t*)((void*)(entry->data + sizeof(rfat_dir_t)));
                                
				    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
				    {
					volume->cwd_clsno = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				    }
				    else
				    {
					volume->cwd_clsno = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				    }
				}
			    }
			    else
			    {
				status = F_ERR_NOTFOUND;
			    }
			}
		    }
		    else
		    {
			status = rfat_path_find_entry(volume, clsno_d, 0, 0, rfat_path_find_callback_name, NULL, NULL, NULL, &dir);

			if (status == F_NO_ERROR)
			{
			    if (dir != NULL)
			    {
				if (dir->dir_attr & RFAT_DIR_ATTR_DIRECTORY)
				{
				    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
				    {
					volume->cwd_clsno = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				    }
				    else
				    {
					volume->cwd_clsno = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				    }
				}
				else
				{
				    status = F_ERR_INVALIDDIR;
				}
			    }
			    else
			    {
				status = F_ERR_NOTFOUND;
			    }
			}
		    }
		}
	    }
        }

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_getcwd(char *dirname, int length)
{
    int status = F_NO_ERROR;
    uint32_t clsno_s, clsno_p, clsno_g, blkno, index;
    char *dirname_e;
    rfat_cache_entry_t *entry;
    rfat_dir_t *dir;
    rfat_volume_t *volume;
    
    volume = RFAT_DEFAULT_VOLUME();

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (length == 1)
	{
            status = F_ERR_TOOLONGNAME;
	}
	else
	{
	    clsno_s = volume->cwd_clsno;
	
	    index = length -1;
	    
	    if (clsno_s != RFAT_CLSNO_NONE)
	    {
		blkno = volume->cls_blk_offset + (clsno_s << volume->cls_blk_shift);
		    
		status = rfat_dir_cache_read(volume, blkno, &entry);

		if (status == F_NO_ERROR)
		{
		    dir = (rfat_dir_t*)((void*)(entry->data + sizeof(rfat_dir_t)));
		    
		    if (volume->type == RFAT_VOLUME_TYPE_FAT32)
		    {
			clsno_p = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
		    }
		    else
		    {
			clsno_p = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
		    }
		    
		    do
		    {
			if (clsno_p != RFAT_CLSNO_NONE)
			{
			    blkno = volume->cls_blk_offset + (clsno_p << volume->cls_blk_shift);
			    
			    status = rfat_dir_cache_read(volume, blkno, &entry);
			    
			    if (status == F_NO_ERROR)
			    {
				dir = (rfat_dir_t*)((void*)(entry->data + sizeof(rfat_dir_t)));
				
				if (volume->type == RFAT_VOLUME_TYPE_FAT32)
				{
				    clsno_g = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				}
				else
				{
				    clsno_g = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
				}
			    }
			}
			else
			{
			    clsno_g = RFAT_CLSNO_NONE;
			}

			if (status == F_NO_ERROR)
			{
			    status = rfat_path_find_entry(volume, clsno_p, 0, 0, rfat_path_find_callback_directory, (void*)clsno_s, NULL, NULL, &dir);

			    if (status == F_NO_ERROR)
			    {
				if (dir != NULL)
				{
				    if (clsno_s != volume->cwd_clsno)
				    {
					if (index != 0)
					{
					    dirname[index--] = F_SEPARATORCHAR;
					}
					else
					{
					    status = F_ERR_TOOLONGNAME;
					}
				    }
				    
				    if (status == F_NO_ERROR)
				    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
					/* It's possible to have a sfn_name only ("." and ".." for example).
					 */
					if (volume->lfn_count)
					{
					    dirname_e = rfat_name_uniname_to_cstring(volume->lfn_name, volume->lfn_count, dirname, dirname + index);
					}
					else
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
					{
					    dirname_e = rfat_name_dosname_to_cstring(dir->dir_name, dir->dir_nt_reserved, dirname, dirname + index);
					}
					
					if (dirname_e != NULL)
					{
					    index -= (dirname_e - dirname -1);
					    
					    memcpy(&dirname[index+1], &dirname[0], (dirname_e - dirname -1));
					    
					    clsno_s = clsno_p;
					    clsno_p = clsno_g;
					}
					else
					{
					    status = F_ERR_TOOLONGNAME;
					}
				    }
				}
				else
				{
				    status = F_ERR_NOTFOUND;
				}
			    }
			}
		    }
		    while ((status == F_NO_ERROR) && (clsno_s != RFAT_CLSNO_NONE));
		}
	    }
		
	    if (status == F_NO_ERROR)
	    {
		if (index >= 2)
		{
		    dirname[index] = F_SEPARATORCHAR;
		    
		    memcpy(&dirname[0], &dirname[index], (length - index));
		    
		    dirname[length - index] = '\0';
		}
		else
		{
		    status = F_ERR_TOOLONGNAME;
		}
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }
	
    return status;
}


#if !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD)

int f_rename(const char *filename, const char *newname)
{
    int status = F_NO_ERROR;
    uint32_t blkno, clsno_d, clsno_o, index_o;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
    uint32_t clsno, index, entries_o, count;
    uint8_t nt_reserved_o;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
    rfat_dir_t *dir;
    rfat_cache_entry_t *entry;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    status = rfat_path_find_directory(volume, filename, &filename, &clsno_d);
            
	    if (status == F_NO_ERROR)
	    {
		status = rfat_path_convert_filename(volume, filename, NULL);
	    
		if (status == F_NO_ERROR)
		{
		    status = rfat_path_find_entry(volume, clsno_d, 0, 0, rfat_path_find_callback_name, NULL, &clsno_o, &index_o, &dir);

		    if (status == F_NO_ERROR)
		    {
			if (dir != NULL)
			{
			    if (dir->dir_attr & RFAT_DIR_ATTR_READ_ONLY)
			    {
				status = F_ERR_ACCESSDENIED;
			    }
			    else
			    {
				if (rfat_file_enumerate(volume, NULL, clsno_o, index_o))
				{
				    status = F_ERR_LOCKED;
				}
				else
				{
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
				    memcpy(&volume->dir, dir, sizeof(rfat_dir_t));

				    nt_reserved_o = dir->dir_nt_reserved;

				    /* rfat_path_find_callback_name() fills in the proper volume->dir_entries on a match.
				     */
				    entries_o = volume->dir_entries;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

				    status = rfat_path_convert_filename(volume, newname, NULL);
				
				    if (status == F_NO_ERROR)
				    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
					if ((volume->dir.dir_name[0] == '\0') || (volume->dir.dir_nt_reserved & RFAT_DIR_TYPE_LOSSY))
					{
					    count = 1 + volume->dir_entries;
					}
					else
					{
					    count = 1;
					}

					status = rfat_path_find_entry(volume, clsno_d, 0, count, rfat_path_find_callback_name, NULL, &clsno, &index, &dir);
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
					status = rfat_path_find_entry(volume, clsno_d, 0, 0, rfat_path_find_callback_name, NULL, NULL, NULL, &dir);
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
				    
					if (status == F_NO_ERROR)
					{
					    if (dir != NULL)
					    {
						status = F_ERR_DUPLICATED;
					    }
					    else
					    {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
						if ((entries_o != 0) || (count != 1))
						{
#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1)
						    /* For TRANSACTION_SAFE, the RFAT_DIR_FLAG_DESTROY_ENTRY operation is recorded first, and
						     * the rfat_path_create_entry() gets called to add the RFAT_DIR_FLAG_CREATE_ENTRY part.
						     * If that fails somehow, then the pending operations are canceled.
						     */
						    volume->dir_flags |= RFAT_DIR_FLAG_DESTROY_ENTRY;
						    volume->del_clsno = clsno_o;
						    volume->del_index = index_o;
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
						    volume->del_entries = entries_o;
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

						    /* The call to rfat_path_create_entry() is special. The RFAT_CLUSTER_END_OF_CHAIN tells it
						     * use the existing volume->dir as a template, and to overlay "attr" onto dir_nt_reserved.
						     */
						    status = rfat_path_create_entry(volume, clsno_d, clsno, index, NULL, nt_reserved_o, RFAT_CLSNO_END_OF_CHAIN, 0, 0);
						
						    if (status != F_NO_ERROR)
						    {
							volume->dir_flags &= ~(RFAT_DIR_FLAG_DESTROY_ENTRY | RFAT_DIR_FLAG_CREATE_ENTRY);
						    }

#else /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
						    status = rfat_volume_dirty(volume);

						    if (status == F_NO_ERROR)
						    {
							/* The call to rfat_path_create_entry() is special. The RFAT_CLUSTER_END_OF_CHAIN tells it
							 * use the existing volume->dir as a template, and to overlay "attr" onto dir_nt_reserved.
							 */

							status = rfat_path_create_entry(volume, clsno_d, clsno, index, NULL, nt_reserved_o, RFAT_CLSNO_END_OF_CHAIN, 0, 0);
						    
							if (status == F_NO_ERROR)
							{
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
							    status = rfat_path_destroy_entry(volume, clsno_o, index_o, entries_o, RFAT_CLSNO_NONE);
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
							    status = rfat_path_destroy_entry(volume, clsno_o, index_o, RFAT_CLUSTER_NONE);
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
							}

							status = rfat_volume_clean(volume, status);
						    }
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 1) */
						}
						else
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
						{
						    /* Nothing special to do here for TRANSACTION_SAFE/VOLUME_DIRTY/FSINFO. The dir entry
						     * update is atomic and not dependent upon the FAT.
						     */

						    if (clsno_o == RFAT_CLSNO_NONE)
						    {
							blkno = volume->root_blkno + RFAT_INDEX_TO_BLKCNT_ROOT(index_o);
						    }
						    else
						    {
							blkno = RFAT_CLSNO_TO_BLKNO(clsno_o) + RFAT_INDEX_TO_BLKCNT(index_o);
						    }
						
						    status = rfat_dir_cache_read(volume, blkno, &entry);
						
						    if (status == F_NO_ERROR)
						    {
							dir = (rfat_dir_t*)((void*)(entry->data + RFAT_INDEX_TO_BLKOFS(index_o)));
						    
							memcpy(dir->dir_name, volume->dir.dir_name, 11);
						    
							dir->dir_nt_reserved = volume->dir.dir_nt_reserved;
						    
							status = rfat_dir_cache_write(volume);
						    }
						}
					    }
					}
				    }
				}
			    }
			}
			else
			{
			    status = F_ERR_NOTFOUND;
			}
		    }
		}
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

#endif /* !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD) */

int f_delete(const char *filename)
{
    int status = F_NO_ERROR;
    uint32_t clsno, index, first_clsno;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    status = rfat_path_find_file(volume, filename, &clsno, &index, &dir);
            
	    if (status == F_NO_ERROR)
	    {
		if (dir->dir_attr & RFAT_DIR_ATTR_DIRECTORY)
		{
		    status = F_ERR_NOTFOUND;
		}
		else if (dir->dir_attr & RFAT_DIR_ATTR_READ_ONLY)
		{
		    status = F_ERR_ACCESSDENIED;
		}
		else
		{
		    if (rfat_file_enumerate(volume, NULL, clsno, index))
		    {
			status = F_ERR_LOCKED;
		    }
		    else
		    {
			if (volume->type == RFAT_VOLUME_TYPE_FAT32)
			{
			    first_clsno = ((uint32_t)RFAT_FTOHS(dir->dir_clsno_hi) << 16) | (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
			}
			else
			{ 
			    first_clsno = (uint32_t)RFAT_FTOHS(dir->dir_clsno_lo);
			}

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
			status = rfat_volume_dirty(volume);
		    
			if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
			{
#if (RFAT_CONFIG_VFAT_SUPPORTED == 1)
			    /* rfat_path_find_file() fills in the proper volume->dir_entries on a match.
			     */
			    status = rfat_path_destroy_entry(volume, clsno, index, volume->dir_entries, first_clsno);
#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */
			    status = rfat_path_destroy_entry(volume, clsno, index, first_clsno);
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 1) */

#if (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0)
			    status = rfat_volume_clean(volume, status);
#endif /* (RFAT_CONFIG_TRANSACTION_SAFE_SUPPORTED == 0) */
			}
		    }
		}
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

long f_filelength(const char *filename)
{
    int status = F_NO_ERROR;
    uint32_t length;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    length = 0;

    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
      status = rfat_path_find_file(volume, filename, NULL, NULL, &dir);
            
	if (status == F_NO_ERROR)
	{
	    length = dir->dir_file_size;
	}

	status = rfat_volume_unlock(volume, status);
    }

    return (long)length;
}

int f_findfirst(const char *filename, F_FIND *find)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;
    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	find->find_index = 0;

        status = rfat_path_find_directory(volume, filename, &filename, &find->find_clsno);
            
        if (status == F_NO_ERROR)
        {
#if (RFAT_CONFIG_VFAT_SUPPORTED == 0)

	    /* For non-VFAT, build a MSDOS style template where '*' is expanded into
	     * a sequence of '?'.
	     */

	    filename = rfat_name_cstring_to_pattern(filename, find->find_pattern);

	    if (filename == NULL)
	    {
		status = F_ERR_INVALIDNAME;
	    }

#else /* (RFAT_CONFIG_VFAT_SUPPORTED == 0) */

	    /* Copy the remaining filename into find_pattern, and do a WINNT compatible
	     * pattern match based upon the incoming lfn_name, or the converted sfn_name.
	     */

	    char c, *pattern, *pattern_e;
	    
	    pattern = &find->find_pattern[0];
	    pattern_e = &find->find_pattern[F_MAXPATH];
	    
	    do
	    {
		if (pattern == pattern_e)
		{
		    status = F_ERR_TOOLONGNAME;
		}
		else
		{
		    c = *filename++;
		    
		    *pattern++ = c;
		}
	    }
	    while ((status == F_NO_ERROR) && (c != '\0'));
#endif /* (RFAT_CONFIG_VFAT_SUPPORTED == 0) */

	    if (status == F_NO_ERROR)
	    {
		status = rfat_path_find_next(volume, find);
	    }
	}

	if (status != F_NO_ERROR)
	{
	    find->find_clsno = RFAT_CLSNO_END_OF_CHAIN;
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_findnext(F_FIND *find)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    volume = RFAT_FIND_VOLUME(find);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (find->find_clsno == RFAT_CLSNO_END_OF_CHAIN)
	{
	    status = F_ERR_NOTFOUND;
	}
	else
	{
	    status = rfat_path_find_next(volume, find);
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

#if !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD)

int f_settimedate(const char *filename, unsigned short ctime, unsigned short cdate)
{
    int status = F_NO_ERROR;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    status = rfat_path_find_file(volume, filename, NULL, NULL, &dir);
	
	    if (status == F_NO_ERROR)
	    {
		/* Nothing special to do here for TRANSACTION_SAFE/VOLUME_DIRTY/FSINFO. The dir entry
		 * update is atomic and not dependent upon the FAT.
		 */
		dir->dir_wrt_time = RFAT_HTOFS(ctime);
		dir->dir_wrt_date = RFAT_HTOFS(cdate);
	    
		status = rfat_dir_cache_write(volume);
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_gettimedate(const char *filename, unsigned short *p_ctime, unsigned short *p_cdate)
{
    int status = F_NO_ERROR;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	status = rfat_path_find_file(volume, filename, NULL, NULL, &dir);
	
	if (status == F_NO_ERROR)
	{
	    *p_ctime = RFAT_FTOHS(dir->dir_wrt_time);
	    *p_cdate = RFAT_FTOHS(dir->dir_wrt_date);
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_setattr(const char *filename, unsigned char attr)
{
    int status = F_NO_ERROR;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	if (volume->flags & RFAT_VOLUME_FLAG_WRITE_PROTECTED)
	{
	    status = F_ERR_WRITEPROTECT;
	}
	else
	{
	    status = rfat_path_find_file(volume, filename, NULL, NULL, &dir);
	
	    if (status == F_NO_ERROR)
	    {
		/* Nothing special to do here for TRANSACTION_SAFE/VOLUME_DIRT/FSINFO. The dir entry
		 * update is atomic and not dependent upon the FAT.
		 */
		dir->dir_attr = ((dir->dir_attr & ~(F_ATTR_ARC | F_ATTR_SYSTEM | F_ATTR_HIDDEN | F_ATTR_READONLY)) |
				 (attr & (F_ATTR_ARC | F_ATTR_SYSTEM | F_ATTR_HIDDEN | F_ATTR_READONLY)));
	    
		status = rfat_dir_cache_write(volume);
	    }
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

int f_getattr(const char *filename, unsigned char *p_attr)
{
    int status = F_NO_ERROR;
    rfat_dir_t *dir;
    rfat_volume_t *volume;

    volume = RFAT_PATH_VOLUME(filename);

    status = rfat_volume_lock(volume);
    
    if (status == F_NO_ERROR)
    {
	status = rfat_path_find_file(volume, filename, NULL, NULL, &dir);
	
	if (status == F_NO_ERROR)
	{
	    *p_attr = dir->dir_attr;
	}

	status = rfat_volume_unlock(volume, status);
    }

    return status;
}

#endif /* !defined(RFAT_CONFIG_ULTRA_LIGHT_BUILD) */

F_FILE * f_open(const char *filename, const char *type)
{
    int status = F_NO_ERROR;
    int c;
    rfat_file_t *file = NULL;
    uint32_t mode, size;
    rfat_volume_t *volume;

    mode = 0;
    size = 0;

    while ((status == F_NO_ERROR) && (c = *type++))
    {
	if ((c == 'r') || (c == 'w') || (c == 'a'))
	{
	    if (!(mode & (RFAT_FILE_MODE_READ | RFAT_FILE_MODE_WRITE)))
	    {
		if (c == 'r')
		{
		    mode = RFAT_FILE_MODE_READ;
		}
		else if (c == 'w')
		{
		    mode = RFAT_FILE_MODE_WRITE | RFAT_FILE_MODE_CREATE | RFAT_FILE_MODE_TRUNCATE;
		}
		else
		{
		    mode = RFAT_FILE_MODE_WRITE | RFAT_FILE_MODE_CREATE | RFAT_FILE_MODE_APPEND;
		}
		
		if (*type == '+')
		{
		    mode |= (RFAT_FILE_MODE_READ | RFAT_FILE_MODE_WRITE);
		    type++;
		}
	    }
	    else
	    {
		status = F_ERR_NOTUSEABLE;
	    }
	}
	else if (c == 'c')
	{
	    mode |= RFAT_FILE_MODE_COMMIT;
	}
	else if (c == 'S')
	{
	    mode &= ~RFAT_FILE_MODE_RANDOM;
	    mode |= RFAT_FILE_MODE_SEQUENTIAL;
	}
	else if (c == 'R')
	{
	    mode &= ~RFAT_FILE_MODE_SEQUENTIAL;
	    mode |= RFAT_FILE_MODE_RANDOM;
	}
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
	else if ((c == ',') && (*type != '\0'))
	{
	    /* ",<size>". Make sure that <size> does not overflow
	     * RFAT_FILE_SIZE_MAX.
	     */
	    while ((status == F_NO_ERROR) && (*type != '\0'))
	    {
		c = *type++;

		if ((c >= '0') && (c <= '9'))
		{
		    if (size >= (RFAT_FILE_SIZE_MAX / 10u))
		    {
			size = RFAT_FILE_SIZE_MAX;
		    }
		    else
		    {
			size = size * 10u;

			if (size >= (RFAT_FILE_SIZE_MAX - (c - '0')))
			{
			    size = RFAT_FILE_SIZE_MAX;
			}
			else
			{
			    size += (c - '0');
			}
		    }
		}
		else if ((c == 'K') && (*type == '\0'))
		{
		    if (size >= (RFAT_FILE_SIZE_MAX / 1024u))
		    {
			size = RFAT_FILE_SIZE_MAX;
		    }
		    else
		    {
			size = size * 1024u;
		    }
		}
		else if ((c == 'M') && (*type == '\0'))
		{
		    if (size >= (RFAT_FILE_SIZE_MAX / 1048576u))
		    {
			size = RFAT_FILE_SIZE_MAX;
		    }
		    else
		    {
			size = size * 1048576u;
		    }
		}
		else if ((c == 'G') && (*type == '\0'))
		{
		    if (size >= (RFAT_FILE_SIZE_MAX / 1073741824u))
		    {
			size = RFAT_FILE_SIZE_MAX;
		    }
		    else
		    {
			size = size * 1073741824u;
		    }
		}
		else
		{
		    status = F_ERR_NOTUSEABLE;
		}
	    }
	}
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
	else
	{
	    status = F_ERR_NOTUSEABLE;
	}
    }

    if ((status == F_NO_ERROR) && (mode & (RFAT_FILE_MODE_READ | RFAT_FILE_MODE_WRITE)))
    {
	volume = RFAT_PATH_VOLUME(filename);
      
        status = rfat_volume_lock(volume);
    
        if (status == F_NO_ERROR)
        {
	    status = rfat_file_open(volume, filename, mode, size, &file);

	    status = rfat_volume_unlock(volume, status);
        }
    }
    else
    {
        status = F_ERR_NOTUSEABLE;
    }

    return (status == F_NO_ERROR) ? file : NULL;
}

int f_close(F_FILE *file)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
	/* There is no check for "file->status" here, as this
	 * is handled in rfat_file_close(), so that the meta
	 * data gets updated, while the file data is treated
	 * as optional.
	 */

	volume = RFAT_FILE_VOLUME(file);
    
        status = rfat_volume_lock(volume);
    
        if (status == F_NO_ERROR)
        {
	    status = rfat_file_close(volume, file);

	    status = rfat_volume_unlock(volume, status);
        }
    }

    return status;
}

int f_flush(F_FILE *file)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
	status = F_ERR_NOTOPEN;
    }
    else
    {
	status = file->status;
	
	if (status == F_NO_ERROR)
	{
	    if (file->mode & RFAT_FILE_MODE_WRITE)
	    {
		volume = RFAT_FILE_VOLUME(file);
	    
		status = rfat_volume_lock(volume);
		
		if (status == F_NO_ERROR)
		{
		    status = rfat_file_flush(volume, file, FALSE);
		    
		    status = rfat_volume_unlock(volume, status);
		}
	    }
	}
    }

    return status;
}

long f_write(const void *buffer, long size, long count, F_FILE *file)
{
    int status = F_NO_ERROR;
    long result = 0;
    uint32_t total;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
        if (!(file->mode & RFAT_FILE_MODE_WRITE))
        {
            status = F_ERR_ACCESSDENIED;
        }
        else
        {
	    status = file->status;
	    
	    if (status == F_NO_ERROR)
	    {
		if ((size > 0) && (count > 0))
		{
		    volume = RFAT_FILE_VOLUME(file);
		    
		    status = rfat_volume_lock(volume);
		    
		    if (status == F_NO_ERROR)
		    {
			status = rfat_file_write(volume, file, (const uint8_t*)buffer, (unsigned long)count * (unsigned long)size, &total);

			result = total / (unsigned long)size;
			
			status = rfat_volume_unlock(volume, status);
		    }
                }
            }
        }
    }

    return result;
}

long f_read(void *buffer, long size, long count, F_FILE *file)
{
    int status = F_NO_ERROR;
    long result = 0;
    uint32_t total;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
        if (!(file->mode & RFAT_FILE_MODE_READ))
        {
            status = F_ERR_ACCESSDENIED;
        }
        else
        {
	    status = file->status;
		
	    if (status == F_NO_ERROR)
	    {
		if ((size > 0) && (count > 0))
		{
		    volume = RFAT_FILE_VOLUME(file);
		    
		    status = rfat_volume_lock(volume);
		    
		    if (status == F_NO_ERROR)
		    {
			status = rfat_file_read(volume, file, (uint8_t*)buffer, (unsigned long)count * (unsigned long)size, &total);

			result = total / (unsigned long)size;

			status = rfat_volume_unlock(volume, status);
		    }
                }
            }
        }
    }

    return result;
}

int f_seek(F_FILE *file, long offset, int whence)
{
    int status = F_NO_ERROR;
    uint32_t position;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
	status = file->status;

	if (status == F_NO_ERROR)
	{
	    switch (whence) {
	    case F_SEEK_CUR:
		if ((offset >= 0)
		    ? ((uint32_t)offset > (RFAT_FILE_SIZE_MAX - file->position))
		    : ((uint32_t)(0 - offset) > file->position))
		{
		    status = F_ERR_NOTUSEABLE;
		}
		else
		{
		    position = file->position + offset;
		}
		break;

	    case F_SEEK_END:
		if ((offset >= 0)
		    ? ((uint32_t)offset > (RFAT_FILE_SIZE_MAX - file->length))
		    : ((uint32_t)(0 - offset) > file->length))
		{
		    status = F_ERR_NOTUSEABLE;
		}
		else
		{
		    position = file->length + offset;
		}
		break;

	    case F_SEEK_SET:
		if ((offset < 0) || (offset > RFAT_FILE_SIZE_MAX))
		{
		    status = F_ERR_NOTUSEABLE;
		}
		else
		{
		    position = offset;
		}
		break;

	    default:
		status = F_ERR_NOTUSEABLE;
		break;
	    }
            
	    if (status == F_NO_ERROR)
	    {
		volume = RFAT_FILE_VOLUME(file);

		status = rfat_volume_lock(volume);
    
		if (status == F_NO_ERROR)
		{
		    status = rfat_file_seek(volume, file, position);

		    status = rfat_volume_unlock(volume, status);
		}
	    }
	}
    }

    return status;
}

long f_tell(F_FILE *file)
{
    int status = F_NO_ERROR;
    uint32_t position;
    
    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
	status = file->status;
        position = file->position;
    }

    return (status == F_NO_ERROR) ? (long)position : -1;
}

int f_eof(F_FILE *file)
{
    return (!file || !file->mode || (file->position >= file->length));
}

int f_error(F_FILE *file)
{
    int status = F_NO_ERROR;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
	status = file->status;
    }

    return status;
}

int f_rewind(F_FILE *file)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
	volume = RFAT_FILE_VOLUME(file);

	status = rfat_volume_lock(volume);
    
	if (status == F_NO_ERROR)
	{
	    /* Call rfat_disk_sync() to collect all outstanding asynchronous
	     * errors. If that succeeds, clear the error status for the file
	     * and seek to the beginning of the time.
	     */

	    status = rfat_data_cache_flush(volume, file);

	    if (status == F_NO_ERROR)
	    {
		status = rfat_disk_sync(volume->disk, &file->status);

		if (status == F_NO_ERROR)
		{
		    file->status = F_NO_ERROR;
		    
		    status = rfat_file_seek(volume, file, 0);
		}
	    }

	    status = rfat_volume_unlock(volume, status);
	}
    }

    return status;
}

int f_putc(int c, F_FILE *file)
{
    int status = F_NO_ERROR;
    int result = -1;
    uint8_t data;
    uint32_t total;
    rfat_cache_entry_t *entry;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
        if (!(file->mode & RFAT_FILE_MODE_WRITE))
        {
            status = F_ERR_ACCESSDENIED;
        }
        else
        {
	    status = file->status;

	    if (status == F_NO_ERROR)
	    {
		volume = RFAT_FILE_VOLUME(file);

#if (RFAT_CONFIG_FILE_DATA_CACHE == 1)
		entry = &file->data_cache;
#else /* (RFAT_CONFIG_FILE_DATA_CACHE) */
		entry = &volume->dir_cache;
#endif /* (RFAT_CONFIG_FILE_DATA_CACHE) */

		if ((entry->blkno == file->blkno) && ((file->mode & RFAT_FILE_MODE_APPEND) ? (file->position == file->length) : (file->position <= file->length)))
		{
		    file->flags |= RFAT_FILE_FLAG_DATA_MODIFIED;

		    *(entry->data + (file->position & RFAT_BLK_MASK)) = c;

		    rfat_data_cache_modify(volume, file);
                
		    file->position++;
                
		    if (!(file->position & RFAT_BLK_MASK))
		    {
			file->blkno++;
		    }
                
		    if (file->position >= file->length)
		    {
			file->length = file->position;
		    }

		    result = c;
		}
		else
		{
		    status = rfat_volume_lock(volume);
                
		    if (status == F_NO_ERROR)
		    {
			data = c;
                
			status = rfat_file_write(volume, file, &data, 1, &total);

			if (status == F_NO_ERROR)
			{
			    if (total == 1)
			    {
				result = c;
			    }
			}

			status = rfat_volume_unlock(volume, status);
		    }
		}
	    }
	}
    }

    return result;
}

int f_getc(F_FILE *file)
{
    int status = F_NO_ERROR;
    int result = -1;
    uint8_t data;
    uint32_t total;
    rfat_cache_entry_t *entry;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
        if (!(file->mode & RFAT_FILE_MODE_READ))
        {
            status = F_ERR_ACCESSDENIED;
        }
        else
        {
	    status = file->status;

	    if (status == F_NO_ERROR)
	    {
		volume = RFAT_FILE_VOLUME(file);

#if (RFAT_CONFIG_FILE_DATA_CACHE == 1)
		entry = &file->data_cache;
#else /* (RFAT_CONFIG_FILE_DATA_CACHE) */
		entry = &volume->dir_cache;
#endif /* (RFAT_CONFIG_FILE_DATA_CACHE) */
		    
		if ((entry->blkno == file->blkno) && (file->position < file->length))
		{
		    result = *(entry->data + (file->position & RFAT_BLK_MASK));
			
		    file->position++;
			
		    if (!(file->position & RFAT_BLK_MASK))
		    {
			file->blkno++;
		    }
		}
		else
		{
		    status = rfat_volume_lock(volume);
			
		    if (status == F_NO_ERROR)
		    {
			status = rfat_file_read(volume, file, &data, 1, &total);
			    
			if (total == 1)
			{
			    result = data;
			}
			    
			status = rfat_volume_unlock(volume, status);
		    }
		}
	    }
	}
    }

    return result;
}

int f_seteof(F_FILE *file)
{
    int status = F_NO_ERROR;
    rfat_volume_t *volume;

    if (!file || !file->mode)
    {
        status = F_ERR_NOTOPEN;
    }
    else
    {
	status = file->status;
	
	if (status == F_NO_ERROR)
	{
	    volume = RFAT_FILE_VOLUME(file);

	    status = rfat_volume_lock(volume);
        
	    if (status == F_NO_ERROR)
	    {
		/* Allow always a truncation, to deal with the case where there
		 * was a file->length but no cluster allocated.
		 */
		if ((file->position == 0) || (file->position < file->length))
		{
		    status = rfat_file_shrink(volume, file);
		}
		else
		{
		    if ((file->first_clsno == RFAT_CLSNO_NONE) || (file->length < file->position))
		    {
#if (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1)
			if (file->flags & RFAT_FILE_FLAG_CONTIGUOUS)
			{
			    if (file->size < file->position)
			    {
				status = F_ERR_EOF;
			    }
			}

			if (status == F_NO_ERROR)
#endif /* (RFAT_CONFIG_CONTIGUOUS_SUPPORTED == 1) */
			{
			    status = rfat_file_extend(volume, file, file->position);
			}
		    }
		}
		
		status = rfat_volume_unlock(volume, status);
	    }
	}
    }

    return status;
}

F_FILE * f_truncate(const char *filename, long length)
{
    int status = F_NO_ERROR;
    rfat_file_t *file;

    file = f_open(filename, "r+");

    if (file != NULL)
    {
	status = f_seek(file, length, F_SEEK_SET);

	if (status == F_NO_ERROR)
	{
	    status = f_seteof(file);
	}
	
	if (status != F_NO_ERROR)
	{
	    f_close(file);

	    file = NULL;
	}
    }

    return file;
}
 
