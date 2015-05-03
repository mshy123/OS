#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "threads/thread.h"
#include <list.h>
#include "devices/disk.h"

struct sup_page_table_entry {
    void *page;
    bool writable;

    disk_sector_t disk_num;
    struct list_elem elem;
};

void add_sup_page_table_entry (disk_sector_t disk_n, void *page, bool writable);
struct sup_page_table_entry *get_sup_page_table_entry (struct thread *current_t, void *uaddr);
bool load_page(struct sup_page_table_entry *spte);
void destroy_sup_page_table (struct thread *current_t);

#endif
