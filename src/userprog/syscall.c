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

/* Project2 : additiona function */
void check_valid_user_pointer(const void *user_pointer);
struct file *fd_to_file (int fd);
void get_args(struct intr_frame *f, int *args, int num);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid_user_pointer((const void *)f->esp);
  
  int args[4];

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
      check_valid_user_pointer((const void *)args[0]);  
      f->eax = exec((char *)args[0]);
      break;

    case SYS_WAIT:                   /* Wait for a child process to die. */
      get_args(f, args, 1);
      f->eax = wait(args[0]);
      break;

    case SYS_CREATE:                 /* Create a file. */
      get_args(f, args, 2);
      check_valid_user_pointer((const void *)args[0]);  
      f->eax = create((const char *)args[0], (unsigned)args[1]);
      break;

    case SYS_REMOVE:                 /* Delete a file. */
      get_args(f, args, 1);
      check_valid_user_pointer((const void *)args[0]);  
      f->eax = remove((const char *)args[0]);
      break;

    case SYS_OPEN:                   /* Open a file. */
      get_args(f, args, 1);
      check_valid_user_pointer((const void *)args[0]);  
      f->eax = open((const char *)args[0]);
      break;

    case SYS_FILESIZE:               /* Obtain a file's size. */
      get_args(f, args, 1);
      f->eax = filesize(args[0]);
      break;

    case SYS_READ:                   /* Read from a file. */
      get_args(f, args, 3);
      check_valid_user_pointer((const void *)args[1]);  
      f->eax = read(args[0], (void *)args[1], (unsigned)args[2]);
      break;

    case SYS_WRITE:                  /* Write to a file. */
      get_args(f, args, 3);
      check_valid_user_pointer((const void *)args[1]);  
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
  }
}

void check_valid_user_pointer(const void *user_pointer) {  
  struct thread *curr = thread_current ();
  uint32_t *pd;
  
  pd = curr->pagedir;

  if(user_pointer == NULL || is_kernel_vaddr(user_pointer) || pagedir_get_page(pd, user_pointer) == NULL) exit(-1);
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
    return filesys_create(file, initial_size);
}

bool remove (const char *file) {
    return filesys_remove(file);
}

int open (const char *file) {
    struct thread *t = thread_current();
    struct file *fp = filesys_open(file);
    
    if (fp == NULL) return -1;
    
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
        
        if(f == NULL) return -1;

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

        if(f == NULL) return -1;

        int write_bytes = file_write(f, buffer, size);
        return write_bytes;
    
    }
    return -1;
}

void seek (int fd, unsigned position) {
    struct file *f = fd_to_file(fd);

    if(f == NULL) return;

    file_seek(f, position);
}

unsigned tell (int fd) {
    struct file *f = fd_to_file(fd);

    if(f == NULL) return -1;

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
        check_valid_user_pointer((const void *)(f->esp + i * 4));
        args[i - 1] = *((int *)f->esp + i);
    }
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

