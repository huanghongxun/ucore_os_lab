#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <default_sched_rr.h>
#include <default_sched_stride.h>

// the list of timer
static list_entry_t timer_list;

static struct sched_class *sched_class;

static struct run_queue *rq;

static inline void
sched_class_enqueue(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->enqueue(rq, proc);
    }
}

static inline void
sched_class_dequeue(struct proc_struct *proc) {
    sched_class->dequeue(rq, proc);
}

static inline struct proc_struct *
sched_class_pick_next(void) {
    return sched_class->pick_next(rq);
}

void
sched_class_proc_tick(struct proc_struct *proc) {
    if (proc != idleproc) {
        // 对于一般进程，交由调度算法检查是否需要调度
        // 比如检查时间片是否已到
        sched_class->proc_tick(rq, proc);
    }
    else {
        // 确保 idle 进程始终会被调度算法尝试进行调度
        proc->need_resched = 1;
    }
}

static struct run_queue __rq;

void
sched_init(void) {
    list_init(&timer_list);

    sched_class = &stride_sched_class;

    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
}

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current) {
                sched_class_enqueue(proc);
            }
        }
        else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

/**
 * 调度程序，选择一个可以运行的进程抢占 CPU
 */
void schedule(void) {
    bool intr_flag;
    struct proc_struct *next;
    // 调度时中断敏感，我们需要先关中断
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;

        // 如果当前的进程还是可执行的，我们将该进程
        // 标记为待调度的状态
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);
        }

        // 选择一个进程抢占 CPU
        if ((next = sched_class_pick_next()) != NULL) {
            sched_class_dequeue(next);
        }

        // 如果不存在可以调度的进程，这说明当前没有进程
        // 可以运行，则执行 idle 进程不断尝试进行调度
        if (next == NULL) {
            next = idleproc;
        }
        next->runs ++;
        // 切换上下文给该进程
        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
