/*
 * litmus/fp_common.c
 *
 * Common functions for fixed-priority scheduler.
 */

#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/list.h>

#include <litmus/litmus.h>
#include <litmus/sched_plugin.h>
#include <litmus/sched_trace.h>

#include <litmus/fp_common.h>

/* fp_higher_prio -  returns true if first has a higher static priority
 *                   than second. Deadline ties are broken by PID.
 *
 * both first and second may be NULL
 */
int fp_higher_prio(struct task_struct* first,
		   struct task_struct* second)
{
	struct task_struct *first_task = first;
	struct task_struct *second_task = second;

	/* There is no point in comparing a task to itself. */
	if (unlikely(first && first == second)) {
		TRACE_TASK(first,
			   "WARNING: pointless FP priority comparison.\n");
		return 0;
	}


	/* check for NULL tasks */
	if (!first || !second)
		return first && !second;

#ifdef CONFIG_LITMUS_LOCKING

	/* Check for inherited priorities. Change task
	 * used for comparison in such a case.
	 */
	if (unlikely(first->rt_param.inh_task))
		first_task = first->rt_param.inh_task;
	if (unlikely(second->rt_param.inh_task))
		second_task = second->rt_param.inh_task;

	/* Check for priority boosting. Tie-break by start of boosting.
	 */
	if (unlikely(is_priority_boosted(first_task))) {
		/* first_task is boosted, how about second_task? */
		if (!is_priority_boosted(second_task) ||
		    lt_before(get_boost_start(first_task),
			      get_boost_start(second_task)))
			return 1;
		else
			return 0;
	} else if (unlikely(is_priority_boosted(second_task)))
		/* second_task is boosted, first is not*/
		return 0;

#endif


	return !is_realtime(second_task)  ||

		get_priority(first_task) < get_priority(second_task) ||

		/* Break by PID.
		 */
		(get_priority(first_task) == get_priority(second_task) &&
	        (first_task->pid < second_task->pid ||

		/* If the PIDs are the same then the task with the inherited
		 * priority wins.
		 */
		(first_task->pid == second_task->pid &&
		 !second->rt_param.inh_task)));
}

int fp_ready_order(struct bheap_node* a, struct bheap_node* b)
{
	return fp_higher_prio(bheap2task(a), bheap2task(b));
}

void fp_domain_init(rt_domain_t* rt, check_resched_needed_t resched,
		    release_jobs_t release)
{
	rt_domain_init(rt,  fp_ready_order, resched, release);
}

/* need_to_preempt - check whether the task t needs to be preempted
 */
int fp_preemption_needed(struct fp_prio_queue *q, struct task_struct *t)
{
	struct task_struct *pending;

	pending = fp_prio_peek(q);

	if (!pending)
		return 0;
	if (!t)
		return 1;

	/* make sure to get non-rt stuff out of the way */
	return !is_realtime(t) || fp_higher_prio(pending, t);
}

void fp_prio_queue_init(struct fp_prio_queue* q)
{
	int i;

	for (i = 0; i < FP_PRIO_BIT_WORDS; i++)
		q->bitmask[i] = 0;
	for (i = 0; i < LITMUS_MAX_PRIORITY; i++)
		bheap_init(&q->queue[i]);
}
