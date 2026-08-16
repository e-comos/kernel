/* include/kernel/process.h */
#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stddef.h>
#include <stdint.h>
/*========================================
 * Kernel Process Management Core Interface
 * Only includes what the kernel must provide
 * User-space C library builds on top of this
 ========================================*/

/*------------------
 * Process Identifier
 *-----------------*/
typedef int32_t pid_t;
#define PID_INVALID (-1)
#define PID_KERNEL (0) /* Kernel process ID */
#define PID_IDLE (0)   /* Idle process */
#define PID_INIT (1)   /* First user process */

/*------------------
 * Process State (Internal to kernel)
 *-----------------*/
typedef enum {
	PROC_NEW,      /* New, not ready */
	PROC_READY,    /* Ready to run */
	PROC_RUNNING,  /* Currently executing */
	PROC_BLOCKED,  /* Blocked (waiting for resource) */
	PROC_SLEEPING, /* Sleeping (waiting for time) */
	PROC_ZOMBIE,   /* Zombie (waiting for parent to reap) */
	PROC_DEAD      /* Dead, resources can be reclaimed */
} proc_state_t;

/*------------------
 * Process Context (Register state)
 *-----------------*/
typedef struct {
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t rip;    /* Instruction pointer */
	uint64_t cs;     /* Code segment selector */
	uint64_t rflags; /* Flags register */
	uint64_t rsp;    /* Stack pointer */
	uint64_t ss;     /* Stack segment selector */

	/* FPU/SSE state (if needed) */
	uint8_t fpu_state[512] __attribute__((aligned(16)));
} proc_context_t;

/*------------------
 * Process Memory Mapping (Kernel internal management)
 *-----------------*/
typedef struct {
	uintptr_t code_start; /* Code segment physical address */
	size_t code_size;     /* Code segment size */
	uintptr_t stack_top;  /* Stack top address */
	size_t stack_size;    /* Stack size */
	uintptr_t heap_start; /* Heap start address */
	uintptr_t heap_break; /* Current heap break */

	/* Page table information */
	uintptr_t page_dir;    /* Page directory physical address (CR3) */
	uint32_t *page_tables; /* Page table array */
} proc_memory_t;

/*------------------
 * Process Control Block (PCB)
 * Kernel internal management, not exposed to user-space
 *-----------------*/
typedef struct proc {
	/* Basic information */
	pid_t pid;          /* Process ID */
	pid_t ppid;         /* Parent Process ID */
	char name[16];      /* Process name (for debugging) */
	proc_state_t state; /* Current state */

	/* Execution context */
	proc_context_t ctx;     /* CPU context (for switching) */
	proc_memory_t mem;      /* Memory mapping */
	uintptr_t kernel_stack; /* Kernel stack */

	/* Scheduling information */
	uint8_t priority;    /* Priority 0-31 */
	uint64_t time_slice; /* Remaining time slice */
	uint64_t time_used;  /* CPU time used */
	uint64_t wake_time;  /* Wake time (for sleep) */

	/* Resource management */
	struct proc *parent;   /* Parent process pointer */
	struct proc *children; /* Child process list head */
	struct proc *sibling;  /* Sibling process list */

	/* Wait queue */
	struct proc *wait_queue; /* Processes waiting for this one */
	int exit_code;           /* Exit code */
	uint8_t signal_pending;  /* Pending signals */

	/* Statistics */
	uint64_t create_time; /* Creation timestamp */
	uint64_t start_time;  /* Start execution time */

	/* Inter-process communication */
	void *ipc_buffer;       /* IPC message buffer */
	size_t ipc_buffer_size; /* Buffer size */

	/* Miscellaneous flags */
	uint32_t flags; /* Process flags */
} proc_t;

/* Process flags */
#define PROC_FLAG_KERNEL (1 << 0) /* Kernel process */
#define PROC_FLAG_USER (1 << 1)   /* User process */
#define PROC_FLAG_VM (1 << 2)     /* Has its own address space */
#define PROC_FLAG_MMAP (1 << 3)   /* Has memory mappings */
#define PROC_FLAG_SIGNAL (1 << 4) /* Supports signals */

/*------------------
 * Process Management Functions (Internal to kernel)
 * These functions can only be called from Ring 0
 *-----------------*/

/* Process table management */
void proc_table_init(void);
proc_t *proc_alloc(void);
void proc_free(proc_t *proc);
proc_t *proc_find(pid_t pid);

/* Process creation and destruction */
pid_t proc_create_kernel(void (*entry)(void), const char *name, uint8_t priority);
pid_t proc_create_user(uintptr_t entry, uintptr_t stack, const char *name);
void proc_destroy(pid_t pid);
void proc_exit(int exit_code);

/* Process scheduling */
void proc_schedule(void);
void proc_yield(void);
void proc_block(proc_t *proc);
void proc_wakeup(proc_t *proc);

/* Process context switching */
void proc_context_save(proc_context_t *ctx);
void proc_context_restore(proc_context_t *ctx);
void proc_switch_to(proc_t *next);

/* Process state management */
void proc_set_state(proc_t *proc, proc_state_t state);
proc_state_t proc_get_state(pid_t pid);
int proc_is_alive(pid_t pid);

/* Current process */
proc_t *proc_current(void);
pid_t proc_current_pid(void);

/* Process relationships */
pid_t proc_get_parent(pid_t pid);
int proc_add_child(proc_t *parent, proc_t *child);
int proc_remove_child(proc_t *parent, pid_t child_pid);

/* Process waiting */
int proc_wait(pid_t pid, int *status, int options);
void proc_notify_parent(proc_t *proc);

/* Debug and information */
void proc_dump(proc_t *proc);
void proc_dump_all(void);
int proc_count(void);

/*------------------
 * System call handler functions
 * These functions handle requests from user-space
 *-----------------*/

/* System call numbers definition */
#define SYS_PROC_CREATE 1
#define SYS_PROC_EXIT 2
#define SYS_PROC_WAIT 3
#define SYS_PROC_GETPID 4
#define SYS_PROC_GETPPID 5
#define SYS_PROC_YIELD 6
#define SYS_PROC_SLEEP 7
#define SYS_PROC_FORK 8
#define SYS_PROC_EXEC 9

/* System call handler functions (implemented in syscall.c) */
int sys_proc_create(uintptr_t entry, uintptr_t stack, uintptr_t name);
void sys_proc_exit(int code);
pid_t sys_proc_wait(int *status, int options);
pid_t sys_proc_getpid(void);
pid_t sys_proc_getppid(void);
void sys_proc_yield(void);
int sys_proc_sleep(uint32_t ms);
pid_t sys_proc_fork(void);
int sys_proc_exec(const char *path, const char **argv, const char **envp);

#endif /* KERNEL_PROCESS_H */
