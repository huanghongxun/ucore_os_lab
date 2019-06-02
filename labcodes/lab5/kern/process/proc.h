#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>


/**
 * 表示进程生命周期中的各个状态。
 * 此处使用 RUNNABLE 来同时表示正在运行和等待时间片的进程。
 */
enum proc_state {
    // 未初始化，新建进程时 (alloc_proc 函数) 将其状态设为该项
    PROC_UNINIT = 0,
    // 阻塞：一般是在等待资源
    PROC_SLEEPING,
    /**
     * 就绪：运行中或等待运行中
     * 进程已经可以运行了（可能已经在运行，或者未获得 CPU 的时间片，被其他进程抢占）。
     * 我们进行调度时选择 RUNNABLE 的进程进行调度。
     */
    PROC_RUNNABLE,
    // 进程执行结束，但未被操作系统或父进程回收
    PROC_ZOMBIE,
};

/**
 * 内核态切换时来保存通用寄存器的结构体。
 * 
 * 由于上下文是内核的，内核态的段寄存器都是一样的（操作系统只用一个），
 * 因此不需要保存段寄存器值。
 * %eax 寄存器的值我们这里不予以进行维护。
 * @note context 结构体的顺序必须和 switch.S 中的代码完全一致
 */
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};

#define PROC_NAME_LEN               15
#define MAX_PROCESS                 4096
#define MAX_PID                     (MAX_PROCESS * 2)

extern list_entry_t proc_list;

/**
 * 进程控制块
 * 由于我们把线程看作特殊的进程，我们就可以同时方便地管理进程和线程，因此线程和进程
 * 都使用该结构体来存放相关信息。
 * 在 alloc_proc 中初始化该结构体
 */
struct proc_struct {
    enum proc_state state;                      // 进程状态，初始化为 PROC_UNINIT
    int pid;                                    // 进程 ID，初始化为 -1
    int runs;                                   // 进程运行的次数，初始化为 0
    uintptr_t kstack;                           // 进程内核栈地址
    volatile bool need_resched;                 // 是否需要对当前进程进行调度，由 (sched.c:schedule) 函数进行管理，初始化时为 false
    struct proc_struct *parent;                 // 父进程的结构体地址
    struct mm_struct *mm;                       // 进程的内存管理器相关信息
    struct context context;                     // 进程运行的寄存器上下文
    struct trapframe *tf;                       // 当前中断的 trapframe
    uintptr_t cr3;                              // CR3 寄存器: 进程自己的页目录表页基址
    uint32_t flags;                             // Process flag
    char name[PROC_NAME_LEN + 1];               // 进程名
    list_entry_t list_link;                     // 进程集合列表的链表指针
    list_entry_t hash_link;                     // 进程哈希表的链表指针
    int exit_code;                              // exit code (be sent to parent proc)
    uint32_t wait_state;                        // waiting state
    struct proc_struct *cptr;                   // 第一个子进程的结构体地址
    struct proc_struct *yptr;                   // 当前进程的右（年轻）兄弟进程（与当前进程的父进程相同）的结构体地址
    struct proc_struct *optr;                   // 当前进程的左（年长）兄弟进程（与当前进程的父进程相同）的结构体地址
};

#define PF_EXITING                  0x00000001      // getting shutdown

#define WT_CHILD                    (0x00000001 | WT_INTERRUPTED)
#define WT_INTERRUPTED               0x80000000                    // the wait state could be interrupted


#define le2proc(le, member)         \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);
int do_yield(void);
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);
int do_wait(int pid, int *code_store);
int do_kill(int pid);
#endif /* !__KERN_PROCESS_PROC_H__ */

