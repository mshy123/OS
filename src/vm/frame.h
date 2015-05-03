#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include <list.h>

struct list frame_table;

struct lock frame_lock;

struct frame_table_entry {
    void *frame;
    void *page;
    bool writable;

    struct thread *own_thread;
    struct list_elem elem;
};

void frame_table_init (void);
void *frame_alloc (enum palloc_flags flag, void *page, bool writable);
void add_frame_table (void *upage, void *kpage, bool writable);
void frame_free (struct thread *current_t);
void single_frame_free (void *frame);
void *evict_frame(enum palloc_flags flag);

#endif
