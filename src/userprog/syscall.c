#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "userprog/stack.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <keyed_hash.h>
#include <hash.h>

static void syscall_handler (struct intr_frame *);

/* SYSTEM CALL PROTOTYPES */

static int sys_unimplemented(void);

/* Projects 2 and later. */
static void sys_halt (void);
static int sys_exit (int status);
static int sys_exec (const char *file);
static int sys_wait (pid_t pid);
static int sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *file);
static int sys_filesize (int fd);
static int sys_read (int fd, void *buffer, unsigned size);
static int sys_write (int fd, const void *buffer, unsigned size);
static void sys_seek (int fd, unsigned position);
static int sys_tell (int fd);
static void sys_close (int fd);

/* Project 3 and optionally project 4. */
static int sys_mmap (int fd, void *addr);
static int sys_munmap (mapid_t mapid);

/* Project 4 only. */
static int sys_chdir (const char *dir);
static int sys_mkdir (const char *dir);
static int sys_readdir (int fd, char name[READDIR_MAX_LEN + 1]);
static int sys_isdir (int fd);
static int sys_inumber (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  /*
  //This does not appear to be what we're doing. If we did this, the function
  //would instead need to return void and have `intr_frame` as its argument.
  //In that case, we would have each function store the syscall status in the
  //eax register, and then intr_register_int would have to dispatch by calling
  //INT N and then intrrupt.c would handle calling the function?
  intr_register_int (SYS_HALT, 3, INTR_ON, (syscall_func) sys_halt, "halt");
  intr_register_int (SYS_EXIT, 3, INTR_ON, sys_exit, "exit");
  intr_register_int (SYS_EXEC, 3, INTR_ON, sys_exec, "exec");
  intr_register_int (SYS_WAIT, 3, INTR_ON, sys_wait, "wait");
  intr_register_int (SYS_CREATE, 3, INTR_ON, sys_create, "create");
  intr_register_int (SYS_REMOVE, 3, INTR_ON, sys_remove, "remove");
  intr_register_int (SYS_OPEN, 3, INTR_ON, sys_open, "open");
  intr_register_int (SYS_FILESIZE, 3, INTR_ON, sys_filesize, "filesize");
  intr_register_int (SYS_READ, 3, INTR_ON, sys_read, "read");
  intr_register_int (SYS_WRITE, 3, INTR_ON, sys_write, "write");
  intr_register_int (SYS_SEEK, 3, INTR_ON, sys_seek, "seek");
  intr_register_int (SYS_TELL, 3, INTR_ON, sys_tell, "tell");
  intr_register_int (SYS_CLOSE, 3, INTR_ON, sys_close, "close");
  */
}

static int
syscall_argc (int sys_number) {
  
  switch (sys_number) {
    case SYS_HALT:
      return 0;
    
    case SYS_EXIT:
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_TELL:
    case SYS_CLOSE:
      return 1;
    
    case SYS_CREATE:
    case SYS_SEEK:
      return 2;
    
    case SYS_READ:
    case SYS_WRITE:
      return 3;
    
    default:
      return -1;
  }
}

/* We will not edit `src/lib/user/syscall.c`, but just this file. That file will
 * JUST push arguments for interrupt 0x30, which as you can see it registerd
 * above, is just the below method. */
