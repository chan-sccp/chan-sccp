/*!
 * \file        sccp_threadpool.c
 * \brief       SCCP Threadpool Class
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \note        Based on the work of Johan Hanssen Seferidis
 *              Library providing a threading pool where you can add work. 
 * \since       2009-01-16
 *
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");
#include "sccp_threadpool.h"
#include <signal.h>
#undef pthread_create
#if defined(__GNUC__) && __GNUC__ > 3 && defined(HAVE_SYS_INFO_H)
#include <sys/sysinfo.h>											// to retrieve processor info
#endif
//#define SEMAPHORE_LOCKED	(0)
//#define SEMAPHORE_UNLOCKED	(1)
void sccp_threadpool_grow(sccp_threadpool_t * tp_p, int amount);
void sccp_threadpool_shrink(sccp_threadpool_t * tp_p, int amount);

typedef struct sccp_threadpool_thread sccp_threadpool_thread_t;

struct sccp_threadpool_thread {
	pthread_t thread;
	sccp_threadpool_t *tp_p;
	SCCP_LIST_ENTRY (sccp_threadpool_thread_t) list;
	boolean_t die;
};

/* The threadpool */
struct sccp_threadpool {
	SCCP_LIST_HEAD (, sccp_threadpool_job_t) jobs;
	SCCP_LIST_HEAD (, sccp_threadpool_thread_t) threads;
	pbx_cond_t work;
	pbx_cond_t exit;
	time_t last_size_check;											/*!< Time since last size check */
	time_t last_resize;											/*!< Time since last resize */
	int job_high_water_mark;										/*!< Highest number of jobs outstanding since last resize check */
	volatile int sccp_threadpool_shuttingdown;
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

/* Initialise thread pool */
sccp_threadpool_t *sccp_threadpool_init(int threadsN)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Starting Threadpool\n");
	sccp_threadpool_t *tp_p;

#if defined(__GNUC__) && __GNUC__ > 3 && defined(HAVE_SYS_INFO_H)
	threadsN = get_nprocs_conf();										// get current number of active processors
#endif
	if (!threadsN || threadsN < THREADPOOL_MIN_SIZE) {
		threadsN = THREADPOOL_MIN_SIZE;
	}
	if (threadsN > THREADPOOL_MAX_SIZE) {
		threadsN = THREADPOOL_MAX_SIZE;
	}
	/* Make new thread pool */
	if (!(tp_p = sccp_calloc(sizeof *tp_p, 1))) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return NULL;
	}

	/* initialize the thread pool */
	SCCP_LIST_HEAD_INIT(&tp_p->threads);

	/* Initialise the job queue */
	SCCP_LIST_HEAD_INIT(&tp_p->jobs);
	tp_p->last_size_check = time(0);
	tp_p->job_high_water_mark = 0;
	tp_p->last_resize = time(0);
	tp_p->sccp_threadpool_shuttingdown = 0;

	/* Initialise Condition */
	pbx_cond_init(&(tp_p->work), NULL);
	pbx_cond_init(&(tp_p->exit), NULL);

	/* Make threads in pool */
	SCCP_LIST_LOCK(&(tp_p->threads));
	sccp_threadpool_grow(tp_p, threadsN);
	SCCP_LIST_UNLOCK(&(tp_p->threads));

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Threadpool Started\n");
	return tp_p;
}

// sccp_threadpool_grow needs to be called with locked &(tp_p->threads)->lock
void sccp_threadpool_grow(sccp_threadpool_t * tp_p, int amount)
{
	pthread_attr_t attr;
	sccp_threadpool_thread_t *tp_thread;
	int t;

	if (tp_p && !tp_p->sccp_threadpool_shuttingdown) {
		for (t = 0; t < amount; t++) {
			if (!(tp_thread = sccp_calloc(sizeof *tp_thread, 1))) {
                		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				return;
			}
			tp_thread->die = FALSE;
			tp_thread->tp_p = tp_p;

			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
			SCCP_LIST_LOCK(&(tp_p->threads));
			SCCP_LIST_INSERT_HEAD(&(tp_p->threads), tp_thread, list);
			SCCP_LIST_UNLOCK(&(tp_p->threads));
			pbx_pthread_create(&(tp_thread->thread), &attr, (void *) sccp_threadpool_thread_do, (void *) tp_thread);
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Created thread %d(%p) in pool \n", t, (void *) tp_thread->thread);
			pbx_cond_broadcast(&(tp_p->work));
		}
	}
}

