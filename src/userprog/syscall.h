#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

/* Project2 : Using in process.c function */
void remove_child_process_all(void);
void remove_all_file(void);
void munmap_all(void);

struct file *mapid_to_file (int mapid);
#endif /* userprog/syscall.h */
