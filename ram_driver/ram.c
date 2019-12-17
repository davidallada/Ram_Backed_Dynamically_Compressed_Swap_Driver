#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/module.h>  // Needed by all modules
#include <linux/kernel.h>  // Needed for KERN_INFO
#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors

#include <linux/uaccess.h>

#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		(1 << PAGE_SECTORS_SHIFT)
#define PAGE_SIZE_BYTES		(1 << 12)
#define BLOCK_SIZE_BYTES    (1 << 7)
#define SECTOR_SIZE_BYTES	(1 << 9)
#define BLOCKS_IN_PAGE    (SECTOR_SIZE_BYTES * PAGE_SECTORS / BLOCK_SIZE_BYTES)

// ----- compression functions (exported) -----
long unrle(unsigned char *in, unsigned char *out, long sizein); // uncompress
long rle(unsigned char *in, unsigned char *out, long sizein); // compress
long enc_size(unsigned char * in, long size_in); // get size if we compress

int num_sectors = -1;
int B_TO_MB = 1000000;
int UNMAPPED;
int temp_comp_len = 0;

// represents a block of compressed data,
// note this has a smaller granularity than a page
struct compressed_block {
    char data[124]; // the compressed data
    int next; // offset node refers to as an index number (block) not bytes
};

struct brd_device {
	int		brd_number;

	struct request_queue	*brd_queue;
	struct gendisk		*brd_disk;
	struct list_head	brd_list;

	//SYSTEMS CAPSTONE VARIABLES
	int *                   	mapping_array; // mapping_array[sector] = index, if index < 0, it's in compressed, otherwise it's in uncmopressed
	int *                       rev_mapping_array; // rev_mapping_array[index] = sector, only for uncompressed
	int          			    index; // points to next uncompressed block
	int 			            oldest_block; // points to the oldest uncompressed block

    struct compressed_block *   head; // the first free compressed block
    int							head_sector; // the sector (index) corresponding to that block
    struct compressed_block *   tail; // the last free compressed block
    int							tail_sector; // the sector (index) corresponding to that block
    char * 						compressed_data; // where the compressed data is stored
    char * 						uncompressed_data; // where the uncompressed data is stored

	int	     		            num_compressed_blocks; // number of comrpessed blocks available

    // constants (self-explanatory)
	int 			            size_uncompressed_bytes;
	int	     		            size_compressed_bytes;
	int 			            num_uncompressed_pages;
	int 		            	high_watermark; // distance b/w oldest block and newest block (above level, compress)
	int 		                low_watermark; // distance b/w oldest block and newest block (at level, stop compressing)

    // bookkeeping (self-explanator)
	int 						used_compressed_bytes;
	int							used_uncompressed_bytes;
	int 						total_bytes_written;
	int							total_bytes_written_after_compression;

    /*
	 * Backing store of pages and lock to protect it. This is the contents
	 * of the block device.
	 */
	spinlock_t		brd_lock;
};

// compresses the page at `start_index`
static int compress_page(int start_index, struct brd_device* brd) 
{
	unsigned char array[PAGE_SIZE_BYTES]; // the uncompressed data
	unsigned char compressed_data[PAGE_SIZE_BYTES]; // the to-be compressed version of the uncompressed data

	memset(compressed_data, 1, 4096);
	memcpy(array, brd->uncompressed_data + (PAGE_SIZE_BYTES * start_index), PAGE_SIZE_BYTES); // grab the page

	int compressed_length = enc_size(array, 4096);

    // if compressing saves us no space, store the uncompressed version in the compressed section
	if(compressed_length >= 4096)
	{
		memcpy(compressed_data, array, 4096); //THIS is a janky solution. We should just use the uncompressed. In reality we should just keep it in the uncompressed in general
		compressed_length = 4096;
	}
	else
	{
		compressed_length = (int)rle(array, compressed_data, PAGE_SIZE_BYTES); // compress page
		temp_comp_len = compressed_length;
		brd->total_bytes_written_after_compression = brd->total_bytes_written_after_compression - 4096 + compressed_length;

	}

	int length = compressed_length;
	int num_blocks = (compressed_length + (BLOCK_SIZE_BYTES-sizeof(int))-1) / (BLOCK_SIZE_BYTES-sizeof(int)); // figure out how many compressed blocks we need (divide data by 124 bytes)

	// if we dont have enough compressed space, don't add to compressed
	if(brd->num_compressed_blocks - num_blocks <= 34)
	{
		return 0;
	}

	struct compressed_block * curr;
	int sector_of_first = brd->head_sector;

    // for every compressed block we're going to make out of the compressed data
	int i;
	for(i = 0; i < num_blocks; i++) 
	{
        // write to the next free compressed block
		memcpy(brd->head->data, compressed_data + (i*(BLOCK_SIZE_BYTES-sizeof(int))), min_t(size_t, (BLOCK_SIZE_BYTES-sizeof(int)), length));
		curr = brd->head;

        // update to next free compressed block
		brd->head_sector = brd->head->next;
		brd->head = (struct compressed_block *) brd->compressed_data + brd->head->next - 1;

		//set the next of the last block to be its negative length so its easier to uncompress
		if (i == num_blocks - 1)
		{
			curr->next = -(length);
		}

		length-= 124; // keep track of the length
	}

	brd->num_compressed_blocks -= num_blocks;

	return -sector_of_first; //return the starting index of the data as negative to indicate compressed
}


