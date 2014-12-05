#ifndef __VM_PAGE__
#define __VM_PAGE__

#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "filesys/off_t.h"

struct supp_page
{
	void* user_addr;		/*user address of this page*/
	bool loaded;			/*If the corresponding frame is loaded or not*/

	int type;			/*supp_page type. 1 is file, 2 is swap, 3 file in swap*/
	struct file* file;		/*if supp_page type is 1, corresponding file*/
	off_t offset;			/*offset of the virtual address*/
	uint32_t read_bytes;		/*needed space for data out of 4KB*/
	uint32_t zero_bytes;		/*4KB - read_bytes*/
	bool writable;			/*if the page is writable or not*/

	size_t swap_index;		/*index to swap*/
	bool swap_writable;		/*if swap is  writable or not*/

	struct hash_elem elem;		/*supplemental page table element*/
};

struct supp_page* create_supp_page (struct file*, off_t offset, 
									uint32_t read_bytes, uint32_t zero_bytes,
									bool writable, uint8_t* addr );
bool add_supp_page (struct hash*, struct supp_page*);
void delete_supp_page (struct supp_page*);
struct supp_page* get_supp_page (struct hash*, uint8_t*);
void reclaim_pages (struct hash*);

#endif
