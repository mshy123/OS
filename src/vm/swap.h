#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/disk.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <bitmap.h>

#define DISK_UNIT_SIZE (PGSIZE / DISK_SECTOR_SIZE)

void swap_init(void);
void swap_in(disk_sector_t sec_n, void *frame);
disk_sector_t swap_out(void *frame);
void reset_disk_sector (disk_sector_t sec_n);

#endif
