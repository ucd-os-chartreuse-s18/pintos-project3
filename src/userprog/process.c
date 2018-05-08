#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/stack.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include <keyed_hash.h>
#include <hash.h>

/* These values are relatively arbitrary, but seem reasonable.
 * There is an average of 16 chars availible for each argument. */
const int MAX_ARGS = 32;
const int MAX_CMDLN = 512;

struct process_args
{
  const char *args;
  struct semaphore *sema_ptr;
  bool *success_ptr;
  struct thread *parent;
};

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  char *argv;
  tid_t tid;
  
  /* Make a copy in a new page to avoid a race between the caller and load. */
  //ALLOCATION TYPE: Kernel (so we don't have to mess with it)
  //But: We want to book-keep its location
  argv = palloc_get_page (0);
  if (argv == NULL)
    return TID_ERROR;
  strlcpy (argv, file_name, PGSIZE);
  
  char *fname = (char*) malloc (15);
  strlcpy (fname, file_name, 15);
  fname = strtok_r (fname, " ", &fname);
  
  struct semaphore load_sema;
  sema_init (&load_sema, 0);
  bool load_successful = true;
  
  //Arguments to pass to start_process
  struct process_args *pargs = malloc (512);
  pargs->args = argv;
  pargs->sema_ptr = &load_sema;
  pargs->success_ptr = &load_successful;
  pargs->parent = thread_current ();
  
  tid = thread_create (fname, PRI_DEFAULT, start_process, pargs);
  
  //When sema continues, load_status will have been updated.
  sema_down (&load_sema);
  
  /* The timing of this free seems dangerous since it can happen
   * before a child thread exits and needs to refer to its name.
   * The only reason I am keeping it is because I can literally
   * change the string here and it won't affect the output at all,
   * even though there are alternating orders between a thread
   * exiting and getting to this point. */
  //strlcpy (fname, "hello", 15);
  free (fname);
  
  if (!load_successful)
    tid = TID_ERROR;
  
  if (tid == TID_ERROR) {
    palloc_free_page (argv);
    free (pargs);
  }
    
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *pargs_)
{
  struct process_args* pargs = (struct process_args*) pargs_;
  const char *argv = pargs->args;
  struct intr_frame if_;
  bool success;
  
  /* Any hashtables will need to be initialized in start_process
   * because a hashtable can only be initialized while the
   * current process is running. */
  struct thread *tc = thread_current ();
  struct thread *p = pargs->parent;
  keyed_hash_init (&tc->children_hash);
  keyed_hash_init (&tc->open_files_hash);
  keyed_hash_init (&tc->pages);
  hash_insert (&p->children_hash, &tc->hash_elem);
  
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (argv, &if_.eip, &if_.esp);
  *pargs->success_ptr = success;
  sema_up (pargs->sema_ptr);
  
  /* If load failed, quit. */
  if (!success) {
    thread_exit (-1);
  }
  
  /* We need to make sure that any dynamically allocated
   * variables from process_execute become freed here. */
  palloc_free_page ((char*) argv);
  free (pargs);
  
  printf ("starting to execute %s\n", tc->name);
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  /* What happens if this is called when the process has already
   * exited? The parent doesn't need to wait. Would errors just
   * arise? Maybe we should check the exit status of the current
   * thread and exit if it is not the default value. I don't
   * think it is set currently. */
  
  struct thread *tc = thread_current();
  struct hash *h = &tc->children_hash;
  struct hash_elem *e = hash_lookup_key (h, child_tid);
  if (e == NULL) return -1; //Not found
	struct thread *t = hash_entry (e, struct thread, hash_elem);
  hash_delete_key (&tc->children_hash, child_tid);
  
  /* Is this threadsafe? Consider a duplicate tid of 4 is
   * called on this function. The previous method just got
   * interrupted and was about to delete the key, but then
   * the new thread finds the element isn't deleted yet
   * when in fact it should.
   *
   * This may not be a problem, but maybe I could initialize
   * a static semaphore for this method? IDK, that might be
   * interesting. */
  sema_down (&t->dying_sema);
  int exit_status = t->exit_status;
  sema_up (&t->status_sema);
  return exit_status;
  
}

static void file_destructor (struct hash_elem *e, void* aux UNUSED) {
  struct hash_key *k = hash_entry (e, struct hash_key, elem);
  file_close ((struct file*) k);
}

