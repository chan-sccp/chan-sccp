
/*!
 * \file        sccp_threadpool.c
 * \brief       SCCP Threadpool Class
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \note        Based on the work of Johan Hanssen Seferidis
 *              Library providing a threading pool where you can add work. 
 * \since       2009-01-16
 * \remarks     Purpose:        SCCP Hint
 *              When to use:    Does the business of hint status
 *
 * $Date: 2011-01-04 17:29:12 +0100 (Tue, 04 Jan 2011) $
 * $Revision: 2215 $
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2215 $")
#include "sccp_threadpool.h"
#include <signal.h>
#undef pthread_create
#if defined(__GNUC__) && __GNUC__ > 3 && defined(HAVE_SYS_INFO_H)
#include <sys/sysinfo.h>											// to retrieve processor info
#endif
#define SEMAPHORE_LOCKED	(0)
#define SEMAPHORE_UNLOCKED	(1)
void sccp_threadpool_grow(sccp_threadpool_t * tp_p, int amount);
void sccp_threadpool_shrink(sccp_threadpool_t * tp_p, int amount);

typedef struct sccp_threadpool_thread sccp_threadpool_thread_t;

struct sccp_threadpool_thread {
	pthread_t thread;
	int die;
	sccp_threadpool_t *tp_p;
	SCCP_LIST_ENTRY (sccp_threadpool_thread_t) list;
};

/* The threadpool */
struct sccp_threadpool {
	//      pthread_t *threads;                                                     /*!< pointer to threads' ID   */
	//      int threadsN;                                                           /*!< amount of threads        */
	SCCP_LIST_HEAD (, sccp_threadpool_job_t) jobs;
	SCCP_LIST_HEAD (, sccp_threadpool_thread_t) threads;
	ast_cond_t work;
	ast_cond_t exit;
	time_t last_size_check;											/*!< Time since last size check */
	time_t last_resize;											/*!< Time since last resize */
	int job_high_water_mark;										/*!< Highest number of jobs outstanding since last resize check */
};

/* 
 * Fast reminders:
 * 
 * tp                   = threadpool 
 * sccp_threadpool      = threadpool
 * sccp_threadpool_t    = threadpool type
 * tp_p                 = threadpool pointer
 * sem                  = semaphore
 * xN                   = x can be any string. N stands for amount
 * */
static volatile int sccp_threadpool_shuttingdown = 0;

/* Initialise thread pool */
sccp_threadpool_t *sccp_threadpool_init(int threadsN)
{
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Starting Threadpool\n");
	sccp_threadpool_t *tp_p;

#if defined(__GNUC__) && __GNUC__ > 3 && defined(HAVE_SYS_INFO_H)
	threadsN = get_nprocs_conf();										// get current number of active processors
#endif
	if (!threadsN || threadsN < THREADPOOL_MIN_SIZE)
		threadsN = THREADPOOL_MIN_SIZE;

	if (threadsN > THREADPOOL_MAX_SIZE)
		threadsN = THREADPOOL_MAX_SIZE;

	/* Make new thread pool */
	tp_p = (sccp_threadpool_t *) malloc(sizeof(sccp_threadpool_t));						/* MALLOC thread pool */
	if (tp_p == NULL) {
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Could not allocate memory for thread pool\n");
		return NULL;
	}

	/* initialize the thread pool */
	SCCP_LIST_HEAD_INIT(&tp_p->threads);

	/* Initialise the job queue */
	SCCP_LIST_HEAD_INIT(&tp_p->jobs);
	tp_p->last_size_check = time(0);
	tp_p->job_high_water_mark = 0;
	tp_p->last_resize = time(0);

	/* Initialise Condition */
	ast_cond_init(&(tp_p->work), NULL);
	ast_cond_init(&(tp_p->exit), NULL);

	/* Make threads in pool */
	SCCP_LIST_LOCK(&(tp_p->threads));
	sccp_threadpool_grow(tp_p, threadsN);
	SCCP_LIST_UNLOCK(&(tp_p->threads));

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Threadpool Started\n");
	return tp_p;
}

