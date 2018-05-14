#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <../lib/user/syscall.h>

void syscall_init (void);
void sys_munmap (mapid_t mapid);

typedef int syscall_func(int, int, int);

#endif /* userprog/syscall.h */
