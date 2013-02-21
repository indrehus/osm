#define PROCESS_MAX_NAMESIZE 256

typedef enum {
	PROCESS_RUNNING;
	PROCESS_ZOMBIE;
	PROCESS_DEAD;
	PROCESS_FREE;
} 

typedef struct {
	process_id_t pid;
	char name[PROCESS_MAX_NAMESIZE];
	
} process_control_block_t;

process_id_t process_acquire()
{
	for (process_id_t pid = 0; pid < PROCESS_MAX_PROCESSES; ++pid) {
		if (process_table[pid].state != PROCESS_FREE)
			continue;
		else
			return pid;
	}
}

process_id_t process_spawn(const char *executable)
{
	process_id_t pid = process_get_free();
	stringcpy(process_table[pid].name, executable, 256);
	thread_create(process_start(process_table[pid].name));
}

int process_join(process_id_t pid) {
	if (process_table[pid].parent != process_get_current_process())
		return -1;
	interrupt_status_t state = _interrupt_disable();
	spinlock_acquire(&slock);
	while (process_table[pid].state != PROCESS_ZOMBIE)
		sleepq_add(BLOBFIXBLOBTHIS!!!);
		spinlock_release(&slock);
		thread_switch();
		spinlock_acquire(&slock);

	// Do stuff

	spinlock_release(&slock);
	_interrupt_set_state(state);
	return 0; /* Dummy */
}

void process_finish (int retval) {

	// Do stuff.

	// Finish:
	thread_table_t *thr = thread_get _current_thread_entry();
	vm_destroy_pagetable(thr->pagetable);
	thr->pagetable = NULL;

	sleepq_wakeall(BLOBLOBLOBLOB!!!);
	thread_finish();
