
/*!
 * \file 	sccp_threadpool.h
 * \brief 	SCCP Threadpool Header
 * \author 	Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \note	Based on the work of Johan Hanssen Seferidis
 * 		Library providing a threading pool where you can add work. 
 * \since 	2009-01-16
 *
 * $Date: 2010-11-17 12:03:44 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2130 $  
 */
#ifndef SCCP_THREADPOOL_H_
#  define SCCP_THREADPOOL_H_

#  include <pthread.h>

#  ifndef DARWIN								/* Normal implementation for named and unnamed semaphores */
#    include <semaphore.h>
#  else										/* Mac OSX does not implement sem_init and only supports named semaphores by default */
#    include <mach/mach_init.h>
#    include <mach/task.h>
#    include <mach/semaphore.h>
#    define sem_t 		semaphore_t					/* Remapping semaphore command to sync up with gnu implementation */
#    define sem_init(s,p,c) 	semaphore_create(mach_task_self(),s,SYNC_POLICY_FIFO,c)
#    define sem_wait(s) 	semaphore_wait(*s)
#    define sem_post(s) 	semaphore_signal(*s)
#    define sem_destroy(s) 	semaphore_destroy(mach_task_self(), *s)
#  endif

/* Description: 	Library providing a threading pool where you can add work on the fly. The number
 *              	of threads in the pool is adjustable when creating the pool. In most cases
 *              	this should equal the number of threads supported by your cpu.
 *          
 *              	In this header file a detailed overview of the functions and the threadpool logical
 *              	scheme is present in case tweaking of the pool is needed. 
 * */

/* 
 * Fast reminders:
 * 
 * tp  		        = threadpool 
 * sccp_threadpool      = threadpool
 * sccp_threadpool_t    = threadpool type
 * tp_p         	= threadpool pointer
 * sem          	= semaphore
 * xN           	= x can be any string. N stands for amount
 * 
 * */

/*                       _______________________________________________________        
 *                      /                                                       \
 *                      |   JOB QUEUE        | job1 | job2 | job3 | job4 | ..   |
 *                      |                                                       |
 *                      |   threadpool      | thread1 | thread2 | ..            |
 *                      \_______________________________________________________/
 *      
 * Description:       	Jobs are added to the job queue. Once a thread in the pool
 *                    	is idle, it is assigned with the first job from the queue(and
 *                    	erased from the queue). It's each thread's job to read from 
 *                    	the queue serially(using lock) and executing each job
 *                    	until the queue is empty.
 * 
 * 
 *    Scheme:
 * 
 *    sccp_threadpool_                jobqueue____                      ______ 
 *    |               |               |           |       .----------->|_job0_| Newly added job
 *    |               |               |  head------------'             |_job1_|
 *    | jobqueue--------------------->|           |                    |_job2_|
 *    |               |               |  tail------------.             |__..__| 
 *    |_______________|               |___________|       '----------->|_jobn_| Job for thread to take
 * 
 * 
 *    job0________ 
 *    |           |
 *    | function---->
 *    |           |
 *    |   arg------->
 *    |           |                   job1________ 
 *    |  next------------------------>|           |
 *    |___________|                   |           |..
 */

/* ================================= STRUCTURES ================================================ */

/* Individual job */
typedef struct sccp_threadpool_job_t {
	void *(*function) (void *arg);						/*!< function pointer         */
	void *arg;								/*!< function's argument      */
	struct sccp_threadpool_job_t *next;					/*!< pointer to next job      */
	struct sccp_threadpool_job_t *prev;					/*!< pointer to previous job  */
} sccp_threadpool_job_t;

/* Job queue as doubly linked list */
typedef struct sccp_threadpool_jobqueue {
	sccp_threadpool_job_t *head;						/*!< pointer to head of queue */
	sccp_threadpool_job_t *tail;						/*!< pointer to tail of queue */
	int jobsN;								/*!< amount of jobs in queue  */
	sem_t *queueSem;							/*!< semaphore(this is probably just holding the same as jobsN) */
} sccp_threadpool_jobqueue;