static void
syscall_handler (struct intr_frame *f) 
{
  uint8_t *esp = f->esp;
    
  /* The test `sc-bad-sp` is very explicit that the stack shouldn't be below
   * the instruction pointer. However, to be safe I'm making sure that esp
   * is not in the entire page. There is no lower bound for the page because
   * the page allocated for eip is at the very bottom of the program's user
   * space, so there shouldn't be no other reference below that.*/
  if ((void*) esp <= pg_round_up(f->eip)) {
    sys_exit (-1);
  }
  
  if (!is_mapped_user_vaddr (esp))
    sys_exit (-1);
  
  int sys_number;
  POP (sys_number);
  int argc = syscall_argc(sys_number);
  
  //Function pointer
  int (*sys_func)(int, int, int);
  
  switch (sys_number) {
    case SYS_HALT:     sys_func = (syscall_func*) sys_halt; break;
    case SYS_EXIT:     sys_func = (syscall_func*) sys_exit; break;
    case SYS_EXEC:     sys_func = (syscall_func*) sys_exec; break;
    case SYS_WAIT:     sys_func = (syscall_func*) sys_wait; break;
    case SYS_CREATE:   sys_func = (syscall_func*) sys_create; break;
    case SYS_REMOVE:   sys_func = (syscall_func*) sys_remove; break;
    case SYS_OPEN:     sys_func = (syscall_func*) sys_open; break;
    case SYS_FILESIZE: sys_func = (syscall_func*) sys_filesize; break;
    case SYS_READ:     sys_func = (syscall_func*) sys_read; break;
    case SYS_WRITE:    sys_func = (syscall_func*) sys_write; break;
    case SYS_SEEK:     sys_func = (syscall_func*) sys_seek; break;
    case SYS_TELL:     sys_func = (syscall_func*) sys_tell; break;
    case SYS_CLOSE:    sys_func = (syscall_func*) sys_close; break;
    default:           sys_func = (syscall_func*) sys_unimplemented;
  }
  
  int args[] = {0, 0, 0};
  for (int i = 0; i < argc; ++i) {
    if (!is_mapped_user_vaddr (esp))
      sys_exit (-1);
    POP (args[i]);
  }
  
  f->eax = sys_func(args[0], args[1], args[2]);
  
  //Note: I see now Ivo suggested to use something like `struct syscall`, and
  //have an array of syscalls so that you can reference them by index via their
  //sys_number, and call the number. I see no main benefit to use this instead,
  //but maybe a use for it could be found, or at least this implementation could
  //be referenced as an alternate design in the design doc if needed.
  
}

/* Added for the mmap syscall. This was created following Ivo's manual. */
struct mapping {
    struct list_elem elem;    /* List element. */
    int handle;               /* Mapping id. */
    struct file *file;        /* File. */
    uint8_t *base;            /* Start of memory mapping. */
    size_t page_cnt;          /* Number of pages mapped. */
};

static int sys_unimplemented (void) {
  printf ("Warning: A syscall was called that hasn't been implemented yet.\n");
  return EXIT_FAILURE;
}

static void sys_halt (void) {
  shutdown_power_off ();
}

static int sys_exit (int status) {
  thread_exit (status);
  NOT_REACHED ();
}

static int sys_exec (const char *file) {
  
  // this should indicate a bad ptr was passed
  if (!is_mapped_user_vaddr (file))
    sys_exit (-1);
  
  if (file == NULL || file[1] == '\0')
    sys_exit (-1);
  
  return process_execute (file);
}

static int sys_wait (pid_t pid) {
  return process_wait (pid);
}

static int sys_create (const char *file, unsigned initial_size) {
  
  if (!is_mapped_user_vaddr (file))
    sys_exit (-1);
  
  if (file == NULL || file[1] == '\0')
    sys_exit (-1);
    
  return filesys_create (file, initial_size);
}

static bool sys_remove (const char *file) {
  return filesys_remove (file);
}

static int sys_open (const char *file) {
  
  if (!is_mapped_user_vaddr (file))
    sys_exit (-1);
  
  struct file *f = filesys_open (file);
  if (f == NULL)
    return -1;
  
  struct thread *t = thread_current ();
  struct hash *h = &t->open_files_hash;
  struct hash_key *hkey = (struct hash_key*) f;
  hash_insert (h, &hkey->elem); 

  return hkey->key;
}

static int sys_filesize (int fd) {
  struct thread *tc = thread_current ();
  struct hash *h = &tc->open_files_hash;
  struct hash_elem *e = NULL;

  e = hash_lookup_key (h, fd);

  int filesize = 0;

  if (e != NULL) {
    struct hash_key *hkey = hash_entry (e, struct hash_key, elem);
    filesize = file_length ((struct file*) hkey);
  }

  return filesize;
}

static int sys_read (int fd, void *buffer, unsigned size) {
  if (fd ==1 || !is_mapped_user_vaddr (buffer)) {
    sys_exit (-1);
  }

  int read = 0; // hold the number of bytes read from the file

  if (fd == 0) {
    read = input_getc (); //keyboard input, I am not sure we really need to worry about this
  }

  struct thread *tc = thread_current ();
  struct hash *h = &tc->open_files_hash;
  struct hash_elem *e = NULL;

  e = hash_lookup_key (h, fd);

  if (e != NULL) {
    struct hash_key *hkey = hash_entry (e, struct hash_key, elem);
    read = file_read ((struct file*) hkey, buffer, size);
  }

  return read;

}


