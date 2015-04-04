#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void halt (void);
void exit (int status);
/*pid_t exec (const char *cmd_line);
int wait (int pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);*/
void check_valid_user_pointer(const void *user_pointer);
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

 printf("sysysys : %d\n", *(int *)f->esp);  
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
      break;

    case SYS_WAIT:                   /* Wait for a child process to die. */
      get_args(f, args, 1);
      break;

    case SYS_CREATE:                 /* Create a file. */
      get_args(f, args, 2);
      break;

    case SYS_REMOVE:                 /* Delete a file. */
      get_args(f, args, 1);
      break;

    case SYS_OPEN:                   /* Open a file. */
      get_args(f, args, 1);
      break;

    case SYS_FILESIZE:               /* Obtain a file's size. */
      get_args(f, args, 1);
      break;

    case SYS_READ:                   /* Read from a file. */
      get_args(f, args, 3);
      break;

    case SYS_WRITE:                  /* Write to a file. */
      get_args(f, args, 3);
      break;

    case SYS_SEEK:                   /* Change position in a file. */
      get_args(f, args, 2);
      break;

    case SYS_TELL:                   /* Report current position in a file. */
      get_args(f, args, 1);
      break;

    case SYS_CLOSE:                  /* Close a file. */
      get_args(f, args, 1);
      break;
  }

  printf ("system call!\n");
}

void check_valid_user_pointer(const void *user_pointer) {  
  struct thread *curr = thread_current ();
  uint32_t *pd;
  
  pd = curr->pagedir;

  if(user_pointer == NULL || !is_user_vaddr(user_pointer) || pagedir_get_page(pd, user_pointer) == NULL) 
    return;
    //exit(-1);
}

void halt(void) {
    power_off();
}

void exit (int status) {
    struct thread *t = thread_current();
    t->o_p->status = status;
    printf("Now entering exit\n");
    thread_exit();
    printf("Success to exit\n");
}

void get_args(struct intr_frame *f, int *args, int num) {
    int i;

    for(i = 1; i <= num; i++) {
        check_valid_user_pointer((const void *)(f->esp + i * 4));
        args[i - 1] = *((int *)f->esp + i);
    }
}

struct own_process *get_child_process(int pid) {
    struct thread *t = thread_current();
    struct list_elem *c_elem = list_begin(&t->child_list);
    struct own_process *c_p;

    while(c_elem != list_end(&t->child_list)) {
        c_p = list_entry(c_elem, struct own_process, elem);
        if(c_p->pid == pid) {
            return c_p;
        }
        c_elem = list_next(c_elem);
    }
    return NULL;
}

struct own_process *create_child_process(int pid) {
    struct own_process *o_p = malloc(sizeof(struct own_process));
    
    o_p->pid = pid;
    o_p->wait = false;
    o_p->exit = false;
    o_p->success_load = 0;

    list_push_back(&thread_current()->child_list, &o_p->elem);
    
    return o_p;
}

void remove_child_process_all(void) {
    struct thread *t = thread_current();
    struct list_elem *c_elem;

    while(!list_empty(&t->child_list)) {
        c_elem = list_pop_front(&t->child_list);
        free(list_entry(c_elem, struct own_process, elem));
    }
}
