#ifndef __KERN_SCHEDULE_SCHED_H__
#define __KERN_SCHEDULE_SCHED_H__

#include <defs.h>
#include <list.h>
#include <skew_heap.h>

#define MAX_TIME_SLICE 5

struct proc_struct;

/**
 * 定义了 timer_t 的基本结构。
 * 可以用 sched.h 中的 timer_init 函数对其进行初始化。
 * 
 * 基于定时器，操作系统得以向上提供基于时间点的事件，并实现基于
 * 时间长度的睡眠等待和唤醒机制。在每个时钟中断发生时，操作系统
 * 产生对应的时间事件。应用程序或者操作系统的其他组件可以以此来
 * 构建更复杂和高级的进程管理和调度算法。
 * 
 * 一个 timer_t 在系统中的存活周期可以被描述如下：
 * 1. timer_t 在某个位置被创建和初始化，并通过 add_timer 加入系统管理列表中
 * 2. 系统时间被不断累加，直到 run_timer_list 发现该 timer_t 到期。
 * 3. run_timer_list 更改对应的进程状态，并从系统管理列表中移除该timer_t。
 */
typedef struct {
    unsigned int expires;       //the expire time
    struct proc_struct *proc;   //the proc wait in this timer. If the expire time is end, then this proc will be scheduled
    list_entry_t timer_link;    //the timer list
} timer_t;

#define le2timer(le, member)            \
to_struct((le), timer_t, member)

/* 对某定时器进行初始化，让它在 expires 时间片之后唤醒 proc 进程 */
static inline timer_t *
timer_init(timer_t *timer, struct proc_struct *proc, int expires) {
    timer->expires = expires;
    timer->proc = proc;
    list_init(&(timer->timer_link));
    return timer;
}

struct run_queue;

// The introduction of scheduling classes is borrrowed from Linux, and makes the 
// core scheduler quite extensible. These classes (the scheduler modules) encapsulate 
// the scheduling policies. 
struct sched_class {
    // sched_class 的名称
    const char *name;
    // 初始化 run queue 的函数
    void (*init)(struct run_queue *rq);
    // 将进程加入 run queue 中，必须使用 rq_lock
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);
    // 从 run queue 中弹出一个进程，必须使用 rq_lock
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);
    // 选择下一个要运行的可执行任务
    struct proc_struct *(*pick_next)(struct run_queue *rq);
    // 时钟中断处理函数
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc);
    /* for SMP support in the future
     *  load_balance
     *     void (*load_balance)(struct rq* rq);
     *  get some proc from this rq, used in load_balance,
     *  return value is the num of gotten proc
     *  int (*get_proc)(struct rq* rq, struct proc* procs_moved[]);
     */
};

/**
 * 存储调度算法维护的进程队列等数据结构
 */
struct run_queue {
    list_entry_t run_list; // 调度队列的头指针
    unsigned int proc_num; // 在调度队列中的进程数
    int max_time_slice; // 队列的最大时间片，进程重新获得时间片的时间为该项值
    // For LAB6 ONLY
    skew_heap_entry_t *lab6_run_pool; // 对于 Stride Scheduling 算法，该项为左偏树的根节点
};

void sched_init(void);
void wakeup_proc(struct proc_struct *proc);
void schedule(void);

/**
 * 向系统添加某个初始化过的timer_t，该定时器在指定时间后被激活，
 * 并将对应的进程唤醒至 runnable（如果当前进程处在等待状态）。
 */
void add_timer(timer_t *timer);

/**
 * 向系统删除（或者说取消）某一个定时器。该定时器在取消后不会被
 * 系统激活并唤醒进程。
 */
void del_timer(timer_t *timer);

/**
 * 更新当前系统时间点，遍历当前所有处在系统管理内的定时器，
 * 找出所有应该激活的计数器，并激活它们。该过程在且只在每次
 * 定时器中断时被调用。在 ucore 中，其还会调用调度器事件处理程序。
 */
void run_timer_list(void);

#endif /* !__KERN_SCHEDULE_SCHED_H__ */