/* Free the current process's resources. */
void
process_exit (int exit_status)
{
  struct thread *t = thread_current ();
  uint32_t *pd;
  
  file_close (t->executable);
  
  //Close all files
  hash_destroy (&t->open_files_hash, file_destructor);
  
  /* This was moved from sys_exit. Now we can see the printed status
   * upon exiting without needing to call the system call. This was
   * useful because the test-bad-* tests want to see the exit status
   * after a page fault occurs in exception.c. Now that we have the
   * status here as a variable, I wonder if there is a better way to
   * share the exit status with exec. */
  printf ("%s: exit(%d)\n", t->name, exit_status);
  thread_current ()->exit_status = exit_status;
  
  //Release process_wait
  sema_up (&t->dying_sema);
  
  /* Now we wait so process_wait can grab `exit_status`, which is
   * set in sys_exit. Alternatively, the kernel may exit a thread,
   * in which case the default exit_status is read as -1. It might
   * be better to pass a status to thread_exit and process_exit so
   * that this is made clearer. However, it seems we will still
   * end up needing to pass the exit status as a member of a child
   * thread, so we really don't need to carry that variable all the
   * way here.
   *
   * Also note that the same thing can be done by yielding the thread
   * (instead of using a semaphore) as shown below, but I don't think
   * it can actually guarantee that the other process will get to read
   * the exit_status, especially if we are not using a round robin thread
   * switching system, which we are currently using. */
  sema_down (&t->status_sema);
  //thread_yield ();
  
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = t->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      t->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();
  //printf ("context switch! (%s leaving)\n", t->name);
  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char *cmdline);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *cmdline, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();
  
  //file name limited to 14 by the system, add 1 for '\0'
  char *file_name = malloc (15);
  strlcpy (file_name, cmdline, 15);
  file_name = strtok_r (file_name, " ", &file_name);
  
  /* Open executable file. */
  file = filesys_open (file_name);
  free (file_name);
  if (file == NULL) {
    printf ("load: %s: open failed\n", cmdline);
    goto done;
  }
    
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);
      
      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }
  
  /* Set up stack. */
  if (!setup_stack (esp, cmdline))
    goto done;
  
  printf ("%s should be \"loaded\"\n", thread_current()->name);
  
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;
  success = true;
  file_deny_write (file);
  
  done:  
  /* We arrive here whether the load is successful or not. */
  t->executable = file;
  return success;
}

/* load helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) {
      
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      //will malloc mess up frame bookeeping?
      struct file_info *f_info = malloc (sizeof (struct file_info));
      f_info->file = file;
      f_info->file_offset = ofs;
      f_info->file_bytes = page_read_bytes;
      f_info->private = !writable;
      
      //Put into supplemental page table
      create_spt_entry(upage, IN_FILE, f_info);
      
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes; //do we need this?
      upage += PGSIZE;
  }
  return true;
}

static char*
setup_argv (const char *cmdline, int *argc, const char **argv) {
  
  //const char* cannot be altered, so copy it
  //line cannot be deleted in this method since argv points to it
  char *line = malloc (MAX_CMDLN);
  strlcpy (line, cmdline, MAX_CMDLN);
  char *del_ptr = line;
  char *token;
  
  token = strtok_r (line, " ", &line);
  for (*argc = 0; token != NULL; ++(*argc)) {
    argv[*argc] = token;
    token = strtok_r (NULL, " ", &line);
  }
  
  return del_ptr;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp_, const char *cmdline)
{
  uint8_t *kpage;
  uint8_t *upage;
  bool success = false;
  
  //The macro I made just uses esp. I think it is more
  //straightforward to write it this way.
  uint8_t* esp = *esp_;
  
  //ALLOCATION TYPE: FRAME (This one is also going to be mapped immediately)
  //Do we have to include it as a supplemental page entry? Yes
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) {
      
      upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
      
      /* From the manual: "All pages can be loaded lazily, except the first
       * user stack page which can be loaded in setup_stack() */
      success = install_page (upage, kpage, true);
      success = true;
      if (success) {
        
        esp = PHYS_BASE;
        const char *argv[MAX_ARGS];
        int argc;
        char *line;
        
        /* Until argv gets copied to esp, line cannot be freed. */
        line = setup_argv (cmdline, &argc, argv);
        
        /* By the c convention mentioned on page 36 of the Pintos Documentation
         * we need to push a null pointer (argv[argc]) as the sentinel value. */
        uint32_t* args_cpy[argc + 1];
        args_cpy[argc] = NULL;
        
        /* Copy the individual characters of each string onto the stack. (Note
         * that eacy copy is partitioned by blocks because pushing the individual
         * characters would result in a flipped order due to how memory is read
         * off of the stack. The stack pointer for each written block is saved
         * so that it can be written later. The extra char is to copy the null
         * terminator from the string. */
        for (int i = 0; i < argc; ++i) {
          esp -= strlen(argv[i]) + 1;
          memcpy (esp, argv[i], strlen(argv[i]) + 1);
          args_cpy[i] = (uint32_t*) esp;
          //Print addresses to confirm the hex dump:
          //printf ("argv[%d] = %p\n", i, (uint32_t*) esp);
        } free (line);
        
        //Word Align (push single bytes until we are aligned)
        while (((uint32_t) esp) % 4)
          PUSH (uint8_t, 0);
        
        //Push argv (n to 0), argv (char**), argc, fake return address
        PUSH (args_cpy);
        PUSH (char**, (char**) esp);
        PUSH (uint32_t, (uint32_t) argc);
        PUSH (uint32_t, 0);
        
      #if false
        printf ("Hex dump:\n");
        uint32_t dw = (uint32_t) PHYS_BASE - (uint32_t) esp;
        /* a) Hex address to start counting from.
         * b) Hex address to actually get data from.
         * c) Number of bytes to write (should match the amount you wrote)
         * d) Show ASCII
         *                    a)    b)  c)   d)
         *                    |     |   |    |
         *                    v     v   v    v */
        hex_dump ((uintptr_t) esp, esp, dw, true);
      #endif
        
        //Save changes made to esp
        *esp_ = esp;
      } else palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  
  //Here, we should update our information in frame.c regarding mappings
  
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

int
allocate_fd (void)
{
  return thread_current()->next_fd++;
}
