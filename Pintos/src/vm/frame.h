#ifndef FRMAE_H
#define FRMAE_H

#include "threads/thread.h"
#include "threads/palloc.h"

void frame_init (void);
void *allocate_frame (enum palloc_flags flags);
void free_frame (void*);
void set_user_address (void*, uint32_t*, void*);
void pin_frame_by_page (void* kpage);
void unpin_frame_by_page (void* kpage);
void reclaim_frames (struct thread *t);

#endif