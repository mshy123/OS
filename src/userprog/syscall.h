#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

struct own_process *get_child_process(int pid);
void remove_child_process_all(void);
struct own_process *create_child_process(int pid);


#endif /* userprog/syscall.h */