// sccp_threadpool_grow needs to be called with locked &(tp_p->threads)->lock
void sccp_threadpool_grow(sccp_threadpool_t * tp_p, int amount)
{
	pthread_attr_t attr;
	sccp_threadpool_thread_t *tp_thread;
	int t;

	if (tp_p && !sccp_threadpool_shuttingdown) {
		for (t = 0; t < amount; t++) {
			tp_thread = malloc(sizeof(sccp_threadpool_thread_t));
			if (tp_thread == NULL) {
				pbx_log(LOG_ERROR, "sccp_threadpool_init(): Could not allocate memory for thread\n");
				return;
			}
			tp_thread->die = 0;
			tp_thread->tp_p = tp_p;

			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
			SCCP_LIST_INSERT_HEAD(&(tp_p->threads), tp_thread, list);
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Create thread %d in pool \n", t);
			pbx_pthread_create(&(tp_thread->thread), &attr, (void *) sccp_threadpool_thread_do, (void *) tp_thread);
			ast_cond_broadcast(&(tp_p->work));
		}
	}
}

// sccp_threadpool_shrink needs to be called with locked &(tp_p->threads)->lock
void sccp_threadpool_shrink(sccp_threadpool_t * tp_p, int amount)
{
	sccp_threadpool_thread_t *tp_thread;
	int t;

	if (tp_p && !sccp_threadpool_shuttingdown) {
		for (t = 0; t < amount; t++) {
			tp_thread = SCCP_LIST_FIRST(&(tp_p->threads));
			tp_thread->die = 1;
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Sending die signal to thread %d in pool \n", (unsigned int) tp_thread->thread);
			// wake up all threads
			ast_cond_broadcast(&(tp_p->work));
		}
	}
}

/* check threadpool size (increase/decrease if necessary) */
static void sccp_threadpool_check_size(sccp_threadpool_t * tp_p)
{
	if (tp_p && !sccp_threadpool_shuttingdown) {
		sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_check_resize) in thread: %d\n", (unsigned int) pthread_self());
		SCCP_LIST_LOCK(&(tp_p->threads));
		{
			if (SCCP_LIST_GETSIZE(tp_p->jobs) > (SCCP_LIST_GETSIZE(tp_p->threads) * 2) && SCCP_LIST_GETSIZE(tp_p->threads) < THREADPOOL_MAX_SIZE) {	// increase
				sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Add new thread to threadpool %p\n", tp_p);
				sccp_threadpool_grow(tp_p, 1);
				tp_p->last_resize = time(0);
			} else if (((time(0) - tp_p->last_resize) > THREADPOOL_RESIZE_INTERVAL * 3) &&		// wait a little longer to decrease
				   (SCCP_LIST_GETSIZE(tp_p->threads) > THREADPOOL_MIN_SIZE && SCCP_LIST_GETSIZE(tp_p->jobs) < (SCCP_LIST_GETSIZE(tp_p->threads) / 2))) {	// decrease
				sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Remove thread %d from threadpool %p\n", SCCP_LIST_GETSIZE(tp_p->threads) - 1, tp_p);
				// kill last thread only if it is not executed by itself
				sccp_threadpool_shrink(tp_p, 1);
				tp_p->last_resize = time(0);
			}
			tp_p->last_size_check = time(0);
			tp_p->job_high_water_mark = SCCP_LIST_GETSIZE(tp_p->jobs);
			sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_check_resize) Number of threads: %d, job_high_water_mark: %d\n", SCCP_LIST_GETSIZE(tp_p->threads), tp_p->job_high_water_mark);
		}
		SCCP_LIST_UNLOCK(&(tp_p->threads));
	}
}

void sccp_threadpool_thread_end(void *p)
{
	sccp_threadpool_thread_t *tp_thread = (sccp_threadpool_thread_t *) p;
	sccp_threadpool_thread_t *res = NULL;
	sccp_threadpool_t *tp_p = tp_thread->tp_p;

	SCCP_LIST_LOCK(&(tp_p->threads));
	res = SCCP_LIST_REMOVE(&(tp_p->threads), tp_thread, list);
	SCCP_LIST_UNLOCK(&(tp_p->threads));

	ast_cond_signal(&(tp_p->exit));
	if (res)
		free(res);
}

