
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

#include "config.h"
#include "common.h"


SCCP_FILE_VERSION(__FILE__, "$Revision: 2215 $")
#ifdef DARWIN
#  include <fcntl.h>
#endif
#include <semaphore.h>
#include "sccp_threadpool.h"
#include <signal.h>
#undef pthread_create

/* The threadpool */
struct sccp_threadpool_t {
	pthread_t *threads;							/*!< pointer to threads' ID   */
	int threadsN;								/*!< amount of threads        */
	sccp_threadpool_jobqueue *jobqueue;					/*!< pointer to the job queue */
	time_t last_size_check;							/*!< Time since last resize check */
	int job_high_water_mark;						/*!< Highest number of jobs outstanding since last resize check */
};

/* Container for all things that each thread is going to need */
struct thread_data {
	sccp_mutex_t *mutex_p;
	sccp_threadpool_t *tp_p;
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
#ifndef CPU_COUNT
	#warning CPU_COUNT not defined
#endif	
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Starting Threadpool\n");
	sccp_threadpool_t *tp_p;

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
	tp_p->threads = (pthread_t *) malloc(threadsN * sizeof(pthread_t));					/* MALLOC thread IDs */
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

	/* Initialise semaphore */
#ifdef DARWIN													/* MacOSX does not support unnamed semaphores */
	const char *sem_file_name = "sccp_threadpool_thread.semaphore";
	tp_p->jobqueue->queueSem = sem_open(sem_file_name, O_CREAT, 0777, 0);
	if (tp_p->jobqueue->queueSem == NULL) {
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Error creating named semaphore (error: %s [%d]). Exiting\n", strerror(errno), errno);
		sccp_free(tp_p->threads);
		sccp_free(tp_p);
		return NULL;
	}	
#else
	tp_p->jobqueue->queueSem = (sem_t *) malloc(sizeof(sem_t));						/* MALLOC job queue semaphore */
	if (tp_p->jobqueue->queueSem == NULL) {
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Could not allocate memory for unnamed semaphore (error: %s [%d]). Exiting\n", strerror(errno), errno);
		sccp_free(tp_p->threads);
		sccp_free(tp_p);
		return NULL;
	}
	if (sem_init(tp_p->jobqueue->queueSem, 0 ,0)) {								/* Create semaphore with no shared, initial value */
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Error creating unnamed semaphore (error: %s [%d]). Exiting\n", strerror(errno), errno);
		sccp_free(tp_p->threads);
		sccp_free(tp_p);
		return NULL;
	}
#endif

