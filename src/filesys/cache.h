#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/disk.h"
#include "threads/timer.h"
#include "threads/synch.h"
#include <list.h>

#define MAX_CACHE_SIZE 64
#define BLOCK_SIZE 512

struct list cache_list;
uint32_t cache_size;
struct lock cache_lock;

struct cache_entry {    
    disk_sector_t sector;
    uint8_t block[BLOCK_SIZE];
    bool dirty;
    bool access;
    int open_cnt;
    struct list_elem elem;
};

#endif
