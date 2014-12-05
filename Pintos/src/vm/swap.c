#include "vm/swap.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include <bitmap.h>
#include "threads/synch.h"

/* This is always 2, but for neatness we define it here */
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *block_device;
static struct bitmap *swap_slot_map;

void
init_swap_structures (void)
{
  /* Initialise the BLOCK_SWAP device */
  block_device = block_get_role (BLOCK_SWAP);

  /* Calculate how big we need to make the bitmap. Everything is initialised
     to false in the bitmap, which is the default behaviour in the underlying
     ADT */
  size_t temp = block_size (block_device);
  size_t size = temp / SECTORS_PER_PAGE;
  if ((swap_slot_map = bitmap_create (size)) == NULL)
      PANIC ("Could not allocate memory for swap table");
  
}

size_t
pick_slot_and_swap (void *page)
{
  /* Finds the first slot which has value false (so it's free) and flips it,
     returning the index */
  size_t index = bitmap_scan_and_flip (swap_slot_map, 0, 1, false);

  /* Write the ith BLOCK_SECTOR_SIZE bytes of the page to the block device. We
     have a mapping of SECTORS_PER_PAGE block sectors for every bit in the
     bitmap, so we can just multiply the index we got back by SECTORS_PER_PAGE
     to find the correct block sector to write to, and then increment this
     block sector index in each loop iteration */
  int i = 0;
  for (; i < SECTORS_PER_PAGE; ++i)
    {
      block_write (block_device, (index * SECTORS_PER_PAGE) + i,
                   page + (BLOCK_SECTOR_SIZE * i));
    }
  return index;
}

void
free_slot (void *page, size_t index)
{
  /* Mark the slot as free again */
  ASSERT (bitmap_test (swap_slot_map, index));
  bitmap_flip (swap_slot_map, index);

  /* This is almost identical to the loop in pick_slot_and_swap, we're just
     going the other way */
  int i = 0;
  for (; i < SECTORS_PER_PAGE; ++i)
    {
      block_read (block_device, (index * SECTORS_PER_PAGE) + i,
                   page + (BLOCK_SECTOR_SIZE * i));
    }
}

void
destroy_swap_map (void)
{
  bitmap_destroy (swap_slot_map);
}