/*
 * Copy n bytes from src to the brd starting at sector. Does not sleep.
 */
// this is "writing" to the device
static void copy_to_brd(struct brd_device *brd, const void *src,
			sector_t sector, size_t n)
{
	// Handle out of bounds sector
	if(num_sectors != -1 && sector >= num_sectors)
	{
		return;
	}

    // If we hit the high watermark and we have compressed blocks available, we need to compress
	if(brd->used_uncompressed_bytes >= brd->high_watermark && brd->num_compressed_blocks > 34)
	{
		int total_compressed_bytes = 0;
		int total_num_compressed = 0;
		
        //while we're not at the low watermark
		while(brd->used_uncompressed_bytes > brd->low_watermark)
		{
			int sector = brd->rev_mapping_array[brd->oldest_block]; // get the sector for the (current) oldest block
			int ret_len = compress_page(brd->oldest_block, brd); // RETURN THE FIRST BLOCK INDEX THAT THE COMPRESSED DATA STARTS AT

            // if we can't compress anymore, stop
			if(ret_len == 0)
			{
				break;
			}

			brd->rev_mapping_array[brd->oldest_block] = UNMAPPED; // we no longer have anything mapped to this section of uncompressed data
			brd->mapping_array[sector] = ret_len; // set new compressed sector

            // move 4096 wrap around if necessary (arraydeque)
			brd->oldest_block++;
			if (brd->oldest_block >= brd->num_uncompressed_pages)
			{
				brd->oldest_block -= brd->num_uncompressed_pages;
			}

            // bookkeeping
			brd->used_uncompressed_bytes -= 4096; //data stuff
			total_compressed_bytes += temp_comp_len;
			total_num_compressed += 1;
		}
	}

	if(brd->mapping_array[sector] != UNMAPPED && brd->mapping_array[sector] >= 0) // the sector is mapped, and its mapping is in the uncompressed data portion
	{		
		memcpy(brd->uncompressed_data + (brd->mapping_array[sector] * PAGE_SIZE_BYTES), src, PAGE_SIZE_BYTES);
	}
	else
	{
		if (brd->mapping_array[sector] != UNMAPPED && brd->mapping_array[sector] < 0) // the sector is mapped, and its mapping is in the compressed data portion
		{
			int block_index = -(brd->mapping_array[sector] + 1); 
			struct compressed_block * curr = (struct compressed_block*) brd->compressed_data + block_index; // grab the compressed block
			brd->tail->next = block_index; // put this chain back into the pool of available compressed blocks

			// Break out when curr->next is negative signifying we're at the last block (and -curr->next is length of the last block)
            // Since this data is being overwritten, we're throwing it all out
			while(curr->next > 0)
			{
				curr = (struct compressed_block*) brd->compressed_data + curr->next - 1;
				brd->num_compressed_blocks++;
			}

            // set the end of the list of compressed blocks here
			brd->tail = curr;
			brd->tail->next = -1;
			brd->num_compressed_blocks++;
		}
		
		// DO ORIGINAL INSERT
		memcpy(brd->uncompressed_data + (brd->index * PAGE_SIZE_BYTES), src, PAGE_SIZE_BYTES); // copy to the next open uncompressed block
		brd->mapping_array[sector] = brd->index; // set the mapping array to the uncompressed block
		brd->rev_mapping_array[brd->index] = sector; // set the reverse mapping to the sector

        // increment where to place the next uncompressed block, wrapping around if necessary
		brd->index++;
		if (brd->index >= brd->num_uncompressed_pages)
		{
			brd->index -= brd->num_uncompressed_pages;
		}

        // bookkeeping
		brd->used_uncompressed_bytes += PAGE_SIZE_BYTES;
		brd->total_bytes_written += PAGE_SIZE_BYTES;
		brd->total_bytes_written_after_compression += PAGE_SIZE_BYTES;
	}
}

