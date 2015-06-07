#include "filesys/cache.h"
#include "threads/malloc.h"

void cache_init (void) {
    list_init(&cache_list);
    lock_init(&cache_lock);
    cache_size = 0;
    thread_create("cache_write_behind_thread", PRI_MIN, write_behind_thread, NULL);
}
/*
void cache_read_ahead (disk_sector_t s) {
    disk_sector_t *args = malloc(sizeof(disk_sector_t));
    if (args != NULL) {
        *args = s + 1;
        thread_create("cache_read_ahead_thread", PRI_MIN, read_ahead_thread, args);
    }
}

void read_ahead_thread (void *aux) {
    disk_sector_t s = *(disk_sector_t *)aux;
    struct cache_entry *ce = find_cache_block(s);
    if (ce == NULL) {
        ce = cache_load(s);
        ce->open_cnt = 0;
    }
    free(aux);
}
*/
void cache_write_behind (bool halt) {
    struct list_elem *elem = list_begin(&cache_list);
    struct list_elem *next;
    struct cache_entry *ce;

    while (elem != list_end(&cache_list)) {
        next = list_next(elem);
        ce = list_entry(elem, struct cache_entry, elem);
        if (ce->dirty) {
            disk_write(filesys_disk, ce->sector, (void *)&ce->block);
            ce->dirty = false;
        }
        if (halt) {
            list_remove(elem);
            free(ce);
        }
        elem = next;
    }
}

void write_behind_thread (void *aux UNUSED) {
    while (true) {
        timer_sleep(WRITE_BEHIND_INTERVAL);
        cache_write_behind(false);
    }
}

struct cache_entry *find_cache_block(disk_sector_t s) {
    struct list_elem *elem = list_begin(&cache_list);
    struct cache_entry *ce;

    while (elem != list_end(&cache_list)) {
        ce = list_entry(elem, struct cache_entry, elem);
        if (ce->sector == s) {
            return ce;
        }
        elem = list_next(elem);
    }
    
    return NULL;
}

void free_cache_block (disk_sector_t s) {
    struct list_elem *elem = list_begin(&cache_list);
    struct cache_entry *ce;

    while (elem != list_end(&cache_list)) {
        ce = list_entry(elem, struct cache_entry, elem);
        if (ce->sector == s) {
            list_remove(elem);
            free(ce);
            break;
        }
        elem = list_next(elem);
    }
}

struct cache_entry *cache_load(disk_sector_t s) {
    struct cache_entry *c = find_cache_block(s);
    
    if (c != NULL) {
        c->access = true;
        c->open_cnt++;
        return c;
    }
    else {
        if (cache_size >= MAX_CACHE_SIZE) { //evict cache using FIFO
            struct list_elem *elem = list_front(&cache_list);
            while (true) {
                struct cache_entry *ce = list_entry(elem, struct cache_entry, elem);
                if (ce->open_cnt <= 0)  { //evict when cache block is not use
                    if (ce->dirty) {
                        disk_write(filesys_disk, ce->sector, (void *)&ce->block);
                    }
                    list_remove(&ce->elem);
                    free(ce);
                    cache_size--;
                    break;
                }
                elem = list_next(elem);
                if (elem == list_end(&cache_list)) {
                    elem = list_front(&cache_list);
                }
            }
        }
        
        cache_size++;
        c = malloc(sizeof(struct cache_entry));
        if (c == NULL) {
            cache_size--;
            return NULL;
        }
        c->sector = s;
        disk_read(filesys_disk, s, (void *)&c->block);
        c->dirty = false;
        c->access = true;
        c->open_cnt = 1;
        list_push_back(&cache_list, &c->elem);
    }
    
    return c;
}

off_t cache_read_at (disk_sector_t s, void *buffer, off_t size, off_t offs) {
    struct cache_entry *ce = cache_load(s);

    if (ce == NULL) {
        return -1;
    }
    memcpy(buffer, ce->block + offs, size);
    ce->open_cnt--; 
    ce->access = true;

    return size;
}

off_t cache_write_at (disk_sector_t s, const void *buffer, off_t size, off_t offs) {
    struct cache_entry *ce = cache_load(s);
    
    if (ce == NULL) {
        return -1;
    }
    memcpy(ce->block + offs, buffer, size);
    ce->open_cnt--;
    ce->access = true;
    ce->dirty = true;

    return size;
}
