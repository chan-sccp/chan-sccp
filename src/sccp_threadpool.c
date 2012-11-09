
/*!
 * \file 	sccp_threadpool.c
 * \brief 	SCCP Threadpool Class
 * \author 	Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \note	Based on the work of Johan Hanssen Seferidis
 * 		Library providing a threading pool where you can add work. 
 * \since 	2009-01-16
 * \remarks	Purpose: 	SCCP Hint
 * 		When to use:	Does the business of hint status
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
#    include <sys/sysinfo.h>											// to retrieve processor info
#endif
#define SEMAPHORE_LOCKED	(0)
#define SEMAPHORE_UNLOCKED	(1)

/* The threadpool */
struct sccp_threadpool_t {
	pthread_t *threads;							/*!< pointer to threads' ID   */
	int threadsN;								/*!< amount of threads        */
	SCCP_LIST_HEAD(, sccp_threadpool_job_t) jobs;
	ast_cond_t work;
	time_t last_size_check;							/*!< Time since last resize check */
	int job_high_water_mark;						/*!< Highest number of jobs outstanding since last resize check */
};

/* 
 * Fast reminders:
 * 
 * tp 	         	= threadpool 
 * sccp_threadpool      = threadpool
 * sccp_threadpool_t    = threadpool type
 * tp_p         	= threadpool pointer
 * sem  	        = semaphore
 * xN           	= x can be any string. N stands for amount
 * */
static int sccp_threadpool_keepalive = 1;
static int sccp_threadpool_shuttingdown = 0;

/* Create mutex variable */
AST_MUTEX_DEFINE_STATIC(threadpool_mutex);									/* used to serialize queue access */

/* Initialise thread pool */
sccp_threadpool_t *sccp_threadpool_init(int threadsN)
{
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Starting Threadpool\n");
	sccp_threadpool_t *tp_p;

//#ifndef CS_CPU_COUNT
//      #warning CPU_COUNT not defined
//#endif        

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
	tp_p->threads = (pthread_t *) malloc(THREADPOOL_MAX_SIZE * sizeof(pthread_t));					/* MALLOC thread IDs */
	if (tp_p->threads == NULL) {
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Could not allocate memory for thread IDs\n");
		sccp_free(tp_p);
		return NULL;
	}
	tp_p->threadsN = threadsN;

	/* Initialise the job queue */
	if (sccp_threadpool_jobqueue_init(tp_p) == -1) {
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Could not allocate memory for job queue\n");
		sccp_free(tp_p->threads);
		sccp_free(tp_p);
		return NULL;
	}

	/* Initialise Condition */
	ast_cond_init(&(tp_p->work), NULL);

	/* Make threads in pool */
	int t;

	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	for (t = 0; t < tp_p->threadsN; t++) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Create thread %d in pool \n", t);

		pbx_pthread_create(&(tp_p->threads[t]), &attr, (void *)sccp_threadpool_thread_do, (void *)tp_p);	/* MALLOCS INSIDE PTHREAD HERE */
	}

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Threadpool Started\n");
	return tp_p;
}

/* resize threadpool */
static void sccp_thread_resize(sccp_threadpool_t * tp_p, int threadsN)
{
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Resizing threadpool to %d threads\n", threadsN);
	// Doesn't seem to work correctly, fixed pthread_array to THREADPOOL_MAX_SIZE for now
/*	pthread_t *tmpPtr = (pthread_t *)realloc(tp_p, threadsN * sizeof(pthread_t));		// REALLOC thread IDs

	if (tmpPtr == NULL) {
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Could not allocate memory for thread IDs\n");
		return;
	}
	tp_p->threads = (pthread_t *) tmpPtr;
*/	
	tp_p->threadsN = threadsN;
}

/* check threadpool size (increase/decrease if necessary)*/
static void sccp_threadpool_check_size(sccp_threadpool_t * tp_p)
{
	if (tp_p && !sccp_threadpool_shuttingdown) {
		sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_check_resize) in thread: %d\n", (unsigned int)pthread_self());
		if (SCCP_LIST_GETSIZE(tp_p->jobs) > (tp_p->threadsN * 2) && tp_p->threadsN < THREADPOOL_MAX_SIZE) {	// increase
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Add new thread to threadpool %p\n", tp_p);
			pthread_attr_t attr;

			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
			sccp_thread_resize(tp_p, tp_p->threadsN + 1);
			pthread_create(&(tp_p->threads[tp_p->threadsN - 1]), &attr, (void *)sccp_threadpool_thread_do, (void *)tp_p);
		} else if (((time(0) - tp_p->last_size_check) > THREADPOOL_RESIZE_INTERVAL*3) && 					// wait a little longer to decrease
			  (tp_p->threadsN > THREADPOOL_MIN_SIZE && SCCP_LIST_GETSIZE(tp_p->jobs) < (tp_p->threadsN / 2))) {	// decrease
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Remove thread %d from threadpool %p\n", tp_p->threadsN - 1, tp_p);
			// Send thread cancel message 
			pthread_cancel(tp_p->threads[tp_p->threadsN - 2]);
			// Wake up all threads
			ast_cond_broadcast(&(tp_p->work));
			// Wait for threads to finish 
			pthread_join(tp_p->threads[tp_p->threadsN - 2], NULL);
			sccp_thread_resize(tp_p, tp_p->threadsN - 1);
		}
		tp_p->last_size_check = time(0);
		tp_p->job_high_water_mark = SCCP_LIST_GETSIZE(tp_p->jobs);
		sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_check_resize) threads: %d, job_high_water_mark: %d\n", tp_p->threadsN, tp_p->job_high_water_mark);
	}
}