/*
 * Copy n bytes to dst from the brd starting at sector. Does not sleep.
 */
// this is "reading" from the device
static void copy_from_brd(void *dst, struct brd_device *brd,
			sector_t sector, size_t n)
{
    // if this sector is invalid, return a blank page
	if(num_sectors != -1 && sector >= num_sectors)
	{
		memset(dst, 0, 4096);
		return;
	}
		
    // if this sector is UNMAPPED, return a blank page
    if (brd->mapping_array[sector] == UNMAPPED) {
        memset(dst, 0, 4096);
        return;
    }

    // check if the data is in the compressed portion
    if (brd->mapping_array[sector] < 0) {
        int block_index = -(brd->mapping_array[sector] + 1);

        unsigned char array[PAGE_SIZE_BYTES];
        unsigned char uncompressed[PAGE_SIZE_BYTES];

        struct compressed_block * curr = (struct compressed_block*) brd->compressed_data + block_index;
        
        int total_size = 0;

        // add this chain back into the pool of available compressed blocks
        brd->tail->next = block_index;

        // grab all the compressed blocks and put them into `array`
        // Break out when curr->next is negative signifying we're at the last block (and -curr->next is length of the last block)
        while(curr->next > 0)
        {
			memcpy(array + total_size, curr, BLOCK_SIZE_BYTES - sizeof(int));
			total_size += BLOCK_SIZE_BYTES - sizeof(int);
			curr = (struct compressed_block*)brd->compressed_data + curr->next - 1;
			brd->num_compressed_blocks++;
		}

		brd->num_compressed_blocks++;

        // -curr->next is the length of the last block
		memcpy(array + total_size, curr, -(curr->next));
		total_size += -(curr->next);
		brd->tail = curr;
		brd->tail->next = -1;

		int return_length;

		// Do uncompression
        // if total_size >= 4096, then we stored the uncompressed version in the compressed section
		if(total_size < 4096)
		{
            // decompress it
			return_length = unrle((unsigned char*) array, (unsigned char*) uncompressed, total_size); 
			brd->total_bytes_written_after_compression = brd->total_bytes_written_after_compression - total_size + return_length;
		}
		else
		{
			if(total_size > 4096)
			{
				printk(KERN_INFO "Size to be uncompressed Greater than 4096: %d", total_size);
				//Crash
				BUG();
			}

			memcpy(uncompressed, array, total_size);
			return_length = total_size;
		}

		BUG_ON(return_length != PAGE_SIZE_BYTES);

        // If we hit the high watermark and we have compressed blocks available, we need to compress
        if(brd->used_uncompressed_bytes >= brd->high_watermark && brd->num_compressed_blocks > 34)
        {
            int total_compressed_bytes = 0;
            int total_num_compressed = 0;

            //while we're not at the low watermark
            while(brd->used_uncompressed_bytes > brd->low_watermark)
            {
                int sector = brd->rev_mapping_array[brd->oldest_block]; // get the sector for the (current) oldest block
                int ret_len = compress_page(brd->oldest_block, brd); // RETURN THE FIRST BLOCK INDEX THAT THE COMPRESSED DATA STARTS AT

                // if we can't compress anymore, stop
                if(ret_len == 0)
                {
                    break;
                }

                brd->rev_mapping_array[brd->oldest_block] = UNMAPPED; // we no longer have anything mapped to this section of uncompressed data
                brd->mapping_array[sector] = ret_len; // set new compressed sector

                // move 4096 wrap around if necessary (arraydeque)
                brd->oldest_block++;
                if (brd->oldest_block >= brd->num_uncompressed_pages)
                {
                    brd->oldest_block -= brd->num_uncompressed_pages;
                }

                // bookkeeping
                brd->used_uncompressed_bytes -= 4096; //data stuff
                total_compressed_bytes += temp_comp_len;
                total_num_compressed += 1;
            }
        }

        // copy the uncompressed data into the uncompressed portion
		memcpy(brd->uncompressed_data + (brd->index * PAGE_SIZE_BYTES), uncompressed, PAGE_SIZE_BYTES);
		
		brd->mapping_array[sector] = brd->index; // set the mapping array to the uncompressed block
		brd->rev_mapping_array[brd->index] = sector; // set the reverse mapping to the sector
		brd->index++;

        // increment where to place the next uncompressed block, wrapping around if necessary
		if (brd->index >= brd->num_uncompressed_pages)
		{
			brd->index -= brd->num_uncompressed_pages;
		}

        // bookkeeping
		brd->used_uncompressed_bytes += PAGE_SIZE_BYTES;
	}
	
	// return the uncompressed data
	memcpy(dst, brd->uncompressed_data + (brd->mapping_array[sector] * PAGE_SIZE_BYTES), PAGE_SIZE_BYTES);
}