	/* Make threads in pool */
	int t;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	for (t = 0; t < threadsN; t++) {
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
	void *tmpPtr = realloc(tp_p, threadsN * sizeof(pthread_t));		/* REALLOC thread IDs */
	if (tmpPtr == NULL) {
		pbx_log(LOG_ERROR, "sccp_threadpool_init(): Could not allocate memory for thread IDs\n");
		return;
	}
	tp_p->threads = (pthread_t *) tmpPtr;
	tp_p->threadsN = threadsN;
}

/* check threadpool size (increase/decrease if necessary)*/
static void sccp_threadpool_check_size(sccp_threadpool_t * tp_p)
{
	int t;
	int newsize;
	if (tp_p && tp_p->jobqueue) {
		if (tp_p->jobqueue->jobsN > (tp_p->threadsN * 2) && tp_p->threadsN < THREADPOOL_MAX_SIZE) {		/* increase */
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Add new thread to threadpool %p\n", tp_p);

			newsize = tp_p->threadsN + 1;
			sccp_thread_resize(tp_p, newsize);
			pthread_create(&(tp_p->threads[tp_p->threadsN - 1]), NULL, (void *)sccp_threadpool_thread_do, (void *)tp_p);

		} else if (tp_p->threadsN > THREADPOOL_MIN_SIZE && tp_p->jobqueue->jobsN > (tp_p->threadsN / 2)) {	/* increase */
			newsize = tp_p->threadsN - 1;
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Remove thread %d from threadpool %p\n", newsize, tp_p);
			/* Send thread cancel message */
			pthread_cancel(tp_p->threads[newsize - 1]);
			/* Wake up all threads */
			for (t = 0; t < (tp_p->threadsN); t++) {
				if (sem_post(tp_p->jobqueue->queueSem)) {
					pbx_log(LOG_ERROR, "sccp_threadpool_destroy(): Could not bypass sem_wait()\n");
				}
			}
			/* Wait for threads to finish */
			pthread_join(tp_p->threads[newsize - 1], NULL);
			sccp_thread_resize(tp_p, newsize);
		}
		tp_p->last_size_check = time(0);
		tp_p->job_high_water_mark = tp_p->jobqueue->jobsN;
	}
}

/* What each individual thread is doing 
 * */

/* There are two scenarios here. One is everything works as it should and second if
 * the sccp_threadpool is to be killed. In that manner we try to BYPASS sem_wait and end each thread. */
void sccp_threadpool_thread_do(sccp_threadpool_t * tp_p)
{
	while (tp_p && tp_p->jobqueue && tp_p->jobqueue->queueSem && sccp_threadpool_keepalive) {
		pthread_testcancel();
		if (tp_p && tp_p->jobqueue && tp_p->jobqueue->queueSem && sem_wait(tp_p->jobqueue->queueSem)) {							/* WAITING until there is work in the queue */
			pbx_log(LOG_ERROR, "sccp_threadpool_thread_do(): Error while waiting for semaphore (error: %s [%d]). Exiting\n", strerror(errno), errno);
			return;
		}
		if (sccp_threadpool_keepalive) {
			/* Read job from queue and execute it */
			void *(*func_buff) (void *arg);
			void *arg_buff;
			sccp_threadpool_job_t *job_p;

			pbx_mutex_lock(&threadpool_mutex);							/* LOCK */

			if ((job_p = sccp_threadpool_jobqueue_peek(tp_p))) {
				func_buff = job_p->function;
				arg_buff = job_p->arg;
				sccp_threadpool_jobqueue_removelast(tp_p);
			}

			pbx_mutex_unlock(&threadpool_mutex);							/* UNLOCK */

			if (job_p) {
				func_buff(arg_buff);								/* run function */
				free(job_p);
			}											/* DEALLOC job */
		} else {
			return;											/* EXIT thread */
		}

		if ((time(0) - tp_p->last_size_check) > THREADPOOL_RESIZE_INTERVAL) {
			pbx_mutex_lock(&threadpool_mutex);							/* LOCK */
			sccp_threadpool_check_size(tp_p);
			pbx_mutex_unlock(&threadpool_mutex);							/* UNLOCK */
		}
	}
	return;
}

/* Add work to the thread pool */
int sccp_threadpool_add_work(sccp_threadpool_t * tp_p, void *(*function_p) (void *), void *arg_p)
{
	// prevent new work while shutting down
//	if (GLOB(module_running)) {
	if (!sccp_threadpool_shuttingdown) {
		sccp_threadpool_job_t *newJob;

		newJob = (sccp_threadpool_job_t *) malloc(sizeof(sccp_threadpool_job_t));				/* MALLOC job */
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
		return 0;
	}
}

/* Destroy the threadpool */
void sccp_threadpool_destroy(sccp_threadpool_t * tp_p)
{
	int t;
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Destroying Threadpool %p with %d jobs\n", tp_p, tp_p->jobqueue->jobsN);


	sccp_threadpool_shuttingdown = 1;
	// wake up jobs untill jobqueue is empty, before shutting down, to make sure all jobs have been processed
	while (tp_p->jobqueue->jobsN > 0) {
		for (t = 0; t < (tp_p->threadsN); t++) {
			if (sem_post(tp_p->jobqueue->queueSem)) {
				pbx_log(LOG_ERROR, "sccp_threadpool_destroy(): Could not bypass sem_wait()\n");
			}
		}
	}

	/* End each thread's infinite loop */
	sccp_threadpool_keepalive = 0;

	/* Awake idle threads waiting at semaphore */
	for (t = 0; t < (tp_p->threadsN); t++) {
		if (sem_post(tp_p->jobqueue->queueSem)) {
			pbx_log(LOG_ERROR, "sccp_threadpool_destroy(): Could not bypass sem_wait()\n");
		}
	}

	/* Kill semaphore */
#ifdef DARWIN													/* MacOSX does not support unnamed semaphores */
	if (sem_close(tp_p->jobqueue->queueSem) != 0) {
		pbx_log(LOG_ERROR, "sccp_threadpool_destroy(): Could not destroy semaphore (error: %s [%d])\n", strerror(errno), errno);
	}
#else
	if (sem_destroy(tp_p->jobqueue->queueSem) != 0) {
		pbx_log(LOG_ERROR, "sccp_threadpool_destroy(): Could not destroy semaphore (error: %s [%d])\n", strerror(errno), errno);
	}
	free(tp_p->jobqueue->queueSem);										/* DEALLOC job queue semaphore */
#endif	

	/* Wait for threads to finish */
	for (t = 0; t < (tp_p->threadsN); t++) {
		pthread_cancel(tp_p->threads[t]);
		pthread_kill(tp_p->threads[t], SIGURG);
		pthread_join(tp_p->threads[t], NULL);
	}

	sccp_threadpool_jobqueue_empty(tp_p);

	/* Dealloc */
	free(tp_p->threads);											/* DEALLOC threads             */
	free(tp_p->jobqueue);											/* DEALLOC job queue           */
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
	tp_p->jobqueue = (sccp_threadpool_jobqueue *) malloc(sizeof(sccp_threadpool_jobqueue));			/* MALLOC job queue */
	if (tp_p->jobqueue == NULL)
		return -1;
	tp_p->jobqueue->tail = NULL;
	tp_p->jobqueue->head = NULL;
	tp_p->jobqueue->jobsN = 0;
	tp_p->last_size_check = time(0);
	tp_p->job_high_water_mark = 0;
	return 0;
}

/* Add job to queue */
void sccp_threadpool_jobqueue_add(sccp_threadpool_t * tp_p, sccp_threadpool_job_t * newjob_p)
{														/* remember that job prev and next point to NULL */

	newjob_p->next = NULL;
	newjob_p->prev = NULL;

	sccp_threadpool_job_t *oldFirstJob;

	oldFirstJob = tp_p->jobqueue->head;

	/* fix jobs' pointers */
	switch (tp_p->jobqueue->jobsN) {

	case 0:													/* if there are no jobs in queue */
		tp_p->jobqueue->tail = newjob_p;
		tp_p->jobqueue->head = newjob_p;
		break;

	default:												/* if there are already jobs in queue */
		oldFirstJob->prev = newjob_p;
		newjob_p->next = oldFirstJob;
		tp_p->jobqueue->head = newjob_p;

	}

	(tp_p->jobqueue->jobsN)++;										/* increment amount of jobs in queue */
	if (tp_p->jobqueue->jobsN > tp_p->job_high_water_mark)
		tp_p->job_high_water_mark = tp_p->jobqueue->jobsN;
		
	sem_post(tp_p->jobqueue->queueSem);

	int sval;

	sem_getvalue(tp_p->jobqueue->queueSem, &sval);
}

/* Remove job from queue */
int sccp_threadpool_jobqueue_removelast(sccp_threadpool_t * tp_p)
{
	sccp_threadpool_job_t *oldLastJob;

	oldLastJob = tp_p->jobqueue->tail;

	/* fix jobs' pointers */
	switch (tp_p->jobqueue->jobsN) {

	case 0:													/* if there are no jobs in queue */
		return -1;
		break;

	case 1:													/* if there is only one job in queue */
		tp_p->jobqueue->tail = NULL;
		tp_p->jobqueue->head = NULL;
		break;

	default:												/* if there are more than one jobs in queue */
		oldLastJob->prev->next = NULL;									/* the almost last item */
		tp_p->jobqueue->tail = oldLastJob->prev;

	}

	(tp_p->jobqueue->jobsN)--;

	int sval;

	sem_getvalue(tp_p->jobqueue->queueSem, &sval);
	return 0;
}

/* Get first element from queue */
sccp_threadpool_job_t *sccp_threadpool_jobqueue_peek(sccp_threadpool_t * tp_p)
{
	if (tp_p && tp_p->jobqueue && tp_p->jobqueue->tail)
		return tp_p->jobqueue->tail;
	else 
		return NULL;
}

/* Remove and deallocate all jobs in queue */
void sccp_threadpool_jobqueue_empty(sccp_threadpool_t * tp_p)
{
	sccp_threadpool_job_t *curjob;

	curjob = tp_p->jobqueue->tail;

	while (tp_p->jobqueue->jobsN) {
		tp_p->jobqueue->tail = curjob->prev;
		free(curjob);
		curjob = tp_p->jobqueue->tail;
		tp_p->jobqueue->jobsN--;
	}

	/* Fix head and tail */
	tp_p->jobqueue->tail = NULL;
	tp_p->jobqueue->head = NULL;
}

int sccp_threadpool_jobqueue_count(sccp_threadpool_t * tp_p) 
{
	return tp_p->jobqueue->jobsN;
}