typedef struct sccp_threadpool_t sccp_threadpool_t;
typedef struct thread_data thread_data;

/* =========================== FUNCTIONS ================================================ */

/* ----------------------- Threadpool specific --------------------------- */

/*!
 * \brief  Initialize threadpool
 * 
 * Allocates memory for the threadpool, jobqueue, semaphore and fixes 
 * pointers in jobqueue.
 * 
 * \param  number of threads to be used
 * \return threadpool struct on success,
 *         NULL on error
 */
sccp_threadpool_t *sccp_threadpool_init(int threadsN);

/*!
 * \brief What each thread is doing
 * 
 * In principle this is an endless loop. The only time this loop gets interuppted is once
 * sccp_threadpool_destroy() is invoked.
 * 
 * \param threadpool to use
 * \return nothing
 */
void sccp_threadpool_thread_do(sccp_threadpool_t * tp_p);

/*!
 * \brief Add work to the job queue
 * 
 * Takes an action and its argument and adds it to the threadpool's job queue.
 * If you want to add to work a function with more than one arguments then
 * a way to implement this is by passing a pointer to a structure.
 * 
 * ATTENTION: You have to cast both the function and argument to not get warnings.
 * 
 * \param  threadpool to where the work will be added to
 * \param  function to add as work
 * \param  argument to the above function
 * \return int
 */
int sccp_threadpool_add_work(sccp_threadpool_t * tp_p, void *(*function_p) (void *), void *arg_p);

/*!
 * \brief Destroy the threadpool
 * 
 * This will 'kill' the threadpool and free up memory. If threads are active when this
 * is called, they will finish what they are doing and then they will get destroyied.
 * 
 * \param threadpool a pointer to the threadpool structure you want to destroy
 */
void sccp_threadpool_destroy(sccp_threadpool_t * tp_p);

/*!
 * \brief Return number of currently allocate threads in the threadpool
 */
int sccp_threadpool_thread_count(sccp_threadpool_t * tp_p);

/* ------------------------- Queue specific ------------------------------ */

/*!
 * \brief Initialize queue
 * \param  pointer to threadpool
 * \return 0 on success,
 *        -1 on memory allocation error
 */
int sccp_threadpool_jobqueue_init(sccp_threadpool_t * tp_p);

/*!
 * \brief Add job to queue
 * 
 * A new job will be added to the queue. The new job MUST be allocated
 * before passed to this function or else other functions like sccp_threadpool_jobqueue_empty()
 * will be broken.
 * 
 * \param pointer to threadpool
 * \param pointer to the new job(MUST BE ALLOCATED)
 * \return nothing 
 */
void sccp_threadpool_jobqueue_add(sccp_threadpool_t * tp_p, sccp_threadpool_job_t * newjob_p);

/*!
 * \brief Remove last job from queue. 
 * 
 * This does not free allocated memory so be sure to have peeked()
 * before invoking this as else there will result lost memory pointers.
 * 
 * \param  pointer to threadpool
 * \return 0 on success,
 *         -1 if queue is empty
 */
int sccp_threadpool_jobqueue_removelast(sccp_threadpool_t * tp_p);

/*! 
 * \brief Get last job in queue (tail)
 * 
 * Gets the last job that is inside the queue. This will work even if the queue
 * is empty.
 * 
 * \param  pointer to threadpool structure
 * \return job a pointer to the last job in queue,
 *         a pointer to NULL if the queue is empty
 */
sccp_threadpool_job_t *sccp_threadpool_jobqueue_peek(sccp_threadpool_t * tp_p);

/*!
 * \brief Remove and deallocate all jobs in queue
 * 
 * This function will deallocate all jobs in the queue and set the
 * jobqueue to its initialization values, thus tail and head pointing
 * to NULL and amount of jobs equal to 0.
 * 
 * \param pointer to threadpool structure
 * */
void sccp_threadpool_jobqueue_empty(sccp_threadpool_t * tp_p);

/*!
 * \brief Return Number of Jobs in the Queue
 */
int sccp_threadpool_jobqueue_count(sccp_threadpool_t * tp_p);

#endif
