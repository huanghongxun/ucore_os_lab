#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

#define USE_SKEW_HEAP 1

/* You should define the BigStride constant here*/
/* LAB6: YOUR CODE */
#define BIG_STRIDE (1<<10)   /* you should give a value, and is ??? */

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
static int
proc_stride_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, lab6_run_pool);
     struct proc_struct *q = le2proc(b, lab6_run_pool);
     int32_t c = p->lab6_stride - q->lab6_stride;
     if (c > 0) return 1;
     else if (c == 0) return 0;
     else return -1;
}

/**
 * @brief 初始化运行队列 rq
 */
static void
stride_init(struct run_queue *rq) {
     /* LAB6: YOUR CODE */
     // (1) init the ready process list: rq->run_list: should be a empty list after initialization.
     list_init(&rq->run_list);
     // (2) init the run pool: rq->lab6_run_pool: NULL
     rq->lab6_run_pool = NULL;
     // (3) set number of process: rq->proc_num to 0
     rq->proc_num = 0;
     // max_time_slice: no need here, the variable would be assigned by the caller.
}

/**
 * @brief 将 proc 插入到运行队列 rq 中。
 * 本函数将会检查/初始化 proc 的相关成员，并且将 lab6_run_pool 插入到队列中
 * （我们使用左偏树来存储）。本函数还会更新 rq 的 meta date。
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     // (1) 将 proc 插入到调度队列中，一种方法是使用队列维护
     // list_add_before(&rq->run_list, &proc->run_link);
     // 另一种方法是使用左偏树来进行维护
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &proc->lab6_run_pool, proc_stride_comp_f);

     // (2) 重新计算进程的时间片
     if (proc->time_slice <= 0 || proc->time_slice > rq->max_time_slice)
          proc->time_slice = rq->max_time_slice;
     // (3) 将当前进程的调度队列 rq 设为当前的 rq
     proc->rq = rq;
     // (4) 当前的调度队列 rq 维护的待调度进程数 + 1
     ++rq->proc_num;
}

/**
 * @brief 从运行队列 rq 中删除进程 proc。
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     // (1) 将进程从调度队列中删除
     // list_del_init(&proc->run_link);
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &proc->lab6_run_pool, proc_stride_comp_f);
     // (2) 标记进程不从属于任意一个调度队列
     proc->rq = NULL;
     // (3) 当前的调度队列 rq 维护的待调度进程数 - 1
     --rq->proc_num;
}

/**
 * @brief 从调度队列中选择一个进程调度
 * Stride Scheduling 算法选择 stride 值最小的进程占用 PCU。
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq) {
     /* LAB6: YOUR CODE */
     // (1) 从调度队列中找到 stride 值最小的进程 p
#if USE_SKEW_HEAP
     // (1.1) If using skew_heap, we can use le2proc get the p from rq->lab6_run_pool
     if (!rq->lab6_run_pool) return NULL;
     struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
#else
     // (1.2) If using list, we have to search list to find the p with minimum stride value
     if (list_empty(&rq->run_list)) return NULL;
     list_entry_t *le = list_next(&rq->run_list);
     struct proc_struct *p = le2proc(le, run_link);
     while ((le = list_next(le)) != &rq->run_list) {
          struct proc_struct *q = le2proc(le, run_link);
          if (p->lab6_stride > q->lab6_stride)
               p = q;
     }
#endif
     // (2) 更新进程 p 的 stride 值
     assert(p->lab6_priority != 0);
#if USE_SKEW_HEAP
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &p->lab6_run_pool, proc_stride_comp_f);
#endif
     // stride += 进程 p 的步长
     p->lab6_stride += BIG_STRIDE / p->lab6_priority;
#if USE_SKEW_HEAP
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &p->lab6_run_pool, proc_stride_comp_f);
#endif
     return p;
}

/**
 * @brief 该函数在时钟中断时触发，更新当前进程的状态。
 * 该函数会更新当前进程的时间片，如果时间片耗尽，那么该进程就需要被调度。
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     if (--proc->time_slice <= 0) { // 如果时间片耗尽
          proc->time_slice = 0;
          proc->need_resched = true; // 标记当前进程需要被调度
     }
}

struct sched_class stride_sched_class = {
     .name = "stride_scheduler",
     .init = stride_init,
     .enqueue = stride_enqueue,
     .dequeue = stride_dequeue,
     .pick_next = stride_pick_next,
     .proc_tick = stride_proc_tick,
};
