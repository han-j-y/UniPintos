#ifndef __VM_PAGE__
#define __VM_PAGE__

#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "filesys/off_t.h"

struct supp_page
{
	void* user_addr;
	bool loaded;

	int type;
	struct file* file;
	off_t offset;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;

	size_t swap_index;
	bool swap_writable;

	struct hash_elem elem;
};

struct supp_page* create_supp_page (struct file*, off_t offset, 
									uint32_t read_bytes, uint32_t zero_bytes,
									bool writable, uint8_t* addr );
bool add_supp_page (struct hash*, struct supp_page*);
void delete_supp_page (struct supp_page*);
struct supp_page* get_supp_page (struct hash*, uint8_t*);
void reclaim_pages (struct hash*);

#endif