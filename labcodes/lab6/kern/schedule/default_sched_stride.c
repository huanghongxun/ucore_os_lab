#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

#define USE_SKEW_HEAP 1

/* You should define the BigStride constant here*/
/* LAB6: YOUR CODE */
#define BIG_STRIDE (1<<30)   /* you should give a value, and is ??? */

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
 * （我们使用优先队列存储）。本函数还会更新 rq 的 meta date。
 *
 * proc->time_slice denotes the time slices allocation for the
 * process, which should set to rq->max_time_slice.
 * 
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     // (1) insert the proc into rq correctly
     // list_add_before(&rq->run_list, &proc->run_link);
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &proc->lab6_run_pool, proc_stride_comp_f);

     if (proc->time_slice <= 0 || proc->time_slice > rq->max_time_slice)
          proc->time_slice = rq->max_time_slice; // (2) recalculate proc->time_slice
     proc->rq = rq; // (3) set proc->rq pointer to rq
     ++rq->proc_num; // (4) increase rq->proc_num
}

/**
 * @brief 从运行队列 rq 中删除进程 proc。
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE 
      * (1) remove the proc from rq correctly
      * NOTICE: you can use skew_heap or list. Important functions
      *         skew_heap_remove: remove a entry from skew_heap
      *         list_del_init: remove a entry from the  list
      */
     // list_del(&proc->run_link);
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &proc->lab6_run_pool, proc_stride_comp_f);
     proc->rq = NULL;
     --rq->proc_num;
}

/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq) {
     /* LAB6: YOUR CODE 
      * (1) get a  proc_struct pointer p  with the minimum value of stride
             (1.1) If using skew_heap, we can use le2proc get the p from rq->lab6_run_poll
             (1.2) If using list, we have to search list to find the p with minimum stride value
      * (2) update p;s stride value: p->lab6_stride
      * (3) return p
      */
     if (!rq->lab6_run_pool) return NULL;
     struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
     assert(p->lab6_priority != 0);
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &p->lab6_run_pool, proc_stride_comp_f);
     p->lab6_stride += BIG_STRIDE / p->lab6_priority;
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &p->lab6_run_pool, proc_stride_comp_f);
     return p;
}

/*
 * stride_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE */
     if (--proc->time_slice <= 0) {
          proc->time_slice = 0;
          proc->need_resched = true;
     }
}

struct sched_class default_sched_class = {
     .name = "stride_scheduler",
     .init = stride_init,
     .enqueue = stride_enqueue,
     .dequeue = stride_dequeue,
     .pick_next = stride_pick_next,
     .proc_tick = stride_proc_tick,
};
