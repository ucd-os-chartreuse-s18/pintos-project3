#include "swap.h"
#include "devices/block.h"

static struct block *swap;

//see devices/block.c for more information
void init_swap_block (void) {
    swap = block_get_role (BLOCK_SWAP);
}