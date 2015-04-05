#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void remove_child_process_all(void);
void remove_all_file(void);
struct thread *create_child_process(int pid);


#endif /* userprog/syscall.h */
