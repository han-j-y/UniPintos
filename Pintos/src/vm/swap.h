#ifndef SWAP_H
#define SWAP_H

#include <stddef.h>

void init_swap_structures (void);
size_t pick_slot_and_swap (void *page);
void free_slot (void *page, size_t index);
void destroy_swap_map (void);

#endif