/*
 * Process a single bvec of a bio.
 */
static int brd_do_bvec(struct brd_device *brd, struct page *page,
			unsigned int len, unsigned int off, unsigned int op,
			sector_t sector)
{
	/*
		Args:
			-page, length of page, operation, sector
	*/
	printk(KERN_INFO "bvec\n");
	void *mem;
	int err = 0;
	
	// Map a page temporarily so you do not lose it
	mem = kmap_atomic(page);

    // either call copy_from_brd or copy_to_brd
    // depending on if we're reading or writing
	if (!op_is_write(op)) {
		copy_from_brd(mem + off, brd, sector, len);
		flush_dcache_page(page);
	} else {
		flush_dcache_page(page);
		copy_to_brd(brd, mem + off, sector, len);
	}

	kunmap_atomic(mem);
	printk(KERN_INFO "Total Bytes Written Without Compression: %d", brd->total_bytes_written);
	printk(KERN_INFO "Total Bytes Written With Compression: %d", brd->total_bytes_written_after_compression);
	
out:
	return err;
}

// original function, did not modify
static blk_qc_t brd_make_request(struct request_queue *q, struct bio *bio)
{
	struct brd_device *brd = bio->bi_disk->private_data;
	spin_lock(&brd->brd_lock);
	struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;

	sector = bio->bi_iter.bi_sector;
	if (bio_end_sector(bio) > get_capacity(bio->bi_disk))
		goto io_error;

	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;
		int err;

		err = brd_do_bvec(brd, bvec.bv_page, len, bvec.bv_offset,
				  bio_op(bio), sector);
		if (err)
			goto io_error;
		sector += len >> SECTOR_SHIFT;
	}

	bio_endio(bio);
	spin_unlock(&brd->brd_lock);
	return BLK_QC_T_NONE;
io_error:
	bio_io_error(bio);
	return BLK_QC_T_NONE;
}

// original function, did not modify
static int brd_rw_page(struct block_device *bdev, sector_t sector,
		       struct page *page, unsigned int op)
{
	void *dst;

	struct brd_device *brd = bdev->bd_disk->private_data;
	spin_lock(&brd->brd_lock);
	int err;

	if (PageTransHuge(page))
		return -ENOTSUPP;

	err = brd_do_bvec(brd, page, PAGE_SIZE, 0, op, sector);
	page_endio(page, op_is_write(op), err);
	spin_unlock(&brd->brd_lock);
	return err;
}

// set the operations of this device
static const struct block_device_operations brd_fops = {
	.owner =		THIS_MODULE,
	.rw_page =		brd_rw_page,
};

// And now the modules code and kernel interface.
static int rd_nr = CONFIG_BLK_DEV_RAM_COUNT;
module_param(rd_nr, int, 0444);
MODULE_PARM_DESC(rd_nr, "Maximum number of brd devices");

