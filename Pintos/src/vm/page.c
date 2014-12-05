#include "userprog/pagedir.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "threads/init.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "page.h"

struct supp_page* create_supp_page (struct file* f, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable, uint8_t* addr )
{
	struct supp_page* spage = malloc (sizeof (struct supp_page));
	ASSERT (spage != NULL);

	spage->file = f;
	spage->type = 1;
	spage->writable = writable;
	spage->offset = offset;
    spage->zero_bytes = zero_bytes;
    spage->read_bytes = read_bytes;
    spage->user_addr = addr;
    spage->loaded = false;

    return spage;
}

bool add_supp_page (struct hash* h, struct supp_page* spage)
{
	return hash_insert (h, &spage->elem) == NULL;
}
void delete_supp_page (struct supp_page *spage)
{
	hash_delete (&thread_current()->supp_table, &spage->elem);
}

struct supp_page* get_supp_page (struct hash *h, uint8_t* addr)
{
	struct supp_page temp;
	temp.user_addr = addr;

	struct hash_elem *temp_elem = hash_find (h, &temp.elem);

	if (temp_elem == NULL)
		return NULL;
	else
		return hash_entry (temp_elem, struct supp_page, elem);
}

void* free_sup_pages (struct hash_elem *e, void* aux UNUSED)
{
	struct supp_page *p = hash_entry (e, struct supp_page, elem);
	free (p);
}

void reclaim_pages (struct hash* h)
{
	hash_destroy (h, free_sup_pages);
}