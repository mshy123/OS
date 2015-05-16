#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

static void syscall_handler (struct intr_frame *);

/* Project2 : Syscall function */
void halt (void);
void exit (int status);
int exec (char *cmd_line);
int wait (int pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* Project3 : add mmap and munmap */
int mmap (int fd, void *addr);
void munmap (int mapping);

/* Project2 : additiona function */
void check_valid_user_pointer(const void *user_pointer, void *esp);
void check_valid_string (const void *user_pointer, void *esp);
struct file *fd_to_file (int fd);
int uaddr_to_kaddr(const void *uaddr);
void get_args(struct intr_frame *f, int *args, int num);

/* Project3 : check this page is writable */
void check_write (void *uaddr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid_user_pointer((const void *)f->esp, f->esp);
  
  int args[4];
  //printf("call : %d\n\n", *(int *)f->esp);
  switch(*(int *) f->esp) {
    case SYS_HALT:                   /* Halt the operating system. */
      halt();
      break;

    case SYS_EXIT:                   /* Terminate this process. */
      get_args(f, args, 1);
      exit(args[0]);
      break;

    case SYS_EXEC:                   /* Start another process. */ 
      get_args(f, args, 1);
      check_valid_string((const void *)args[0], f->esp);
          //hex_dump(pg_round_down(args[0]), pg_round_down(args[0]), PGSIZE, true);
          //printf("\n");
      f->eax = exec((char *)args[0]);
      break;

    case SYS_WAIT:                   /* Wait for a child process to die. */
      get_args(f, args, 1);
      f->eax = wait(args[0]);
      break;

    case SYS_CREATE:                 /* Create a file. */
      get_args(f, args, 2);
      check_valid_string((const void *)args[0], f->esp);  
      f->eax = create((const char *)args[0], (unsigned)args[1]);
      break;

    case SYS_REMOVE:                 /* Delete a file. */
      get_args(f, args, 1);
      check_valid_string((const void *)args[0], f->esp);  
      f->eax = remove((const char *)args[0]);
      break;

    case SYS_OPEN:                   /* Open a file. */
      get_args(f, args, 1);
      check_valid_string((const void *)args[0], f->esp);  
      f->eax = open((const char *)args[0]);
      break;

    case SYS_FILESIZE:               /* Obtain a file's size. */
      get_args(f, args, 1);
      f->eax = filesize(args[0]);
      break;

    case SYS_READ:                   /* Read from a file. */
      get_args(f, args, 3); 

      check_valid_user_pointer((const void *)args[1], f->esp);  
      check_valid_user_pointer((const void *)args[1] + args[2], f->esp);  
      check_write((void *)args[1]);
      f->eax = read(args[0], (void *)args[1], (unsigned)args[2]);
      break;

    case SYS_WRITE:                  /* Write to a file. */
      get_args(f, args, 3);
      check_valid_user_pointer((const void *)args[1], f->esp);  
      check_valid_user_pointer((const void *)args[1] + args[2], f->esp);  
      f->eax = write(args[0], (const void *)args[1], (unsigned)args[2]);
      break;

    case SYS_SEEK:                   /* Change position in a file. */
      get_args(f, args, 2);
      seek(args[0], (unsigned)args[1]);
      break;

    case SYS_TELL:                   /* Report current position in a file. */
      get_args(f, args, 1);
      f->eax = tell(args[0]);
      break;

    case SYS_CLOSE:                  /* Close a file. */
      get_args(f, args, 1);
      close(args[0]);
      break;
    
    case SYS_MMAP:
      get_args(f, args, 2);
      f->eax = mmap(args[0], (void *)args[1]);
      break;

    case SYS_MUNMAP:
      get_args(f, args, 1);
      munmap(args[0]);
      break;
  }
}

void check_valid_user_pointer(const void *user_pointer, void *esp) {  
  struct thread *curr = thread_current ();
  void *uaddr = pg_round_down(user_pointer);
  uint32_t *pd;
  
  pd = curr->pagedir;

  lock_acquire(&frame_lock);
  lock_release(&frame_lock);

  //if(user_pointer == NULL || is_kernel_vaddr(user_pointer) || pagedir_get_page(pd, user_pointer) == NULL) exit(-1);
  if(user_pointer == NULL || is_kernel_vaddr(user_pointer)) exit(-1);

  if(pagedir_get_page(pd, user_pointer) == NULL) {
      bool success = false;
      struct sup_page_table_entry *spte = get_sup_page_table_entry(curr, uaddr);
      if (spte != NULL) {
          success = load_page (spte);
      }
      else if (user_pointer >= esp - 32) {
          if ((size_t) (PHYS_BASE - uaddr) > (1 << 23)) exit (-1);
          uint8_t *frame = frame_alloc(PAL_USER | PAL_ZERO, uaddr, true);

          if (frame != NULL) {
              if (!(success = (pagedir_get_page (curr->pagedir, uaddr) == NULL && pagedir_set_page (curr->pagedir, uaddr, frame, true)))) {
                  single_frame_free (frame);
              }
          }
      }
      if (!success) {
          exit(-1);
      }
  }
}

void check_valid_string (const void *user_pointer, void *esp) {
  char *str = (char *)user_pointer;
  check_valid_user_pointer (user_pointer, esp);
  void *bottom = pg_round_down(user_pointer);
  while (bottom <= str) {
      //printf("aaa\n");
      bottom += 1;
      check_valid_user_pointer((const void *)bottom, esp);
  }
  while (*str != 0) {
      str = str + 1;
      check_valid_user_pointer ((const void *)str, esp);
  }  
}

void halt(void) {
    power_off();
}

void exit (int status) {
    struct thread *t = thread_current();
    t->exit_status = status;
    thread_exit();
}

int exec (char *cmd_line) {
    return (int)process_execute(cmd_line);
}

int wait (int pid) {
    return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) {
    bool success = filesys_create(file, initial_size);
    return success;
}

bool remove (const char *file) {
    bool success = filesys_remove(file);
    return success;
}

int open (const char *file) {
    struct thread *t = thread_current();
    struct file *fp = filesys_open(file);
    
    if (fp == NULL) {
        return -1;
    }
    
    struct file_info *fi = malloc (sizeof(struct file_info));
    
    fi->fd = t->max_fd++;
    fi->file = fp;
    list_push_back(&t->file_list, &fi->elem);

    return fi->fd;
}

int filesize (int fd) {
    struct file *f;
    int size = -1;

    f = fd_to_file(fd);

    size = file_length(f);
    return size;
}

int read (int fd, void *buffer, unsigned size) {
    if(fd == 0) {
        unsigned i;
        uint8_t *local_buf = (uint8_t *)buffer;

        for(i = 0; i < size; i++) local_buf[i] = input_getc();
        
        return size;
    }
    else if(fd == 1) {
        return -1;
    }
    else {
        struct file *f = fd_to_file(fd);
        
        if(f == NULL) { 
            return -1;
        }

        int read_bytes = file_read(f, buffer, size);
        return read_bytes;
    }
    return -1;
}

int write (int fd, const void *buffer, unsigned size) {
    if(fd == 1) {
        putbuf(buffer, size);
        return size;
    }
    else if(fd == 0) {
      return -1;
    }
    else {
        struct file *f = fd_to_file(fd);

        if(f == NULL) {
            return -1;
        }

        int write_bytes = file_write(f, buffer, size);
        return write_bytes;
    }
    return -1;
}

void seek (int fd, unsigned position) {
    struct file *f = fd_to_file(fd);

    if(f == NULL) {
        return;
    }
    file_seek(f, position);
}

unsigned tell (int fd) {
    struct file *f = fd_to_file(fd);

    if(f == NULL) {
        return -1;
    }
    off_t offset = file_tell(f);
    return offset;
}

void close (int fd) {
    struct thread *t = thread_current();
    struct list_elem *f_elem = list_begin(&t->file_list);
    struct file_info *fi;

    while(f_elem != list_end(&t->file_list)) {
        fi = list_entry(f_elem, struct file_info, elem);
        if(fi->fd == fd) {
            file_close(fi->file);
            list_remove(f_elem);
            free(fi);
            break;
        }
        f_elem = list_next(f_elem);
    }
}

int mmap (int fd, void *addr) {
    struct file *f = fd_to_file(fd);
    struct thread *t = thread_current();
    if (fd == 0 || fd == 1 || addr == 0 || f == NULL || is_kernel_vaddr(addr)) {
       return -1;
    }
    if(pagedir_get_page(t->pagedir, addr) != NULL) {
      return -1;
    }
    //if((size_t) (PHYS_BASE - addr) > (1 << 23) || ((uint32_t) addr % PGSIZE) != 0) {
    if((uint32_t) addr % PGSIZE != 0) {
      return -1;
    }
    
    void *uaddr = pg_round_down(addr);
    struct sup_page_table_entry *spte = get_sup_page_table_entry(t, uaddr);
    if (spte != NULL) 
      return -1;
    struct file *mmap_file = file_reopen(f);
    if(mmap_file == NULL || file_length(mmap_file) == 0)
      return -1;

    t->mapid++;

    struct mmap_file_info *mfi = malloc(sizeof(struct mmap_file_info));
    mfi->mapid = t->mapid;
    mfi->file = mmap_file; 
    list_push_back(&thread_current()->mmap_list, &mfi->elem);
    
    off_t ofs = 0;
    uint32_t read_bytes = file_length(mmap_file);
    while (read_bytes > 0) {        
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      if (!add_mmap_file_page_table_entry (t->mapid, mmap_file, ofs, addr, page_read_bytes, page_zero_bytes)) {
        munmap(t->mapid); 
        return -1;
      }

      read_bytes -= page_read_bytes;
      ofs += page_read_bytes;
      addr += PGSIZE;
    }
 
    return t->mapid;
}

void munmap (int mapping) {
    struct thread *t = thread_current();
    struct list_elem *m_elem = list_begin(&t->mmap_list);
    struct list_elem *f_elem = list_begin(&frame_table);
    struct list_elem *n_elem = list_next(f_elem);
    struct mmap_file_info *mfi;
    struct frame_table_entry *fte;

    while(f_elem != list_end(&frame_table)) {
        fte = list_entry(f_elem, struct frame_table_entry, elem);
        if(fte->own_thread == t && fte->mapping && fte->mapid == mapping) {
            if(pagedir_is_dirty(t->pagedir, fte->page)) {
                file_write_at(mapid_to_file(fte->mapid), fte->frame, fte->read_bytes, fte->ofs);
            }
            single_frame_free(fte->frame);
        }
        f_elem = n_elem;
        if(f_elem != list_end(&frame_table)) n_elem = list_next(f_elem);
    }

    struct list_elem *s_elem = list_begin(&t->sup_page_table);
    if(s_elem != list_end(&t->sup_page_table)) n_elem = list_next(s_elem);
    struct sup_page_table_entry *spte;
    while(s_elem != list_end(&t->sup_page_table)) {
        spte = list_entry(s_elem, struct sup_page_table_entry, elem);
        list_remove(s_elem);
        free(spte);
        s_elem = n_elem;
        if(s_elem != list_end(&t->sup_page_table)) n_elem = list_next(s_elem);
    }

    while(m_elem != list_end(&t->mmap_list)) {
        mfi = list_entry(m_elem, struct mmap_file_info, elem);
        if(mfi->mapid == mapping) {
            file_close(mfi->file);
            list_remove(&mfi->elem);
            free(mfi);
            break;
        }
        m_elem = list_next(m_elem);
    }
}

void munmap_all (void) {
    struct thread *t = thread_current();
    struct list_elem *m_elem;
    struct mmap_file_info *mfi;

    while(!list_empty(&t->mmap_list)) {
        m_elem = list_begin(&t->mmap_list);
        mfi = list_entry(m_elem, struct mmap_file_info, elem);
        munmap(mfi->mapid);
    }
}
struct file *mapid_to_file (int mapid) {
    struct thread *t = thread_current();
    struct list_elem *m_elem = list_begin(&t->mmap_list);
    struct mmap_file_info *mfi;

    while(m_elem != list_end(&t->mmap_list)) {
        mfi = list_entry(m_elem, struct mmap_file_info, elem);
        if(mfi->mapid == mapid) {
            ASSERT(mfi->file != NULL);
            return mfi->file;
        }
        m_elem = list_next(m_elem);
    }
    return NULL;
}

struct file *fd_to_file (int fd) {
    struct thread *t = thread_current();
    struct list_elem *f_elem = list_begin(&t->file_list);
    struct file_info *fi;

    while(f_elem != list_end(&t->file_list)) {
        fi = list_entry(f_elem, struct file_info, elem);
        if(fi->fd == fd) {
            return fi->file;
        }
        f_elem = list_next(f_elem);
    }
    return NULL;
}

void get_args(struct intr_frame *f, int *args, int num) {
    int i;

    for(i = 1; i <= num; i++) {
        check_valid_user_pointer((const void *)(f->esp + i * 4), f->esp);
        args[i - 1] = *((int *)f->esp + i);
    }
}

int uaddr_to_kaddr(const void *user_pointer) {
    void *kaddr = pagedir_get_page(thread_current()->pagedir, user_pointer);

    if(kaddr == NULL) exit(-1);

    return (int) kaddr;
}

void remove_all_file(void) {
    struct thread *t = thread_current();
    struct file_info *fi;
    struct list_elem *f_elem;

    while(!list_empty(&t->file_list)) {
        f_elem = list_pop_front(&t->file_list);
        fi = list_entry(f_elem, struct file_info, elem);
        file_close(fi->file);
        free(fi);
    }
}

void check_write (void *uaddr) {
    struct thread *t = thread_current();
    struct frame_table_entry *fte;
    struct list_elem *f_elem = list_begin(&frame_table);

    while(f_elem != list_end(&frame_table)) {
        fte = list_entry (f_elem, struct frame_table_entry, elem);
        if (fte->own_thread->tid == t->tid && fte->page == pg_round_down(uaddr)) {
            if (fte->writable == false) exit (-1);
            else return;
        }
        f_elem = list_next(f_elem);
    }
}

void remove_child_process_all(void) {
    struct thread *t = thread_current();
    struct thread *c_p;
    struct list_elem *c_elem;

    while(!list_empty(&t->child_list)) {
        c_elem = list_pop_front(&t->child_list);
        c_p = list_entry(c_elem, struct thread, c_elem);
        sema_up(&c_p->p_sema);
    }
}