/* What each individual thread is doing */
void sccp_threadpool_thread_do(void *p)
{
	sccp_threadpool_thread_t *tp_thread = (sccp_threadpool_thread_t *) p;
	sccp_threadpool_t *tp_p = tp_thread->tp_p;
	uint threadid = (unsigned int) pthread_self();

	pthread_cleanup_push(sccp_threadpool_thread_end, tp_thread);

	int jobs = 0, threads = 0;

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Starting Threadpool JobQueue\n");
	while (1) {
		pthread_testcancel();
		jobs = SCCP_LIST_GETSIZE(tp_p->jobs);
		threads = SCCP_LIST_GETSIZE(tp_p->threads);

		sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) num_jobs: %d, threadid: %d, num_threads: %d\n", jobs, threadid, threads);

		SCCP_LIST_LOCK(&(tp_p->jobs));									/* LOCK */
		while (SCCP_LIST_GETSIZE(tp_p->jobs) == 0 && !tp_thread->die) {
			if (tp_thread->die && SCCP_LIST_GETSIZE(tp_p->jobs) == 0) {
				sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "JobQueue Die. Exiting thread %d...\n", threadid);
				SCCP_LIST_UNLOCK(&(tp_p->jobs));
				pthread_exit(0);
			}
			sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) Thread %d Waiting for New Work Condition\n", threadid);
			ast_cond_wait(&(tp_p->work), &(tp_p->jobs.lock));
		}
		if (tp_thread->die && SCCP_LIST_GETSIZE(tp_p->jobs) == 0) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "JobQueue Die. Exiting thread %d...\n", threadid);
			SCCP_LIST_UNLOCK(&(tp_p->jobs));
			pthread_exit(0);
		}
		sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) Let's work. num_jobs: %d, threadid: %d, num_threads: %d\n", jobs, threadid, threads);
		{
			/* Read job from queue and execute it */
			void *(*func_buff) (void *arg) = NULL;
			void *arg_buff = NULL;
			sccp_threadpool_job_t *job;

			if ((job = SCCP_LIST_REMOVE_HEAD(&(tp_p->jobs), list))) {
				func_buff = job->function;
				arg_buff = job->arg;
			}
			SCCP_LIST_UNLOCK(&(tp_p->jobs));

			sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) executing %p in threadid: %d\n", job, threadid);
			if (job) {
				func_buff(arg_buff);								/* run function */
				free(job);									/* DEALLOC job */
			}
			// check number of threads in threadpool
			if ((time(0) - tp_p->last_size_check) > THREADPOOL_RESIZE_INTERVAL) {
				sccp_threadpool_check_size(tp_p);						/* Check Resizing */
			}
		}
	}
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "JobQueue Exiting Thread...\n");
	pthread_cleanup_pop(1);
	return;
}

/* Add work to the thread pool */
int sccp_threadpool_add_work(sccp_threadpool_t * tp_p, void *(*function_p) (void *), void *arg_p)
{
	// prevent new work while shutting down
	if (!sccp_threadpool_shuttingdown) {
		sccp_threadpool_job_t *newJob;

		newJob = (sccp_threadpool_job_t *) malloc(sizeof(sccp_threadpool_job_t));			/* MALLOC job */
		if (newJob == NULL) {
			pbx_log(LOG_ERROR, "sccp_threadpool_add_work(): Could not allocate memory for new job\n");
			exit(1);
		}

		/* add function and argument */
		newJob->function = function_p;
		newJob->arg = arg_p;

		/* add job to queue */
		sccp_threadpool_jobqueue_add(tp_p, newJob);
		return 1;
	} else {
		pbx_log(LOG_ERROR, "sccp_threadpool_add_work(): Threadpool shutting down, denying new work\n");
		return 0;
	}
}

