#ifndef MMAP_H
#define MMAP_H

#include <hash.h>
#include "userprog/syscall.h"


struct mapping
  {
    mapid_t mapid;
    struct file *file;
    void *addr;
    int num_pages;
    struct hash_elem elem;
  };

unsigned mapping_hash (const struct hash_elem *m_, void *aux UNUSED);
bool mapping_less (const struct hash_elem *a_, const struct hash_elem *b_,
                   void *aux UNUSED);
void mapping_destroy (struct hash_elem *m_, void *aux UNUSED);

#endif