unsigned long rd_size = CONFIG_BLK_DEV_RAM_SIZE;
module_param(rd_size, ulong, 0444);
MODULE_PARM_DESC(rd_size, "Size of each RAM disk in kbytes.");

static int max_part = 1;
module_param(max_part, int, 0444);
MODULE_PARM_DESC(max_part, "Num Minors to reserve between devices");

//Percent Compressed (In Percent) Kernel does not allow floats 
static int part_compressed = 25;
module_param(part_compressed, int, 0444);
MODULE_PARM_DESC(part_compressed, "Percent towards compressed memory");

static int low_water_param = 50;
module_param(low_water_param, int, 0444);
MODULE_PARM_DESC(part_compressed, "Low water mark for when compressing pages (as a %)");

static int high_water_param = 99;
module_param(high_water_param, int, 0444);
MODULE_PARM_DESC(part_compressed, "High water mark at which you should compress pages (as a %)");

MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(RAMDISK_MAJOR);
MODULE_ALIAS("rd");

#ifndef MODULE
/* Legacy boot options - nonmodular */
static int __init ramdisk_size(char *str)
{
	rd_size = simple_strtol(str, NULL, 0);
	return 1;
}
__setup("ramdisk_size=", ramdisk_size);
#endif

/*
 * The device scheme is derived from loop.c. Keep them in synch where possible
 * (should share code eventually).
 */
static LIST_HEAD(brd_devices);
static DEFINE_MUTEX(brd_devices_mutex);

// allocate everything needed for this device
static struct brd_device *brd_alloc(int i)
{
	struct brd_device *brd;
	struct gendisk *disk;

	brd = kzalloc(sizeof(*brd), GFP_KERNEL);
	if (!brd)
		goto out;
	brd->brd_number		= i;
	spin_lock_init(&brd->brd_lock);
	//INIT_RADIX_TREE(&brd->brd_pages, GFP_ATOMIC);

	brd->brd_queue = blk_alloc_queue(GFP_KERNEL);
	if (!brd->brd_queue)
		goto out_free_dev;

	blk_queue_make_request(brd->brd_queue, brd_make_request);
	blk_queue_max_hw_sectors(brd->brd_queue, 1024);
	
	
	
	/* This is so fdisk will align partitions on 4k, because of
	 * direct_access API needing 4k alignment, returning a PFN
	 * (This is only a problem on very small devices <= 4M,
	 *  otherwise fdisk will align on 1M. Regardless this call
	 *  is harmless)
	 */
	blk_queue_physical_block_size(brd->brd_queue, PAGE_SIZE);
	disk = brd->brd_disk = alloc_disk(max_part);
	if (!disk)
		goto out_free_queue;
	disk->major		= RAMDISK_MAJOR;
	disk->first_minor	= i * max_part;
	disk->fops		= &brd_fops;
	disk->private_data	= brd;
	disk->flags		= GENHD_FL_EXT_DEVT;
	sprintf(disk->disk_name, "ram%d", i);
	set_capacity(disk, rd_size * 2);
	brd->brd_queue->backing_dev_info->capabilities |= BDI_CAP_SYNCHRONOUS_IO;

	/* Tell the block layer that this is not a rotational device */
	blk_queue_flag_set(QUEUE_FLAG_NONROT, brd->brd_queue);
	blk_queue_flag_clear(QUEUE_FLAG_ADD_RANDOM, brd->brd_queue);

	// SYSTEMS CAPSTONE ADD HERE *************************************************************************************************************************************************************************
	int rd_size_bytes = rd_size * 1000;
	num_sectors = (rd_size_bytes) / SECTOR_SIZE_BYTES; //rd_size in kB, so multiply by 1000 to get bytes then divide by sector size in bytes to get sectors

	// Set Compressed and UnCompressed Sizes:
	brd->size_compressed_bytes = (int) (((long long)rd_size_bytes * (long long)part_compressed) / (long long)100); // rd_size in kB, so multiply by 1000 to get bytes and then multiply by percent compressed and divide by 100

	// Put this here so we can trick into having larger compressed_bytes
	brd->size_uncompressed_bytes = (rd_size_bytes) - brd->size_compressed_bytes;
	printk(KERN_INFO "size_uncompressed_bytes %d", brd->size_uncompressed_bytes);

