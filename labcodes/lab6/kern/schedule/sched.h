#ifndef __KERN_SCHEDULE_SCHED_H__
#define __KERN_SCHEDULE_SCHED_H__

#include <defs.h>
#include <list.h>
#include <skew_heap.h>

#define MAX_TIME_SLICE 5

struct proc_struct;

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

struct run_queue {
    list_entry_t run_list;
    unsigned int proc_num;
    int max_time_slice;
    // For LAB6 ONLY
    skew_heap_entry_t *lab6_run_pool;
};

void sched_init(void);
void wakeup_proc(struct proc_struct *proc);
void schedule(void);
void sched_class_proc_tick(struct proc_struct *proc);

#endif /* !__KERN_SCHEDULE_SCHED_H__ */

