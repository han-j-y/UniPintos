#include "vm/mmap.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "threads/malloc.h"

unsigned
mapping_hash (const struct hash_elem *m_, void *aux UNUSED)
{
  struct mapping *m = hash_entry (m_, struct mapping, elem);

  ASSERT (m != NULL);

  return hash_bytes (&m->addr, sizeof m->addr);
}

bool
mapping_less (const struct hash_elem *a_, const struct hash_elem *b_,
              void *aux UNUSED)
{
  struct mapping *a = hash_entry (a_, struct mapping, elem);
  struct mapping *b = hash_entry (b_, struct mapping, elem);

  ASSERT (a != NULL);
  ASSERT (b != NULL);

  return a->addr < b->addr;
}

void mapping_destroy (struct hash_elem *m_, void *aux UNUSED)
{
  struct mapping *m = hash_entry (m_, struct mapping, elem);

  ASSERT (m != NULL);

  file_lock_acquire ();
  file_close (m->file);
  file_lock_release ();

  free (m);
}