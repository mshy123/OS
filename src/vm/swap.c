#include <bitmap.h>
#include "devices/disk.h"
#include "vm/swap.h"
#include "vm/frame.h"

struct disk *swap_disk;
struct bitmap *swap_table;
struct lock swap_lock;

void swap_init(void) {
    lock_init(&swap_lock);
    swap_disk = disk_get(1,1);
    ASSERT(swap_disk != NULL);
    disk_sector_t swap_size = disk_size(swap_disk);
    swap_table = bitmap_create(swap_size/DISK_UNIT_SIZE);
    bitmap_set_all(swap_table, true);
}

disk_sector_t swap_out(void *frame) {
    int i;
 
    lock_acquire(&swap_lock);
    size_t idx = bitmap_scan_and_flip(swap_table, 0, 1, true);
    
    if (idx == BITMAP_ERROR) PANIC ("swap disk is already full.");

    for(i=0; i<DISK_UNIT_SIZE; i++) {
        disk_write(swap_disk, (disk_sector_t)idx*DISK_UNIT_SIZE + i, (const void *)(frame + i*DISK_SECTOR_SIZE));
    }
    lock_release(&swap_lock);

    return (disk_sector_t)idx;
}

void swap_in(disk_sector_t sec_n, void *frame) {
    int i;

    lock_acquire(&swap_lock);
    bitmap_flip(swap_table, (size_t)sec_n);

    for(i=0; i<DISK_UNIT_SIZE; i++) {
        lock_acquire(&frame_lock);
        disk_read(swap_disk, (disk_sector_t)sec_n*DISK_UNIT_SIZE + i, (void *)(frame + i*DISK_SECTOR_SIZE));
        lock_release(&frame_lock);
    }

    lock_release(&swap_lock);
}

void reset_disk_sector (disk_sector_t sec_n) {
    lock_acquire(&swap_lock);
    bitmap_flip(swap_table, (size_t)sec_n);
    lock_release(&swap_lock);
}