// sccp_threadpool_shrink needs to be called with locked &(tp_p->threads)->lock
void sccp_threadpool_shrink(sccp_threadpool_t * tp_p, int amount)
{
	sccp_threadpool_thread_t *tp_thread;
	int t;

	if (tp_p && !tp_p->sccp_threadpool_shuttingdown) {
		for (t = 0; t < amount; t++) {
			SCCP_LIST_LOCK(&(tp_p->threads));
			SCCP_LIST_TRAVERSE(&(tp_p->threads), tp_thread, list) {
				if (tp_thread->die == FALSE) {
					tp_thread->die = TRUE;
					break;
				}
			}
			SCCP_LIST_UNLOCK(&(tp_p->threads));
			
			if (tp_thread) {
				// wake up all threads
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Sending die signal to thread %p in pool \n", (void *) tp_thread->thread);
				pbx_cond_broadcast(&(tp_p->work));
			}
		}
	}
}

/* check threadpool size (increase/decrease if necessary) */
static void sccp_threadpool_check_size(sccp_threadpool_t * tp_p)
{
	if (tp_p && !tp_p->sccp_threadpool_shuttingdown) {
		sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_check_resize) in thread: %p\n", (void *) pthread_self());
		SCCP_LIST_LOCK(&(tp_p->threads));
		{
			if (SCCP_LIST_GETSIZE(&tp_p->jobs) > (SCCP_LIST_GETSIZE(&tp_p->threads) * 2) && SCCP_LIST_GETSIZE(&tp_p->threads) < THREADPOOL_MAX_SIZE) {	// increase
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Add new thread to threadpool %p\n", tp_p);
				sccp_threadpool_grow(tp_p, 1);
				tp_p->last_resize = time(0);
			} else if (((time(0) - tp_p->last_resize) > THREADPOOL_RESIZE_INTERVAL * 3) &&		// wait a little longer to decrease
				   (SCCP_LIST_GETSIZE(&tp_p->threads) > THREADPOOL_MIN_SIZE && SCCP_LIST_GETSIZE(&tp_p->jobs) < (SCCP_LIST_GETSIZE(&tp_p->threads) / 2))) {	// decrease
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Remove thread %d from threadpool %p\n", SCCP_LIST_GETSIZE(&tp_p->threads) - 1, tp_p);
				// kill last thread only if it is not executed by itself
				sccp_threadpool_shrink(tp_p, 1);
				tp_p->last_resize = time(0);
			}
			tp_p->last_size_check = time(0);
			tp_p->job_high_water_mark = SCCP_LIST_GETSIZE(&tp_p->jobs);
			sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_check_resize) Number of threads: %d, job_high_water_mark: %d\n", SCCP_LIST_GETSIZE(&tp_p->threads), tp_p->job_high_water_mark);
		}
		SCCP_LIST_UNLOCK(&(tp_p->threads));
	}
}

static void sccp_threadpool_thread_end(void *p)
{
	sccp_threadpool_thread_t *tp_thread = (sccp_threadpool_thread_t *) p;
	sccp_threadpool_thread_t *res = NULL;
	sccp_threadpool_t *tp_p = tp_thread->tp_p;

	SCCP_LIST_LOCK(&(tp_p->threads));
	res = SCCP_LIST_REMOVE(&(tp_p->threads), tp_thread, list);
	SCCP_LIST_UNLOCK(&(tp_p->threads));

	pbx_cond_signal(&(tp_p->exit));
	if (res) {
		sccp_free(res);
	}
}