/* What each individual thread is doing 
 * */

/* There are two scenarios here. One is everything works as it should and second if
 * the sccp_threadpool is to be killed. In that manner we try to BYPASS sem_wait and end each thread. */
void sccp_threadpool_thread_do(sccp_threadpool_t * tp_p)
{
	uint threadid=(unsigned int)pthread_self();
	int jobs=0;
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Starting Threadpool JobQueue\n");
	while (tp_p && sccp_threadpool_keepalive) {
		pthread_testcancel();
		jobs = SCCP_LIST_GETSIZE(tp_p->jobs);
		sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) tp_p: %p, jobs: %d, threadid: %d\n", tp_p, jobs, threadid);
		SCCP_LIST_LOCK(&(tp_p->jobs));									/* LOCK */
		while (SCCP_LIST_GETSIZE(tp_p->jobs) == 0 && sccp_threadpool_keepalive) {
			sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) Waiting for Semaphore. tp_p: %p, jobs: %d, threadid: %d\n", tp_p, jobs, threadid);
			ast_cond_wait(&(tp_p->work),&(tp_p->jobs.lock));
			sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_thread_do) Let's work. tp_p: %p, jobs: %d, threadid: %d\n", tp_p, jobs, threadid);
		}
		if (!sccp_threadpool_keepalive && SCCP_LIST_GETSIZE(tp_p->jobs) == 0) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "JobQueue keepalive == 0. Exting...\n");
			SCCP_LIST_UNLOCK(&tp_p->jobs);								/* UNLOCK */
			return;											/* EXIT thread */
		}
		{
			/* Read job from queue and execute it */
			void *(*func_buff) (void *arg) = NULL;
			void *arg_buff = NULL;
			sccp_threadpool_job_t *job;

//			SCCP_LIST_LOCK(&(tp_p->jobs));
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
				pbx_mutex_lock(&threadpool_mutex);							/* LOCK */
				sccp_threadpool_check_size(tp_p);							/* Check Resizing */
				pbx_mutex_unlock(&threadpool_mutex);							/* UNLOCK */
			}
		}

	}
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "JobQueue Exiting Thread...\n");
	return;
}

/* Add work to the thread pool */
int sccp_threadpool_add_work(sccp_threadpool_t * tp_p, void *(*function_p) (void *), void *arg_p)
{
	// prevent new work while shutting down
//      if (GLOB(module_running)) {
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
	int t;

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Destroying Threadpool %p with %d jobs\n", tp_p, SCCP_LIST_GETSIZE(tp_p->jobs));

	// After this point, no new jobs can be added
	SCCP_LIST_LOCK(&(tp_p->jobs));
	sccp_threadpool_shuttingdown = 1;
	// shutdown is a kind of work too
	sccp_threadpool_keepalive = 0;
	SCCP_LIST_UNLOCK(&(tp_p->jobs));

	// wake up jobs untill jobqueue is empty, before shutting down, to make sure all jobs have been processed
	ast_cond_broadcast(&(tp_p->work));

	/* Make sure / Wait for threads to finish */
	for (t = 0; t < (tp_p->threadsN); t++) {
		pthread_cancel(tp_p->threads[t]);
		pthread_kill(tp_p->threads[t], SIGURG);
		pthread_join(tp_p->threads[t], NULL);
	}

	/* Dealloc */
	ast_cond_destroy(&(tp_p->work));									/* Remove Condition */
	SCCP_LIST_HEAD_DESTROY(&(tp_p->jobs));
	free(tp_p->threads);											/* DEALLOC threads             */
	free(tp_p);												/* DEALLOC thread pool         */
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Threadpool Ended\n");
}

int sccp_threadpool_thread_count(sccp_threadpool_t * tp_p)
{
	return tp_p->threadsN;
}

/* =================== JOB QUEUE OPERATIONS ===================== */

/* Initialise queue */
int sccp_threadpool_jobqueue_init(sccp_threadpool_t * tp_p)
{
	sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_jobqueue_init) tp_p: %p\n", tp_p);
	SCCP_LIST_HEAD_INIT(&tp_p->jobs);
	tp_p->last_size_check = time(0);
	tp_p->job_high_water_mark = 0;
	return 0;
}

/* Add job to queue */
void sccp_threadpool_jobqueue_add(sccp_threadpool_t * tp_p, sccp_threadpool_job_t * newjob_p)
{
	if (!tp_p) {
		pbx_log(LOG_ERROR, "(sccp_threadpool_jobqueue_add) no tp_p\n");
		return;
	}

	sccp_log(DEBUGCAT_THPOOL) (VERBOSE_PREFIX_3 "(sccp_threadpool_jobqueue_add) tp_p: %p, jobCount: %d\n", tp_p, SCCP_LIST_GETSIZE(tp_p->jobs));
	SCCP_LIST_LOCK(&(tp_p->jobs));
	if (sccp_threadpool_shuttingdown) {
		pbx_log(LOG_ERROR, "(sccp_threadpool_jobqueue_add) shutting down\n");
		SCCP_LIST_UNLOCK(&(tp_p->jobs));
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
