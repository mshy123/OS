#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "threads/thread.h"
#include <list.h>
#include "devices/disk.h"

struct sup_page_table_entry {
    int type;
    void *page;
    bool writable;

    /* for swap */
    disk_sector_t disk_num;
    struct list_elem elem;

    /* for file */
    struct file *file;
    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    /* for mmap */
    int mapid;
};

void add_sup_page_table_entry (disk_sector_t disk_n, void *page, bool writable);
bool add_file_page_table_entry (struct file *file, int32_t offset, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool add_mmap_file_page_table_entry (int mapid, struct file *file, int32_t offset, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes);
struct sup_page_table_entry *get_sup_page_table_entry (struct thread *current_t, void *uaddr);
bool load_page(struct sup_page_table_entry *spte);
void destroy_sup_page_table (struct thread *current_t);

#endif