/* What each individual thread is doing */
void sccp_threadpool_thread_do(void *p)
{
	sccp_threadpool_thread_t *tp_thread = (sccp_threadpool_thread_t *) p;
	sccp_threadpool_t *tp_p = tp_thread->tp_p;
	void *thread = (void *) pthread_self();

	pthread_cleanup_push(sccp_threadpool_thread_end, tp_thread);

	int jobs = 0, threads = 0;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Starting Threadpool JobQueue:%p\n", thread);
	while (1) {
		pthread_testcancel();
		jobs = SCCP_LIST_GETSIZE(&tp_p->jobs);
		threads = SCCP_LIST_GETSIZE(&tp_p->threads);

		sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) num_jobs: %d, thread: %p, num_threads: %d\n", jobs, thread, threads);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		SCCP_LIST_LOCK(&(tp_p->jobs));									/* LOCK */
		while (SCCP_LIST_GETSIZE(&tp_p->jobs) == 0 && !tp_thread->die) {
			sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) Thread %p Waiting for New Work Condition\n", thread);
			pbx_cond_wait(&(tp_p->work), &(tp_p->jobs.lock));
		}
		if (tp_thread->die && SCCP_LIST_GETSIZE(&tp_p->jobs) == 0) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "JobQueue Die. Exiting thread %p...\n", thread);
			SCCP_LIST_UNLOCK(&(tp_p->jobs));
			break;
		}
		sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) Let's work. num_jobs: %d, thread: %p, num_threads: %d\n", jobs, thread, threads);
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

			sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) executing %p in thread: %p\n", job, thread);
			if (job) {
				func_buff(arg_buff);								/* run function */
				sccp_free(job);									/* DEALLOC job */
			}
			// check number of threads in threadpool
			if ((time(0) - tp_p->last_size_check) > THREADPOOL_RESIZE_INTERVAL) {
				sccp_threadpool_check_size(tp_p);						/* Check Resizing */
			}
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "JobQueue Exiting Thread...\n");
	pthread_cleanup_pop(1);
	return;
}

/* Add work to the thread pool */
int sccp_threadpool_add_work(sccp_threadpool_t * tp_p, void *(*function_p) (void *), void *arg_p)
{
	// prevent new work while shutting down
	if (!tp_p->sccp_threadpool_shuttingdown) {
		sccp_threadpool_job_t *newJob;

		if (!(newJob = sccp_calloc(sizeof *newJob, 1))) {
        		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
			exit(1);
		}

		/* add function and argument */
		newJob->function = function_p;
		newJob->arg = arg_p;

		/* add job to queue */
		sccp_threadpool_jobqueue_add(tp_p, newJob);
		return 1;
	} 
        pbx_log(LOG_ERROR, "sccp_threadpool_add_work(): Threadpool shutting down, denying new work\n");
        return 0;
}