/* Destroy the threadpool */
void sccp_threadpool_destroy(sccp_threadpool_t * tp_p)
{
	sccp_threadpool_thread_t *tp_thread = NULL;

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Destroying Threadpool %p with %d jobs\n", tp_p, SCCP_LIST_GETSIZE(tp_p->jobs));

	// After this point, no new jobs can be added
	SCCP_LIST_LOCK(&(tp_p->jobs));
	sccp_threadpool_shuttingdown = 1;
	SCCP_LIST_UNLOCK(&(tp_p->jobs));

	// shutdown is a kind of work too
	SCCP_LIST_LOCK(&(tp_p->threads));
	SCCP_LIST_TRAVERSE(&(tp_p->threads), tp_thread, list) {
		tp_thread->die = 1;
		ast_cond_broadcast(&(tp_p->work));
	}
	SCCP_LIST_UNLOCK(&(tp_p->threads));

	// wake up jobs untill jobqueue is empty, before shutting down, to make sure all jobs have been processed
	ast_cond_broadcast(&(tp_p->work));

	// wait for all threads to exit
	if (SCCP_LIST_GETSIZE(tp_p->threads) != 0) {
		struct timespec ts;
		struct timeval tp;
		int counter = 0;

		SCCP_LIST_LOCK(&(tp_p->threads));
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Waiting for threadpool to wind down. please stand by...\n");
		while (SCCP_LIST_GETSIZE(tp_p->threads) != 0 && counter++ < THREADPOOL_MAX_SIZE) {
			gettimeofday(&tp, NULL);
			ts.tv_sec = tp.tv_sec;
			ts.tv_nsec = tp.tv_usec * 1000;
			ts.tv_sec += 1;										// wait max 2 second
			ast_cond_broadcast(&(tp_p->work));
			ast_cond_timedwait(&tp_p->exit, &(tp_p->threads.lock), &ts);
		}

		/* Make sure threads have finished (should never have to execute) */
		if (SCCP_LIST_GETSIZE(tp_p->threads) != 0) {
			while ((tp_thread = SCCP_LIST_REMOVE_HEAD(&(tp_p->threads), list))) {
				pbx_log(LOG_ERROR, "Forcing Destroy of thread %p\n", tp_thread);
				pthread_cancel(tp_thread->thread);
				pthread_kill(tp_thread->thread, SIGURG);
				pthread_join(tp_thread->thread, NULL);
			}
		}
		SCCP_LIST_UNLOCK(&(tp_p->threads));
	}

	/* Dealloc */
	ast_cond_destroy(&(tp_p->work));									/* Remove Condition */
	ast_cond_destroy(&(tp_p->exit));									/* Remove Condition */
	SCCP_LIST_HEAD_DESTROY(&(tp_p->jobs));
	SCCP_LIST_HEAD_DESTROY(&(tp_p->threads));
	free(tp_p);												/* DEALLOC thread pool */
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Threadpool Ended\n");
}

int sccp_threadpool_thread_count(sccp_threadpool_t * tp_p)
{
	return SCCP_LIST_GETSIZE(tp_p->threads);
}

/* =================== JOB QUEUE OPERATIONS ===================== */

/* Add job to queue */
void sccp_threadpool_jobqueue_add(sccp_threadpool_t * tp_p, sccp_threadpool_job_t * newjob_p)
{
	if (!tp_p) {
		pbx_log(LOG_ERROR, "(sccp_threadpool_jobqueue_add) no tp_p\n");
		sccp_free(newjob_p);
		return;
	}

	sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_jobqueue_add) tp_p: %p, jobCount: %d\n", tp_p, SCCP_LIST_GETSIZE(tp_p->jobs));
	SCCP_LIST_LOCK(&(tp_p->jobs));
	if (sccp_threadpool_shuttingdown) {
		pbx_log(LOG_ERROR, "(sccp_threadpool_jobqueue_add) shutting down\n");
		SCCP_LIST_UNLOCK(&(tp_p->jobs));
		sccp_free(newjob_p);
		return;
	}
	SCCP_LIST_INSERT_TAIL(&(tp_p->jobs), newjob_p, list);
	SCCP_LIST_UNLOCK(&(tp_p->jobs));

	if (SCCP_LIST_GETSIZE(tp_p->jobs) > tp_p->job_high_water_mark) {
		tp_p->job_high_water_mark = SCCP_LIST_GETSIZE(tp_p->jobs);
	}
	ast_cond_signal(&(tp_p->work));
}

int sccp_threadpool_jobqueue_count(sccp_threadpool_t * tp_p)
{
	sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_jobqueue_count) tp_p: %p, jobCount: %d\n", tp_p, SCCP_LIST_GETSIZE(tp_p->jobs));
	return SCCP_LIST_GETSIZE(tp_p->jobs);
}