	brd->size_compressed_bytes = brd->size_compressed_bytes * 11 / 10;
	printk(KERN_INFO "Size_compressed_bytes: %d", brd->size_compressed_bytes);

	brd->num_compressed_blocks = brd->size_compressed_bytes / BLOCK_SIZE_BYTES; //128 bytes per block
	printk(KERN_INFO "num_compressed_blocks %d", brd->num_compressed_blocks);

	brd->num_uncompressed_pages = brd->size_uncompressed_bytes / PAGE_SIZE_BYTES; // 4096 bytes per page
	printk(KERN_INFO "num_uncompressed_pages: %d", brd->num_uncompressed_pages);

	brd->used_compressed_bytes = 0;
	brd->used_uncompressed_bytes = 0;

    // set the value of UNMAPPED to a value we will never set
	UNMAPPED = brd->num_compressed_blocks + brd->num_uncompressed_pages;

    // allocate mapping arrays and set everything to UNMAPPED by default
	brd->mapping_array = (int*) vmalloc(num_sectors * sizeof(int));
	brd->rev_mapping_array = (int*) vmalloc(num_sectors * sizeof(int));

	int j = 0;
	for (j = 0; j < num_sectors; j++) {
	    brd->mapping_array[j] = UNMAPPED;
	    brd->rev_mapping_array[j]= UNMAPPED;
	}

	brd->total_bytes_written = 0;
	brd->total_bytes_written_after_compression = 0;

    // set watermarks
	brd->low_watermark = (int)((long long)brd->size_uncompressed_bytes * low_water_param/ 100);
	if(brd->size_compressed_bytes == 0)
	{
		brd->high_watermark = (int)((long long)brd->size_uncompressed_bytes + 1); //Make it so we never go to compressed
	}
	else
	{
		brd->high_watermark = (int)((long long)brd->size_uncompressed_bytes * high_water_param / 100) - 1;
	}

    // ----- set init compressed portion -----
	if(brd->size_compressed_bytes > 0)
	{
		brd->compressed_data = (char*)vmalloc(sizeof(char) * brd->size_compressed_bytes); // Create a large char array size of the compressed section in bytes
	}
	else
	{
		brd->compressed_data = NULL;

	}
	struct compressed_block temp_block;
	memset(&temp_block, 0, 128);

	int next = 2; // Index related to the mapping index

	int k;
	for(k=1; k < brd->num_compressed_blocks; k++)
	{
		temp_block.next = next;
		memcpy(brd->compressed_data + (BLOCK_SIZE_BYTES*(k-1)), &temp_block, BLOCK_SIZE_BYTES); //write a blank compressed_block to each 128 byte block
		next++; //increment next
	}

	brd->head = (struct compressed_block *)brd->compressed_data; // return the first block at offset 0 of compressed data
	brd->head_sector = 1;

	brd->tail = (struct compressed_block *)brd->compressed_data + ((brd->num_compressed_blocks - 1)); // return the last block
	brd->tail_sector = brd->num_compressed_blocks - 1;
	
    // ----- set init uncompressed portion -----
	brd->uncompressed_data = (char*)vmalloc(sizeof(char) * brd->size_uncompressed_bytes); // Create a large char array size of the compressed section in bytes
	printk(KERN_INFO "uncompressed_data_size %d", sizeof(char) * brd->size_uncompressed_bytes);

	brd->index = 0;
	brd->oldest_block = 0;

	printk(KERN_INFO "Pages: %d", brd->num_uncompressed_pages); 
	
	pr_info("brd: Module Loaded\n"
		"Total Size Requested: %d MB\n"
		"Percent Memory For Compressed Data: %d\n"
		"Compressed Memory Size: %d MB\n"
		"UnCompressed Memory Size: %d MB\n"
		"High Watermark: %d MB / %d MB\n"
		"Low Watermark: %d MB / %d MB\n"
		, rd_size / 1000, part_compressed, brd->size_compressed_bytes / B_TO_MB, brd->size_uncompressed_bytes / B_TO_MB, brd->high_watermark / B_TO_MB, rd_size / 1000, brd->low_watermark / B_TO_MB, rd_size / 1000);
	
