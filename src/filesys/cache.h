#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/disk.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "threads/thread.h"
#include <list.h>

#define MAX_CACHE_SIZE 64
#define BLOCK_SIZE 512
#define WRITE_BEHIND_INTERVAL 5 * TIMER_FREQ

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

void cache_init (void);
//void cache_read_ahead (disk_sector_t s);
//void read_ahead_thread (void *aux);
void cache_write_behind (bool halt);
void write_behind_thread (void *aux);
struct cache_entry *find_cache_block(disk_sector_t s);
void free_cache_block (disk_sector_t s);
struct cache_entry *cache_load(disk_sector_t s);
off_t cache_read_at (disk_sector_t s, void *buffer, off_t size, off_t offs);
off_t cache_write_at (disk_sector_t s, const void *buffer, off_t size, off_t offs);

#endif