static int sys_write (int fd, const void *buffer, unsigned size) {
  /*
  Writes size bytes from buffer to the open file fd. Returns the number of bytes
  actually written.
  Writing past end-of-file would normally extend the file, but file growth is not
  implemented by the basic file system. The expected behavior is to write as many
  bytes as possible up to end-of-file and return the actual number written.
  */
  
  //Note: if size is larger than a few hundred bytes, break up into pieces.
  //It is suggested to use putbuf, but would printf work? Why not do that?
  if (fd == 1) {
    putbuf (buffer, size); 
  }

  else if (fd == 0) {
    return -1;
  }

  else if (!is_mapped_user_vaddr (buffer)) {
    sys_exit (-1);
  }

  struct thread *tc = thread_current();
  struct hash *h = &tc->open_files_hash;
  int written = 0;
  struct hash_elem *e = NULL;

  e = hash_lookup_key (h, fd);

  if (e != NULL) {
    struct hash_key *hkey = hash_entry (e, struct hash_key, elem);
    written = file_write ((struct file*) hkey, buffer, size);     
  }
  
  return written;
}

static void sys_seek (int fd, unsigned position) {
  
  struct thread *tc = thread_current();
  struct hash *h = &tc->open_files_hash;
  struct hash_elem *e = NULL;

  e = hash_lookup_key (h, fd);

  if (e != NULL) {
    struct hash_key *hkey = hash_entry (e, struct hash_key, elem);
    file_seek ((struct file*) hkey, position);
  }
}

static int sys_tell (int fd) {

  int position = 0; //position of the next byte to be read

  struct thread *tc = thread_current();
  struct hash *h = &tc->open_files_hash;
  struct hash_elem *e = NULL;

  e = hash_lookup_key (h, fd);

  if (e != NULL) {
    struct hash_key *hkey = hash_entry (e, struct hash_key, elem);
    position = file_tell ((struct file*) hkey);
  }

  return position;

}

static void sys_close (int fd) {
  
  /* fd 0 and 1 should never be added to a thread's "open_files"
   * list in the first place (so it would just be "not found" in
   * the loop below), but perform a check here just to be safe. */
  if (fd == 0 || fd == 1)
    sys_exit (-1);
  
  struct thread *tc = thread_current();
  struct hash *h = &tc->open_files_hash;
  struct hash_elem *e = NULL;
  e = hash_lookup_key (h, fd);
  
  if (e != NULL) {
    struct hash_key *hkey = hash_entry (e, struct hash_key, elem);
    e = hash_delete_key (h, fd); 
    file_close ((struct file*) hkey); 
  }
  else
    sys_exit (-1);
}

/* Project 3 and optionally project 4. */
UNUSED static int sys_mmap (int fd UNUSED, void *addr UNUSED) {
  return EXIT_FAILURE;
}

UNUSED static int sys_munmap (mapid_t mapid UNUSED) {
  return EXIT_FAILURE;
}

/* Project 4 only. */
UNUSED static int sys_chdir (const char *dir UNUSED) {
  return EXIT_FAILURE;
}

UNUSED static int sys_mkdir (const char *dir UNUSED) {
  return EXIT_FAILURE;
}

UNUSED static int sys_readdir (int fd UNUSED, char name[READDIR_MAX_LEN + 1] UNUSED) {
  return EXIT_FAILURE;
}

UNUSED static int sys_isdir (int fd UNUSED) {
  return EXIT_FAILURE;
}

UNUSED static int sys_inumber (int fd UNUSED) {
  return EXIT_FAILURE;
}

#if false //Not sure where these need to go, so keep it here for now.
/* Returns true if UADDR is a valid, mapped user address,
  false otherwise. */
static bool verify_user (const void *uaddr) {
 return (uaddr < PHYS_BASE
         && pagedir_get_page (thread_current ()->pagedir, uaddr) != NULL);
}

/* Copies a byte from user address USRC to kernel address DST.
  USRC must be below PHYS_BASE.
  Returns true if successful, false if a segfault occurred. */
static inline bool get_user (uint8_t *dst, const uint8_t *usrc) {
 int eax;
 asm ("movl $1f, %%eax; movb %2, %%al; movb %%al, %0; 1:"
      : "=m" (*dst), "=&a" (eax) : "m" (*usrc));
 return eax != 0;
}

/* Writes BYTE to user address UDST.
  UDST must be below PHYS_BASE.
  Returns true if successful, false if a segfault occurred. */
static inline bool put_user (uint8_t *udst, uint8_t byte) {
 int eax;
 asm ("movl $1f, %%eax; movb %b2, %0; 1:"
      : "=m" (*udst), "=&a" (eax) : "q" (byte));
 return eax != 0;
}

#endif 
