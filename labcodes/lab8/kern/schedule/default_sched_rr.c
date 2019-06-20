#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched_rr.h>

/**
 * 初始化 Round Rubin 调度算法
 */
static void
RR_init(struct run_queue *rq) {
    list_init(&(rq->run_list));
    rq->proc_num = 0;
}

/**
 * 将进程加入到调度队列中等待下一次调度。
 * 遇到以下两种情况会将进程进入调度队列并（唤醒进程或让出 CPU）
 * 如果当前进程被唤醒（比如拿到了等待的资源，不需要继续等待了，通过 `wake_up` 函数调用；
 * 如果当前进程时间片已到，需要再次调度时
 */
static void
RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    // 将进程加入到调度队列中
    list_add_before(&(rq->run_list), &(proc->run_link));
    // 如果进程的时间片已到，那么重置该进程的时间片
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq; // proc 进程
    rq->proc_num ++; // 待调度的进程数 + 1
}

/**
 * 将进程从调度队列中删除。
 * 调用此函数后一般会调用 `proc_run` 让进程抢占 CPU。
 */
static void
RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link)); // 从调度队列中删除
    rq->proc_num --; // 待调度的进程数 - 1
}

/**
 * 选择一个进程抢占 CPU
 * Round Rubin 算法选择最先失去 CPU 执行权限的进程调度
 */
static struct proc_struct *
RR_pick_next(struct run_queue *rq) {
    // 从队列头取出一个进程
    // 由于时间片到期的进程被加入队尾，
    // 因此队列头是最长尚未获得执行权的进程，给予这个进程执行权
    list_entry_t *le = list_next(&(rq->run_list));
    if (le != &(rq->run_list)) {
        return le2proc(le, run_link);
    }
    return NULL;
}

/**
 * 时间中断调度处理函数
 * 每次时间中断后维护当前进程的时间片，
 * 如果时间片到期了，执行调度
 */
static void
RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    if (proc->time_slice == 0) {
        // 将当前进程的 need_reschedule 设为 true
        // 这样 trap 函数最后就会调用 schedule 函数以执行调度
        proc->need_resched = 1;
    }
}

struct sched_class rr_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
};

