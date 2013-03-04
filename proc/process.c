/*
 * Process startup.
 *
 * Copyright (C) 2003-2005 Juha Aatrokoski, Timo Lilja,
 *       Leena Salmela, Teemu Takanen, Aleksi Virtanen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: process.c,v 1.11 2007/03/07 18:12:00 ttakanen Exp $
 *
 */

#include "proc/process.h"
#include "proc/elf.h"
#include "kernel/assert.h"
#include "kernel/interrupt.h"
#include "kernel/config.h"
#include "kernel/sleepq.h"
#include "fs/vfs.h"
#include "drivers/yams.h"
#include "vm/vm.h"
#include "vm/pagepool.h"
#include "lib/libc.h"

/** @name Process startup
 *
 * This module contains facilities for managing userland process.
 */

process_control_block_t process_table[PROCESS_MAX_PROCESSES];

/** Spinlock which must be held when manipulating the process table */
spinlock_t process_table_slock;

/**
 * Starts one userland process. The thread calling this function will
 * be used to run the process and will therefore never return from
 * this function. This function asserts that no errors occur in
 * process startup (the executable file exists and is a valid ecoff
 * file, enough memory is available, file operations succeed...).
 * Therefore this function is not suitable to allow startup of
 * arbitrary processes.
 *
 * @pid The process ID of the executable to be run in the userland
 * process
 */
void process_start(const process_id_t pid) {
  // Testing
  kprintf("Running process_start.\n");
    thread_table_t *my_entry;
    pagetable_t *pagetable;
    uint32_t phys_page;
    context_t user_context;
    uint32_t stack_bottom;
    elf_info_t elf;
    openfile_t file;
    interrupt_status_t intr_status = _interrupt_disable();
    spinlock_acquire(&process_table_slock);
    char *executable = process_table[pid].name;
    spinlock_release(&process_table_slock);

    int i;

    my_entry = thread_get_current_thread_entry();

    my_entry->process_id = pid;

    /* If the pagetable of this thread is not NULL, we are trying to
       run a userland process for a second time in the same thread.
       This is not possible. */
    KERNEL_ASSERT(my_entry->pagetable == NULL);

    pagetable = vm_create_pagetable(thread_get_current_thread());
    KERNEL_ASSERT(pagetable != NULL);

    my_entry->pagetable = pagetable;
    _interrupt_set_state(intr_status);

    file = vfs_open((char *)executable);
    /* Make sure the file existed and was a valid ELF file */
    KERNEL_ASSERT(file >= 0);
    KERNEL_ASSERT(elf_parse_header(&elf, file));

    /* Trivial and naive sanity check for entry point: */
    KERNEL_ASSERT(elf.entry_point >= PAGE_SIZE);

    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    KERNEL_ASSERT(elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE
		  <= _tlb_get_maxindex() + 1);

    /* Allocate and map stack */
    for(i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE, 1);
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.ro_vaddr + i*PAGE_SIZE, 1);
    }

    for(i = 0; i < (int)elf.rw_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.rw_vaddr + i*PAGE_SIZE, 1);
    }

    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);
    
    /* Now we may use the virtual addresses of the segments. */

    /* Zero the pages. */
    memoryset((void *)elf.ro_vaddr, 0, elf.ro_pages*PAGE_SIZE);
    memoryset((void *)elf.rw_vaddr, 0, elf.rw_pages*PAGE_SIZE);

    stack_bottom = (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - 
        (CONFIG_USERLAND_STACK_SIZE-1)*PAGE_SIZE;
    memoryset((void *)stack_bottom, 0, CONFIG_USERLAND_STACK_SIZE*PAGE_SIZE);

    /* Copy segments */

    if (elf.ro_size > 0) {
	/* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.ro_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.ro_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.ro_vaddr, elf.ro_size)
		      == (int)elf.ro_size);
    }

    if (elf.rw_size > 0) {
	/* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.rw_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.rw_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.rw_vaddr, elf.rw_size)
		      == (int)elf.rw_size);
    }


    /* Set the dirty bit to zero (read-only) on read-only pages. */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        vm_set_dirty(my_entry->pagetable, elf.ro_vaddr + i*PAGE_SIZE, 0);
    }

    /* Insert page mappings again to TLB to take read-only bits into use */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof(user_context));
    user_context.cpu_regs[MIPS_REGISTER_SP] = USERLAND_STACK_TOP;
    user_context.pc = elf.entry_point;

    intr_status = _interrupt_disable();
    thread_goto_userland(&user_context);
    _interrupt_set_state(intr_status);

    KERNEL_PANIC("thread_goto_userland failed.");
}

void process_init() {
  process_id_t i;
  interrupt_status_t intr_status = _interrupt_disable();
  spinlock_reset(&process_table_slock);
  spinlock_acquire(&process_table_slock);
  for (i = 0; i < PROCESS_MAX_PROCESSES; i++) {
    // Testing.
    kprintf("Initializing process table slot %d\n", (int) i);
    process_table[i].state = PROCESS_FREE;
  }
  spinlock_release(&process_table_slock);
  _interrupt_set_state(intr_status);

}

