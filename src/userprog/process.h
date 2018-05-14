#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char*);
int process_wait (tid_t);
void process_exit (int);
void process_activate (void);
int allocate_id (void);
bool install_page (void*, void*, bool);

#endif /* userprog/process.h */
