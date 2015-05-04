#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

void remove_sup_page_table_entry (struct sup_page_table_entry *spte);

struct sup_page_table_entry *get_sup_page_table_entry (struct thread *current_t, void *uaddr) {
    lock_acquire(&thread_current()->sup_page_table_lock);
    struct list_elem *spt_elem = list_begin(&current_t->sup_page_table);
    struct sup_page_table_entry *spte;

    while (spt_elem != list_end(&current_t->sup_page_table)) {
        spte = list_entry(spt_elem, struct sup_page_table_entry, elem);
        if (spte->page == uaddr) {
            lock_release(&thread_current()->sup_page_table_lock);
            return spte;
        }
        spt_elem = list_next(spt_elem);
    }
    lock_release(&thread_current()->sup_page_table_lock);

    return NULL;
}

void remove_sup_page_table_entry (struct sup_page_table_entry *spte) {
    lock_acquire(&thread_current()->sup_page_table_lock);
    list_remove(&spte->elem);
    free(spte);
    lock_release(&thread_current()->sup_page_table_lock);
}

void destroy_sup_page_table (struct thread *current_t) {
    struct list_elem *spt_elem;
    struct sup_page_table_entry *spte;

    lock_acquire(&thread_current()->sup_page_table_lock);
    while (!list_empty(&current_t->sup_page_table)) {
        spt_elem = list_pop_front(&current_t->sup_page_table);
        spte = list_entry(spt_elem, struct sup_page_table_entry, elem);
        if(spte->type == 1) reset_disk_sector(spte->disk_num);
        free(spte);
    }
    lock_release(&thread_current()->sup_page_table_lock);
}

bool load_page (struct sup_page_table_entry *spte) {
    struct thread *t = thread_current();
    uint8_t *frame;
    bool success = false;

    if(spte->type == 1) {
        frame = frame_alloc (PAL_USER | PAL_ZERO, spte->page, spte->writable);
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
    }

    else if(spte->type == 2) {
        enum palloc_flags flag = (PAL_USER | PAL_ZERO);
        frame = frame_alloc (flag, spte->page, spte->writable);
        if (frame == NULL) {
            return success;
        }
        if (spte->read_bytes > 0) {
            lock_acquire(&thread_current()->sup_page_table_lock);
            if ((int) spte->read_bytes != file_read_at (spte->file, frame, spte->read_bytes, spte->offset)) {
                lock_release(&thread_current()->sup_page_table_lock);
                single_frame_free(frame);
                return success;
            }
            lock_release(&thread_current()->sup_page_table_lock);
            memset (frame + spte->read_bytes, 0, spte->zero_bytes);
        }
        success = (pagedir_get_page (t->pagedir, spte->page) == NULL && pagedir_set_page (t->pagedir, spte->page, frame, spte->writable));
        if(!success) {
            single_frame_free(frame);
            return success;
        }
        remove_sup_page_table_entry(spte);
    }

    return success;
}

bool add_file_page_table_entry (struct file *file, int32_t offset, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
    struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
    if(spte == NULL) return false;
    lock_acquire(&thread_current()->sup_page_table_lock);
    spte->type = 2;
    spte->file = file;
    spte->offset = offset;
    spte->page = upage;
    spte->read_bytes = read_bytes;
    spte->zero_bytes = zero_bytes;
    spte->writable = writable;
    list_push_back (&thread_current()->sup_page_table, &spte->elem);
    lock_release(&thread_current()->sup_page_table_lock);
    return true;
}

void add_sup_page_table_entry (disk_sector_t disk_n, void *page, bool writable) {
    struct sup_page_table_entry *spte = malloc(sizeof(struct sup_page_table_entry));
    lock_acquire(&thread_current()->sup_page_table_lock);
    spte->type = 1;
    spte->disk_num = disk_n;
    spte->page = page;
    spte->writable = writable;
    list_push_back (&thread_current()->sup_page_table, &spte->elem);
    lock_release(&thread_current()->sup_page_table_lock);
}