process_id_t process_spawn(const char *executable) {
  // Testing.
  kprintf("Spawning process %s\n", executable);
  interrupt_status_t intr_status = _interrupt_disable();
  spinlock_acquire(&process_table_slock);
  process_id_t pid = process_get_free();
  // Testing.
  kprintf("Found free process spot %d.\n", (int) pid);
  spinlock_release(&process_table_slock);
  _interrupt_set_state(intr_status);
  if (pid == PROCESS_PTABLE_FULL) {
    KERNEL_PANIC("process_table is full.");
  }
  
  intr_status = _interrupt_disable();
  spinlock_acquire(&process_table_slock);
  process_control_block_t *myentry = &process_table[pid];
  process_id_t par = process_get_current_process();
  stringcopy(myentry->name, executable, PROCESS_MAX_NAMESIZE);
  myentry->pid = pid;
  myentry->parent = par;
  // Testing
  kprintf("Process has parent: %d\n", myentry->parent);
  kprintf("Process 10 has parent: %d\n", process_table[10].parent);
  myentry->state = PROCESS_RUNNING;
  if (par != -1) {
    myentry->resource = process_table[par].resource;
  }
  else {
    int x = 42;
    myentry->resource = &x;
    // Testing.
    kprintf("Process %d initialized with resource %d.\n", pid, &x);
  }
  spinlock_release(&process_table_slock);
  _interrupt_set_state(intr_status);

  // Testing
  kprintf("Starting thread with process %d\n", pid);
  intr_status = _interrupt_disable();
  thread_run(thread_create( (void (*) (uint32_t)) & process_start, pid));
  _interrupt_set_state(intr_status);
  kprintf("process_spawn returns pid: %d\n", pid);
  return pid;
}

/* Stop the process and the thread it runs in. Sets the return value as well */
void process_finish(int retval) {
  // Testing.
  kprintf("Finishing current process.\n");
  if (retval < 0) {
    KERNEL_PANIC("error: process_finish received negative arg");
  }
  interrupt_status_t intr_status = _interrupt_disable();
  spinlock_acquire(&process_table_slock);
  process_control_block_t *my_entry = process_get_current_process_entry();
  spinlock_release(&process_table_slock);
  _interrupt_set_state(intr_status);

  // Testing
  kprintf("Current process: %d\n", my_entry->pid);
  kprintf("Calling process_join_children\n");
  process_join_children(my_entry->pid);
  kprintf("Called process_join_children\n");
  intr_status = _interrupt_disable();
  spinlock_acquire(&process_table_slock);
  // Testing
  kprintf("Check: %d == -1\n", my_entry->parent);
  if (my_entry->parent != -1) {
    // Testing
    kprintf("Does have parent. \n");
    my_entry->state = PROCESS_ZOMBIE;
    sleepq_wake(my_entry->resource);
  }
  else {
    // Testing
    kprintf("Process does not have parent. \n");
    my_entry->state = PROCESS_FREE;
  }
  spinlock_release(&process_table_slock);

  thread_table_t *thr = thread_get_current_thread_entry();
  vm_destroy_pagetable(thr->pagetable);
  thr->pagetable = NULL;
  thread_finish();
  // Testing
  thread_switch();
  _interrupt_set_state(intr_status);
  kprintf("process_finish completed\n");
}

int process_join(process_id_t pid) {
  // Ensure that pid is a child of the current process.
  interrupt_status_t intr_status = _interrupt_disable();
  spinlock_acquire(&process_table_slock);
  if (process_table[pid].parent != process_get_current_process()) {
    spinlock_release(&process_table_slock);
    _interrupt_set_state(intr_status);
    return PROCESS_ILLEGAL_JOIN;
  }
  int *res = process_get_current_process_entry()->resource;
  spinlock_acquire(res);
  while (process_table[pid].state != PROCESS_ZOMBIE) {
    sleepq_add(res);
    spinlock_release(&process_table_slock);
    spinlock_release(res);
    thread_switch();
    spinlock_acquire(&process_table_slock);
    spinlock_acquire(res);
  }
  spinlock_release(&process_table_slock);

  // Work with resource.

  spinlock_release(res);
  process_table[pid].state = PROCESS_FREE;
  process_table[pid].parent = -1;
  spinlock_release(&process_table_slock);
  _interrupt_set_state(intr_status);

  return PROCESS_LEGAL_JOIN;
}


process_id_t process_get_current_process(void) {
    return thread_get_current_thread_entry()->process_id;
}

process_control_block_t *process_get_current_process_entry(void) {
    return &process_table[process_get_current_process()];
}

process_control_block_t *process_get_process_entry(process_id_t pid) {
    return &process_table[pid];
}

/* Finds free position in process table.
 *
 * return
 *
 */
process_id_t process_get_free() {
  process_id_t i;

  for (i = 0; i < PROCESS_MAX_PROCESSES; i++) {
    if (process_table[i].state == PROCESS_FREE) {
      return i;
    }
  }
  return PROCESS_PTABLE_FULL;
}

/* Calls process_join on all process with specified parent.
 *
 * @param pid
 * process_id of parent
 */
void process_join_children(process_id_t pid) {
  process_id_t i;
  interrupt_status_t intr_status = _interrupt_disable();
  spinlock_acquire(&process_table_slock);
  for (i = 0; i < PROCESS_MAX_PROCESSES; i++) {
    if (process_table[i].parent == pid) {
      spinlock_release(&process_table_slock);
      _interrupt_set_state(intr_status);
      process_join(i);
      intr_status = _interrupt_disable();
      spinlock_acquire(&process_table_slock);
    }
  }
  spinlock_release(&process_table_slock);
  _interrupt_set_state(intr_status);
}



/** @} */
