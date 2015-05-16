#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

/* Project3-1: initialize frame table list */
void frame_table_init (void) {
    list_init(&frame_table);
    lock_init(&frame_lock);
}

void *frame_alloc (enum palloc_flags flag, void *page, bool writable) {
    void *frame;

    if ((flag & PAL_USER) == 0) return NULL;
    frame = palloc_get_page(flag);
    if (frame != NULL) {
        add_frame_table(page, frame, writable);
    }
    else {
        frame = evict_frame(flag);
        lock_release(&frame_lock);
        ASSERT(frame != NULL);
        if (frame != NULL) add_frame_table(page, frame, writable);
    }

    return frame;
}


void *frame_alloc_for_mmap (enum palloc_flags flag, void *page, int mapid, int32_t ofs, uint32_t read_bytes, uint32_t zero_bytes) {
    void *frame;

    if ((flag & PAL_USER) == 0) return NULL;
    frame = palloc_get_page(flag);
    if (frame != NULL) {
        add_frame_table_for_mmap(page, frame, mapid, ofs, read_bytes, zero_bytes);
    }
    else {
        frame = evict_frame(flag);
        lock_release(&frame_lock);
        ASSERT(frame != NULL);
        if (frame != NULL)
          add_frame_table_for_mmap(page, frame, mapid, ofs, read_bytes, zero_bytes);
    }

    return frame;
}

/* Project3-1: make frame table entry and add it into the frame table list */
void add_frame_table (void *upage, void *kpage, bool writable) {
    struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
    fte->mapping = false;
    fte->mapid = 0;
    fte->frame = kpage;
    fte->page = upage;
    fte->writable = writable;
    fte->own_thread = thread_current();
    lock_acquire(&frame_lock);
    list_push_back(&frame_table, &fte->elem);
    lock_release(&frame_lock);
}

void add_frame_table_for_mmap (void *upage, void *kpage, int mapid, int32_t ofs, uint32_t read_bytes, uint32_t zero_bytes) {
    struct frame_table_entry *fte = malloc(sizeof(struct frame_table_entry));
    fte->frame = kpage;
    fte->page = upage;
    fte->mapid = mapid;
    fte->ofs = ofs;
    fte->read_bytes = read_bytes;
    fte->zero_bytes = zero_bytes;
    fte->mapping = true;
    fte->writable = true;
    fte->own_thread = thread_current();
    lock_acquire(&frame_lock);
    list_push_back(&frame_table, &fte->elem);
    lock_release(&frame_lock);
}

/* Project3-1: remove all the frame table entries of current thread */
void frame_free (struct thread *current_t) {
    lock_acquire(&frame_lock);
    struct frame_table_entry *fte;
    struct list_elem *f_elem = list_begin(&frame_table);
    struct list_elem *next_f_elem;

    while(f_elem != list_end(&frame_table)) {
        fte = list_entry(f_elem, struct frame_table_entry, elem);
        next_f_elem = list_next(f_elem);
        if(fte->own_thread->tid == current_t->tid) {
            list_remove(f_elem);
            pagedir_clear_page(current_t->pagedir, fte->page);
            palloc_free_page(fte->frame);
            free(fte);
        }
        f_elem = next_f_elem;
    }
    lock_release(&frame_lock);
}

void single_frame_free (void *frame) {
    lock_acquire(&frame_lock);
    struct frame_table_entry *fte;
    struct list_elem *f_elem = list_begin(&frame_table);
    
    while(f_elem != list_end(&frame_table)) {
        fte = list_entry(f_elem, struct frame_table_entry, elem);
        if (fte->frame == frame) {
            list_remove(f_elem);
            pagedir_clear_page(thread_current()->pagedir, fte->page);
            palloc_free_page(frame);
            free(fte);
            break;
        }
        f_elem = list_next(f_elem);
    }
    lock_release(&frame_lock);
}

void *evict_frame(enum palloc_flags flag) {
    struct list_elem *elem = list_begin(&frame_table);


    lock_acquire(&frame_lock);

    while (true) {
        struct frame_table_entry *fte = list_entry(elem, struct frame_table_entry, elem);

        struct thread *t = fte->own_thread;

        if (pagedir_is_accessed(t->pagedir, fte->page)) {
            pagedir_set_accessed(t->pagedir, fte->page, false);
        }
        else {
            if (fte->mapping) { 
                add_mmap_file_page_table_entry (fte->mapid, mapid_to_file(fte->mapid), fte->ofs, fte->page, fte->read_bytes, fte->zero_bytes);
                if(pagedir_is_dirty(t->pagedir, fte->page)) {
                    file_write_at(mapid_to_file(fte->mapid), fte->page, fte->read_bytes, fte->ofs);
                } 
                pagedir_clear_page(t->pagedir, fte->page);
                pagedir_set_dirty(t->pagedir, fte->page, false);
                palloc_free_page(fte->frame);
                free(fte);
                return palloc_get_page(flag);
            }
            else {
                disk_sector_t disk_n = swap_out(fte->frame);
                add_sup_page_table_entry (disk_n, fte->page, fte->writable);
                list_remove(&fte->elem);
                pagedir_clear_page(t->pagedir, fte->page);
                palloc_free_page(fte->frame);
                free(fte);
                return palloc_get_page(flag);
            }
        }
        elem = list_next(elem);
        if (elem == list_end(&frame_table)) {
            elem = list_begin(&frame_table);
        }
    }
}

