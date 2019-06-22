#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched_mlfq.h>

#define QUEUE_NUM 6

static list_entry_t run_list[QUEUE_NUM];
static int max_slice_factor[QUEUE_NUM];

/**
 * 初始化 MLFQ 调度算法
 */
static void
MLFQ_init(struct run_queue *rq) {
    for (int i = 0; i < QUEUE_NUM; ++i) {
        list_init(&run_list[i]);
        if (i == 0) max_slice_factor[i] = 1;
        else max_slice_factor[i] = max_slice_factor[i - 1] * 2;
    }
    rq->proc_num = 0;
}

/**
 * 将进程加入到调度队列中等待下一次调度。
 * 遇到以下两种情况会将进程进入调度队列并（唤醒进程或让出 CPU）
 * 如果当前进程被唤醒（比如拿到了等待的资源，不需要继续等待了，通过 `wake_up` 函数调用；
 * 如果当前进程时间片已到，需要再次调度时
 */
static void
MLFQ_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    assert(proc->lab6_stride < QUEUE_NUM);
    // 将进程加入到调度队列中
    list_add_before(&run_list[proc->lab6_stride], &(proc->run_link));
    // 如果进程的时间片已到，那么重置该进程的时间片
    int max_time_slice = rq->max_time_slice * max_slice_factor[proc->lab6_stride];
    if (proc->time_slice == 0 || proc->time_slice > max_time_slice) {
        proc->time_slice = max_time_slice;
    }
    proc->rq = rq; // proc 进程
    rq->proc_num ++; // 待调度的进程数 + 1
}

/**
 * 将进程从调度队列中删除。
 * 调用此函数后一般会调用 `proc_run` 让进程抢占 CPU。
 */
static void
MLFQ_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link)); // 从调度队列中删除
    rq->proc_num --; // 待调度的进程数 - 1
}

/**
 * 选择一个进程抢占 CPU
 * Round Rubin 算法选择最先失去 CPU 执行权限的进程调度
 */
static struct proc_struct *
MLFQ_pick_next(struct run_queue *rq) {
    // 从队列头取出一个进程
    // 由于时间片到期的进程被加入队尾，
    // 因此队列头是最长尚未获得执行权的进程，给予这个进程执行权
    for (int i = 0; i < QUEUE_NUM; ++i)
        if (!list_empty(&run_list[i])) {
            list_entry_t *le = list_next(&run_list[i]);
            struct proc_struct *p = le2proc(le, run_link);
            if (p->lab6_stride + 1 < QUEUE_NUM)
                p->lab6_stride++;
            return p;
        }
    return NULL;
}

/**
 * 时间中断调度处理函数
 * 每次时间中断后维护当前进程的时间片，
 * 如果时间片到期了，执行调度
 */
static void
MLFQ_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    if (proc->time_slice == 0) {
        // 将当前进程的 need_reschedule 设为 true
        // 这样 trap 函数最后就会调用 schedule 函数以执行调度
        proc->need_resched = 1;
    }
}

struct sched_class mlfq_sched_class = {
    .name = "MLFQ_scheduler",
    .init = MLFQ_init,
    .enqueue = MLFQ_enqueue,
    .dequeue = MLFQ_dequeue,
    .pick_next = MLFQ_pick_next,
    .proc_tick = MLFQ_proc_tick,
};