/* Destroy the threadpool */
boolean_t sccp_threadpool_destroy(sccp_threadpool_t * tp_p)
{
	if (!tp_p) {
		return FALSE;
	}
	sccp_threadpool_thread_t *tp_thread = NULL;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Destroying Threadpool %p with %d jobs\n", tp_p, SCCP_LIST_GETSIZE(&tp_p->jobs));

	// After this point, no new jobs can be added
	SCCP_LIST_LOCK(&(tp_p->jobs));
	tp_p->sccp_threadpool_shuttingdown = 1;
	SCCP_LIST_UNLOCK(&(tp_p->jobs));

	// shutdown is a kind of work too
	SCCP_LIST_LOCK(&(tp_p->threads));
	SCCP_LIST_TRAVERSE(&(tp_p->threads), tp_thread, list) {
		tp_thread->die = TRUE;
		pbx_cond_broadcast(&(tp_p->work));
	}
	SCCP_LIST_UNLOCK(&(tp_p->threads));

	// wake up jobs untill jobqueue is empty, before shutting down, to make sure all jobs have been processed
	pbx_cond_broadcast(&(tp_p->work));

	// wait for all threads to exit
	if (SCCP_LIST_GETSIZE(&tp_p->threads) != 0) {
		struct timespec ts;
		struct timeval tp;
		int counter = 0;

		SCCP_LIST_LOCK(&(tp_p->threads));
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Waiting for threadpool to wind down. please stand by...\n");
		while (SCCP_LIST_GETSIZE(&tp_p->threads) != 0 && counter++ < THREADPOOL_MAX_SIZE) {
			gettimeofday(&tp, NULL);
			ts.tv_sec = tp.tv_sec;
			ts.tv_nsec = tp.tv_usec * 1000;
			ts.tv_sec += 1;										// wait max 2 second
			pbx_cond_broadcast(&(tp_p->work));
			pbx_cond_timedwait(&tp_p->exit, &(tp_p->threads.lock), &ts);
		}

		/* Make sure threads have finished (should never have to execute) */
		if (SCCP_LIST_GETSIZE(&tp_p->threads) != 0) {
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
	pbx_cond_destroy(&(tp_p->work));									/* Remove Condition */
	pbx_cond_destroy(&(tp_p->exit));									/* Remove Condition */
	SCCP_LIST_HEAD_DESTROY(&(tp_p->jobs));
	SCCP_LIST_HEAD_DESTROY(&(tp_p->threads));
	sccp_free(tp_p);
	tp_p = NULL;												/* DEALLOC thread pool */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Threadpool Ended\n");
	return TRUE;
}

int __PURE__ sccp_threadpool_thread_count(sccp_threadpool_t * tp_p)
{
	return SCCP_LIST_GETSIZE(&tp_p->threads);
}

/* =================== JOB QUEUE OPERATIONS ===================== */

/* Add job to queue */
void sccp_threadpool_jobqueue_add(sccp_threadpool_t * tp_p, sccp_threadpool_job_t * newjob_p)
{
	if (!tp_p || !newjob_p) {
		pbx_log(LOG_ERROR, "(sccp_threadpool_jobqueue_add) no tp_p or no work pointer\n");
		sccp_free(newjob_p);
		return;
	}

	sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_jobqueue_add) tp_p: %p, jobCount: %d\n", tp_p, SCCP_LIST_GETSIZE(&tp_p->jobs));
	SCCP_LIST_LOCK(&(tp_p->jobs));
	if (tp_p->sccp_threadpool_shuttingdown) {
		pbx_log(LOG_ERROR, "(sccp_threadpool_jobqueue_add) shutting down. skipping work\n");
		SCCP_LIST_UNLOCK(&(tp_p->jobs));
		sccp_free(newjob_p);
		return;
	}
	SCCP_LIST_INSERT_TAIL(&(tp_p->jobs), newjob_p, list);
	SCCP_LIST_UNLOCK(&(tp_p->jobs));

	if ((int)SCCP_LIST_GETSIZE(&tp_p->jobs) > tp_p->job_high_water_mark) {
		tp_p->job_high_water_mark = SCCP_LIST_GETSIZE(&tp_p->jobs);
	}
	pbx_cond_signal(&(tp_p->work));
}

int sccp_threadpool_jobqueue_count(sccp_threadpool_t * tp_p)
{
	sccp_log((DEBUGCAT_THPOOL)) (VERBOSE_PREFIX_3 "(sccp_threadpool_jobqueue_count) tp_p: %p, jobCount: %d\n", tp_p, SCCP_LIST_GETSIZE(&tp_p->jobs));
	return SCCP_LIST_GETSIZE(&tp_p->jobs);
}


#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
#define NUM_WORK 50
#define test_category "/channels/chan_sccp/threadpool/"
static void *sccp_cli_threadpool_test_thread(void *data)
{
	int loop;
	int num_loops=rand() % 10000;
	for (loop = 0; loop < num_loops; loop++) {
		usleep(1);
	}
	return 0;
}

AST_TEST_DEFINE(sccp_threadpool_create_destroy)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "create_destroy";
			info->category = test_category;
			info->summary = "chan-sccp-b threadpool creation/destruction";
			info->description = "chan-sccp-b threadpool creation/destruction";
			return AST_TEST_NOT_RUN;
	        case TEST_EXECUTE:
	        	break;
	}
	sccp_threadpool_t *test_threadpool = NULL;

	pbx_test_status_update(test, "Destroy non-existing threadpool: %p\n", test_threadpool);
	sccp_threadpool_destroy(test_threadpool);
	pbx_test_validate(test, sccp_threadpool_destroy(test_threadpool) == FALSE);

	pbx_test_status_update(test, "Create threadpool\n");
	test_threadpool = sccp_threadpool_init(THREADPOOL_MIN_SIZE);
	pbx_test_validate(test, NULL != test_threadpool);

	int current_size = sccp_threadpool_thread_count(test_threadpool);
	pbx_test_status_update(test, "Grow threadpool by 3, current %d\n", current_size);
	sccp_threadpool_grow(test_threadpool, 3);
	pbx_test_validate(test, sccp_threadpool_thread_count(test_threadpool) == current_size + 3); 

	current_size = sccp_threadpool_thread_count(test_threadpool);
	pbx_test_status_update(test, "Shrink threadpool by 2, current %d\n", current_size);
	sccp_threadpool_shrink(test_threadpool, 2);
	sleep(1);
	pbx_test_validate(test, sccp_threadpool_thread_count(test_threadpool) == current_size - 2); 

	pbx_test_status_update(test, "Destroy threadpool: %p\n", test_threadpool);
	pbx_test_validate(test, sccp_threadpool_destroy(test_threadpool) == TRUE);

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(sccp_threadpool_work)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "addwork";
			info->category = test_category;
			info->summary = "chan-sccp-b threadpool work";
			info->description = "chan-sccp-b threadpool work test";
			return AST_TEST_NOT_RUN;
	        case TEST_EXECUTE:
	        	break;
	}
	sccp_threadpool_t *test_threadpool = NULL;
	
	pbx_test_status_update(test, "Create Test threadpool\n");
	test_threadpool = sccp_threadpool_init(THREADPOOL_MIN_SIZE);
	pbx_test_validate(test, NULL != test_threadpool);

	int current_size = sccp_threadpool_thread_count(test_threadpool);
	pbx_test_status_update(test, "Grow threadpool by 3\n");
	sccp_threadpool_grow(test_threadpool, 3);
	pbx_test_validate(test, sccp_threadpool_thread_count(test_threadpool) == current_size + 3); 
	
	if (test_threadpool) {
		pbx_test_status_update(test, "Adding work to Test threadpool\n");
		int work, loopcount=0;
		for (work = 0; work < NUM_WORK; work++) {
			pbx_test_validate(test, sccp_threadpool_add_work(test_threadpool, (void *) sccp_cli_threadpool_test_thread, test) > 0);
		}

		pbx_test_status_update(test, "Waiting for work to finishg in Test threadpool\n");
		while (sccp_threadpool_jobqueue_count(test_threadpool) > 0 && loopcount++ < 20) {
			pbx_test_status_update(test, "Job Queue: %d, Threads: %d\n", sccp_threadpool_jobqueue_count(test_threadpool), sccp_threadpool_thread_count(test_threadpool));
			sleep(1);
		}
		pbx_test_validate(test, sccp_threadpool_jobqueue_count(test_threadpool) == 0); 

		pbx_test_status_update(test, "Destroy Test threadpool\n");
		sccp_threadpool_destroy(test_threadpool);
	}
	return AST_TEST_PASS;
}

static void __attribute__((constructor)) sccp_register_tests(void)
{
        AST_TEST_REGISTER(sccp_threadpool_create_destroy);
        AST_TEST_REGISTER(sccp_threadpool_work);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
        AST_TEST_UNREGISTER(sccp_threadpool_create_destroy);
        AST_TEST_UNREGISTER(sccp_threadpool_work);
}
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
