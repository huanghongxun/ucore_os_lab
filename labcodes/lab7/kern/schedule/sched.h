#ifndef __KERN_SCHEDULE_SCHED_H__
#define __KERN_SCHEDULE_SCHED_H__

#include <defs.h>
#include <list.h>
#include <skew_heap.h>

#define MAX_TIME_SLICE 5

struct proc_struct;

typedef struct {
    unsigned int expires;       //the expire time
    struct proc_struct *proc;   //the proc wait in this timer. If the expire time is end, then this proc will be scheduled
    list_entry_t timer_link;    //the timer list
} timer_t;

#define le2timer(le, member)            \
to_struct((le), timer_t, member)

// init a timer
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
void add_timer(timer_t *timer);     // add timer to timer_list
void del_timer(timer_t *timer);     // del timer from timer_list
void run_timer_list(void);          // call scheduler to update tick related info, and check the timer is expired? If expired, then wakup proc

#endif /* !__KERN_SCHEDULE_SCHED_H__ */

