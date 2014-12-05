#include "vm/frame.h"
#include <debug.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include <string.h>
#include "threads/vaddr.h"
#include "threads/pte.h"
#include <bitmap.h>

struct frame
  {
    struct list_elem elem; /* Hash table element. */
    void *page;            /* Page occupying this frame. */
    struct thread* thread; /* Owner of this frame. */
    uint8_t *user_addr;    /* Stored to associate frames and sup_pt entries */
    uint32_t *pte;         /* Page table entry */
    bool pinned;           /* Is the frame pinned? */
  };

static struct list frame_table;

static struct lock frame_lock;
static struct lock eviction_lock;

static struct frame *select_frame_to_evict (void);
static void *evict_frame (void);
static void pin_frame (struct frame *);
static void unpin_frame (struct frame *);
static struct frame *get_frame (void* page);

void
frame_init (void)
{
  list_init (&frame_table);
  lock_init (&frame_lock);
  lock_init (&eviction_lock);
}

/* Get a frame by calling palloc_get_page and allocating a struct frame. If
   no frame is available, evict a frame and use that */
void *
allocate_frame (enum palloc_flags flags)
{
  lock_acquire (&frame_lock);
  void *page = palloc_get_page (flags);
  lock_release (&frame_lock);
  struct frame *f;

  if (page != NULL)
    {
      f = malloc (sizeof (struct frame));
      if (f == NULL)
        PANIC ("Failed to allocate memory for frame.");

      f->thread = thread_current ();
      f->page = page;
      f->pinned = false;

      lock_acquire (&frame_lock);
      list_push_back (&frame_table, &f->elem);
      lock_release (&frame_lock);
    }
  else
    {
      page = evict_frame ();
      ASSERT (page != NULL);
    }

  return page;
}

/* Set the user page and page table entry as UPAGE and PTE on the frame with
   page PAGE. */
void
set_user_address (void *page, uint32_t *pte, void *upage)
{
  struct frame *f = get_frame (page);

  if (f != NULL)
    {
      f->pte = pte;
      f->user_addr = upage;
    }
}

/* Choose a frame to evict and clear it from the page directory of its owner
   thread */
void *
evict_frame (void)
{
  struct frame *choice;
  struct thread *cur = thread_current ();

  lock_acquire (&eviction_lock);

  /* Pick a suitable candidate frame */
  choice = select_frame_to_evict ();

  lock_release (&eviction_lock);

  if (choice == NULL)
    PANIC ("No frames could be evicted.");

  struct thread *t = choice->thread;
  struct supp_page *page = get_supp_page (&t->supp_table, choice->user_addr);

  if (page == NULL)
    {
      /* The sup_page may longer exist if the data was loaded from swap */
      page = malloc (sizeof (struct supp_page));
      page->user_addr = choice->user_addr;
      page->type = 3;
      add_supp_page (&t->supp_table, page);
    }

  size_t swap_index = 0;
  /* Swap the data out if the frame is dirty or if the data cannot be reloaded
     from a file */
  if (pagedir_is_dirty (t->pagedir, page->user_addr) || (page->type != 1))
    {
      pin_frame (choice);
      swap_index = pick_slot_and_swap (page->user_addr);
      unpin_frame (choice);

      if (swap_index == BITMAP_ERROR)
        PANIC ("Could not swap out frame");

      page->type = page->type | 3;
    }

  /* Set the page as writeable if the corresponding page table entry is
     writeable */
  page->swap_index = swap_index;
  page->swap_writable = *(choice->pte) & PTE_W;

  choice->thread = cur;
  choice->pte = NULL;
  choice->user_addr = NULL;

  /* Clear the frame from the former owner's page directory */
  lock_acquire (&t->pd_lock);
  pagedir_clear_page (t->pagedir, page->user_addr);
  lock_release (&t->pd_lock);

  page->loaded = false;

  return choice->page;
}

static struct frame *
select_frame_to_evict (void)
{
  struct frame *choice = NULL;
  struct list_elem *e;

  /* Second chance page replacement algorithm. We always ignore pinned
     frames */
  int j = 0;
  for (; j < 2; ++j)
    {
      /* The first loop finds any perfect candidates for eviction - one that
         is neither accessed nor dirty. */
      for (e = list_begin (&frame_table); e != list_end (&frame_table);
           e = list_next (e))
        {
          choice = list_entry (e, struct frame, elem);
          if (choice->pinned)
            continue;

          struct thread* owner = choice->thread;
          lock_acquire (&owner->pd_lock);

          if (!pagedir_is_dirty (owner->pagedir, choice->user_addr) &&
              !pagedir_is_accessed (owner->pagedir, choice->user_addr))
            {
              /* A perfect frame to evict */
              lock_release (&owner->pd_lock);
              return choice;
            }

          lock_release (&owner->pd_lock);
        }

      /* The second loop looks for slightly worse candidates - ones that are
         not accessed but dirty. If they do not match this criteria, they are
         given a second chance - their accessed bit is set, and we execute the
         for loop again. */
      for (e = list_begin (&frame_table); e != list_end (&frame_table);
           e = list_next (e))
        {
          choice = list_entry (e, struct frame, elem);
          if (choice->pinned)
            continue;

          struct thread* owner = choice->thread;
          lock_acquire (&owner->pd_lock);
          if (pagedir_is_dirty (owner->pagedir, choice->user_addr) &&
              !pagedir_is_accessed (owner->pagedir, choice->user_addr))
            {
              lock_release (&owner->pd_lock);

              lock_acquire (&frame_lock);
              list_remove (e);
              list_push_back (&frame_table, e);
              lock_release (&frame_lock);

              return choice;
            }

          /* Frames that reach here will be hit earlier the next time round,
             in the first while loop or the second depending on whether they
             are dirty or not */
          pagedir_set_accessed (owner->pagedir, choice->user_addr, false);
          lock_release (&owner->pd_lock);
        }
    }

  /* This will only get hit if every frame in existence is pinned */
  return NULL;
}

/* Remove a frame struct from the frame table and free it */
void
free_frame (void *page)
{
  struct frame *f;
  struct list_elem *e;

  lock_acquire (&frame_lock);
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      f = list_entry (e, struct frame, elem);
      if (f->page == page)
        {
          list_remove (e);
          free (f);
          break;
        }
    }
  lock_release (&frame_lock);

  palloc_free_page (page);
}

static void
pin_frame (struct frame *frame)
{
  frame->pinned = true;
}

static void
unpin_frame (struct frame *frame)
{
  frame->pinned = false;
}

void
pin_frame_by_page (void* kpage)
{
  struct frame *f = get_frame (kpage);
  f->pinned = true;
}

void
unpin_frame_by_page (void* kpage)
{
  struct frame *f = get_frame (kpage);
  f->pinned = false;
}

/* Get a frame from its page address */
static struct frame*
get_frame (void* page)
{
  struct frame *f;
  struct list_elem *e;

  lock_acquire (&frame_lock);
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      f = list_entry (e, struct frame, elem);
      if (f->page == page)
        break;

      f = NULL;
    }
  lock_release (&frame_lock);

  return f;
}

/* Remove any frames from the frame table that are owned by the given thread.
   Called by process_exit() */
void
reclaim_frames (struct thread *t)
{
  struct frame *f;
  struct list_elem *e;
  struct list_elem *next = NULL;

  lock_acquire (&frame_lock);
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = next)
    {
      next = list_next (e);

      f = list_entry (e, struct frame, elem);
      ASSERT (f != NULL);

      if (f->thread == t)
        {
          list_remove (e);
          free (f);
        }
    }
  lock_release (&frame_lock);
}