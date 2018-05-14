#include <stdio.h>
#include <syscall-nr.h>
#include <keyed_hash.h>
#include <round.h>
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
#include "vm/page.h"


static bool triggered_segfault (const uint8_t *uaddr);
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
//unmap is not static, see syscall.h

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
    case SYS_MUNMAP:
      return 1;
    
    case SYS_CREATE:
    case SYS_SEEK:
    case SYS_MMAP:
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
  
  /* Syscall numbers are defined in syscall-nr.h */
  #define assign sys_func = (syscall_func*)
  switch (sys_number) {
    case SYS_HALT:     assign sys_halt; break;
    case SYS_EXIT:     assign sys_exit; break;
    case SYS_EXEC:     assign sys_exec; break;
    case SYS_WAIT:     assign sys_wait; break;
    case SYS_CREATE:   assign sys_create; break;
    case SYS_REMOVE:   assign sys_remove; break;
    case SYS_OPEN:     assign sys_open; break;
    case SYS_FILESIZE: assign sys_filesize; break;
    case SYS_READ:     assign sys_read; break;
    case SYS_WRITE:    assign sys_write; break;
    case SYS_SEEK:     assign sys_seek; break;
    case SYS_TELL:     assign sys_tell; break;
    case SYS_CLOSE:    assign sys_close; break;
    case SYS_MMAP:     assign sys_mmap; break;
    case SYS_MUNMAP:   assign sys_munmap; break;
    default:           assign sys_unimplemented;
  }
  #undef assign
  
  int args[] = {0, 0, 0};
  for (int i = 0; i < argc; ++i) {
    if (!is_mapped_user_vaddr (esp))
      sys_exit (-1);
    POP (args[i]);
  }
  
  //printf ("args are [%d, %d, %d]\n", args[0], args[1], args[2]);
  f->eax = sys_func(args[0], args[1], args[2]);
  
  //Note: I see now Ivo suggested to use something like `struct syscall`, and
  //have an array of syscalls so that you can reference them by index via their
  //sys_number, and call the number. I see no main benefit to use this instead,
  //but maybe a use for it could be found, or at least this implementation could
  //be referenced as an alternate design in the design doc if needed.
  
}

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
  if (f == NULL) {
    //printf ("\tfile not found\n");
    return -1;
  }
  
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

/* Basically validates a user address. Returns true if a seg fault
 * occured. This is based completely off of the provided `get_user()`
 * function. */
static bool
triggered_segfault (const uint8_t *uaddr) {
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
    : "=&a" (result) : "m" (*uaddr));
  return (result == -1);
}

static int sys_read (int fd, void *buffer, unsigned size) {
  
  /* This is a hack. memchr activates a page-in on the buffer,
   * while the print and pos ensure that the memchr statement
   * doesn't get optimized out. There is probably a better 
   * way to handle the optimization, but anyways.. found a
   * much better way to cause a segfault.
  void* pos = memchr (buffer, 'x', 1);
  printf ("%s", (pos == 0 ? "":"1"));
  */
  
  if (triggered_segfault (buffer))
    sys_exit (-1);
  if (fd == 1 || !is_mapped_user_vaddr (buffer))
    sys_exit (-1);
  
  int read = 0;
  if (fd == 0) {
    read = input_getc ();
  }
  
  struct hash *h = &thread_current()->open_files_hash;
  struct hash_elem *e = NULL;
  
  e = hash_lookup_key (h, fd);
  if (e != NULL) {
    struct file *file = keyed_hash_entry (e, struct file);
    //read = file_read_at (file, buffer, size, 0);
    read = file_read (file, buffer, size);
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
  } else if (fd == 0) {
    return -1;
  } else if (!is_mapped_user_vaddr (buffer)) {
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
  struct hash_elem *e = hash_lookup_key (h, fd);
  if (e == NULL) sys_exit (-1);
  
  /* If we are done with the file for good (not mmaped), then
   * delete it from the hash as well. */
  struct file *file = keyed_hash_entry (e, struct file);
  //printf ("file is %p\n", file);
  //if (file_close (file))
  
  /* Was expecting to use a newly defined status from file_close
   * instead of using this file_mapped function here, but there
   * is a problem with that. 1) hash_key_delete is dependant on
   * whether or not the file is mapped, and 2) closing the file
   * before deleting the hash messes with the data. */
  if (!file_mapped (file)) {
    hash_delete_key (h, fd);
    file_close (file);
  }
}

static int sys_mmap (int fd, void *upage) {
  
  if (fd == 0 || fd == 1)
    sys_exit (-1);
    
  if (upage == NULL)
    return -1;
  
  struct hash *h = &thread_current()->pages;
  struct hash_elem *e = HASH_LOOKUP_KEY (h, upage);
  if (e != NULL) return -1;
  
  //This does not seem to affect the tests at all
  //if (!is_mapped_user_vaddr (addr))
  //  sys_exit (-1);
  
  h = &thread_current()->open_files_hash;
  e = hash_lookup_key (h, fd);
  if (e == NULL) sys_exit(-1);
  
  struct file *file = keyed_hash_entry (e, struct file);
  if (file_length (file) == 0) return -1;
  if (file_mapped (file)) return -1;
  file_map (file, upage);

  off_t ofs = 0;
  uint32_t read_bytes = file_length (file);
  uint32_t zero_bytes = ROUND_UP (read_bytes, PGSIZE) - read_bytes;
  
  //Based on "load segment"
  while (read_bytes > 0 || zero_bytes > 0) {
      
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      //Note: All mmapping are public
      struct file_info *f_info = malloc (sizeof (struct file_info));
      f_info->file = file;
      f_info->file_offset = ofs;
      f_info->file_bytes = page_read_bytes;
      f_info->private = false;
      
      //Put into supplemental page table
      create_spt_entry (upage, IN_FILE, f_info);
      
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes;
      upage += PGSIZE;
  }
  return fd;
}

void sys_munmap (mapid_t mapid) {
  
  //printf ("UNMAP\n");
  struct thread *tc = thread_current();
  struct hash *h = &tc->open_files_hash;
  struct hash_elem *e = hash_lookup_key (h, mapid);
  if (e == NULL) {
    //printf ("ALPHA\n");
    sys_exit (-1);
  }
  struct file *file = keyed_hash_entry (e, struct file);
  
  int page_count = file_pagec (file);
  uint32_t** pages = file_pagev (file);
  
  struct page *p;
  struct file_info *fi;
  for (int i = 0; i < page_count; i++) {
    
    e = HASH_LOOKUP_KEY (&tc->pages, pages[i]);
    p = keyed_hash_entry (e, struct page);
    
    //Lazy write back (idk how dirty gets set btw)
    if (pagedir_is_dirty (tc->pagedir, pages[i])) {
      fi = p->file_info;
      file_seek (file, fi->file_offset);
      file_write (file, pages[i], fi->file_bytes);
    }
    
    HASH_KEY_DELETE (&tc->pages, pages[i]);
    free (p);
  }
  
  file_unmap (file);
  if (mmap_close (file)) {
    //printf ("REMOVING FROM HASH\n");
    file_close (file);
    HASH_KEY_DELETE (h, mapid);
  } else {
    file_seek (file, 0);
  }
  free (pages);
  
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