	return brd;

out_free_queue:
	blk_cleanup_queue(brd->brd_queue);
out_free_dev:
	kfree(brd);
out:
	return NULL;
}

// ----- everything below here is an original function that we did not modify -----

static void brd_free(struct brd_device *brd)
{
	put_disk(brd->brd_disk);
	blk_cleanup_queue(brd->brd_queue);
	//brd_free_pages(brd);
	kfree(brd);
}

static struct brd_device *brd_init_one(int i, bool *new)
{
	struct brd_device *brd;

	*new = false;
	list_for_each_entry(brd, &brd_devices, brd_list) {
		if (brd->brd_number == i)
			goto out;
	}

	brd = brd_alloc(i);
	if (brd) {
		brd->brd_disk->queue = brd->brd_queue;
		add_disk(brd->brd_disk);
		list_add_tail(&brd->brd_list, &brd_devices);
	}
	*new = true;
out:
	return brd;
}

static void brd_del_one(struct brd_device *brd)
{
	list_del(&brd->brd_list);
	del_gendisk(brd->brd_disk);
	//vfree(brd->mapping_array);
	//vfree(brd->rev_mapping_array);
	brd_free(brd);
}

static struct kobject *brd_probe(dev_t dev, int *part, void *data)
{
	struct brd_device *brd;
	struct kobject *kobj;
	bool new;

	mutex_lock(&brd_devices_mutex);
	brd = brd_init_one(MINOR(dev) / max_part, &new);
	kobj = brd ? get_disk_and_module(brd->brd_disk) : NULL;
	mutex_unlock(&brd_devices_mutex);

	if (new)
		*part = 0;

	return kobj;
}

static int __init brd_init(void)
{
	struct brd_device *brd, *next;
	int i;
	/*
	 * brd module now has a feature to instantiate underlying device
	 * structure on-demand, provided that there is an access dev node.
	 *f
	 * (1) if rd_nr is specified, create that many upfront. else
	 *     it defaults to CONFIG_BLK_DEV_RAM_COUNT
	 * (2) User can further extend brd devices by create dev node themselves
	 *     and have kernel automatically instantiate actual device
	 *     on-demand. Example:
	 *		mknod /path/devnod_name b 1 X	# 1 is the rd major
	 *		fdisk -l /path/devnod_name
	 *	If (X / max_part) was not already created it will be created
	 *	dynamically.
	 */

	if (register_blkdev(RAMDISK_MAJOR, "ramdisk"))
		return -EIO;

	if (unlikely(!max_part))
		max_part = 1;

	for (i = 0; i < rd_nr; i++) {
		brd = brd_alloc(i);
		if (!brd)
			goto out_free;
		list_add_tail(&brd->brd_list, &brd_devices);
	}
	
	
	
	/* point of no return */

	list_for_each_entry(brd, &brd_devices, brd_list) {
		/*
		 * associate with queue just before adding disk for
		 * avoiding to mess up failure path
		 */
		brd->brd_disk->queue = brd->brd_queue;
		add_disk(brd->brd_disk);
	}

	blk_register_region(MKDEV(RAMDISK_MAJOR, 0), 1UL << MINORBITS,
				  THIS_MODULE, brd_probe, NULL, NULL);


	
	return 0;


out_free:
	list_for_each_entry_safe(brd, next, &brd_devices, brd_list) {
		list_del(&brd->brd_list);
		brd_free(brd);
	}
	unregister_blkdev(RAMDISK_MAJOR, "ramdisk");

	pr_info("brd: module NOT loaded !!!\n");
	
	return -ENOMEM;
}

static void __exit brd_exit(void)
{
	struct brd_device *brd, *next;

	list_for_each_entry_safe(brd, next, &brd_devices, brd_list)
		brd_del_one(brd);

	blk_unregister_region(MKDEV(RAMDISK_MAJOR, 0), 1UL << MINORBITS);
	unregister_blkdev(RAMDISK_MAJOR, "ramdisk");
	/*vfree(brd->uncompressed_data);
	if(brd->compressed_data != NULL)
	{
		vfree(brd->compressed_data);
	}*/
	
	//pr_info("brd: module unloaded\n");
}

module_init(brd_init);
module_exit(brd_exit);
