#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

void remove_sup_page_table_entry (struct sup_page_table_entry *spte);

struct sup_page_table_entry *get_sup_page_table_entry (struct thread *current_t, void *uaddr) {
    struct list_elem *spt_elem = list_begin(&current_t->sup_page_table);
    struct sup_page_table_entry *spte;

    while (spt_elem != list_end(&current_t->sup_page_table)) {
        spte = list_entry(spt_elem, struct sup_page_table_entry, elem);
        if (spte->page == uaddr) {
            return spte;
        }
        spt_elem = list_next(spt_elem);
    }

    return NULL;
}

void remove_sup_page_table_entry (struct sup_page_table_entry *spte) {
    list_remove(&spte->elem);
    free(spte);
}

void destroy_sup_page_table (struct thread *current_t) {
    struct list_elem *spt_elem;
    struct sup_page_table_entry *spte;

    while (!list_empty(&current_t->sup_page_table)) {
        spt_elem = list_pop_front(&current_t->sup_page_table);
        spte = list_entry(spt_elem, struct sup_page_table_entry, elem);
        reset_disk_sector(spte->disk_num);
        free(spte);
    }
}

bool load_page (struct sup_page_table_entry *spte) {
    struct thread *t = thread_current();
    bool success = false;

    uint8_t *frame = frame_alloc (PAL_USER | PAL_ZERO, spte->page, spte->writable);
    if (frame == NULL) {
        return success;
    }
    success = (pagedir_get_page (t->pagedir, spte->page) == NULL && pagedir_set_page (t->pagedir, spte->page, frame, spte->writable));
    if (!success) {
        single_frame_free (frame);
    }
    else {
        swap_in(spte->disk_num, frame);
        remove_sup_page_table_entry(spte);
    }

    return success;
}

void add_sup_page_table_entry (disk_sector_t disk_n, void *page, bool writable) {
    struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
    spte->disk_num = disk_n;
    spte->page = page;
    spte->writable = writable;
    list_push_back (&thread_current()->sup_page_table, &spte->elem);
}
