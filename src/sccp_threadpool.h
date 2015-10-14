/*!
 * \file        sccp_threadpool.h
 * \brief       SCCP Threadpool Header
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \note        Based on the work of Johan Hanssen Seferidis
 *              Library providing a threading pool where you can add work. 
 * \since       2009-01-16
 *
 * $Date: 2010-11-17 12:03:44 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2130 $  
 */
#pragma once
//#include <config.h>
//#include "common.h"

/* Description:         Library providing a threading pool where you can add work on the fly. The number
 *                      of threads in the pool is adjustable when creating the pool. In most cases
 *                      this should equal the number of threads supported by your cpu.
 *          
 *                      In this header file a detailed overview of the functions and the threadpool logical
 *                      scheme is present in case tweaking of the pool is needed. 
 * */

/* 
 * Fast reminders:
 * 
 * tp                   = threadpool 
 * sccp_threadpool      = threadpool
 * sccp_threadpool_t    = threadpool type
 * tp_p                 = threadpool pointer
 * xN                   = x can be any string. N stands for amount
 * 
 * */

/*                       _______________________________________________________        
 *                      /                                                       \
 *                      |   JOB QUEUE        | job1 | job2 | job3 | job4 | ..   |
 *                      |                                                       |
 *                      |   threadpool      | thread1 | thread2 | ..            |
 *                      \_______________________________________________________/
 *      
 * Description:         Jobs are added to the job queue. Once a thread in the pool
 *                      is idle, it is assigned with the first job from the queue(and
 *                      erased from the queue). It's each thread's job to read from 
 *                      the queue serially(using lock) and executing each job
 *                      until the queue is empty.
 * 
 */
/* ================================= STRUCTURES ================================================ */

/* Individual job */
typedef struct sccp_threadpool_job sccp_threadpool_job_t;

struct sccp_threadpool_job {
	void *(*function) (void *arg);										/*!< function pointer         */
	void *arg;												/*!< function's argument      */
	SCCP_LIST_ENTRY (sccp_threadpool_job_t) list;
};

typedef struct sccp_threadpool sccp_threadpool_t;

/* =========================== FUNCTIONS ================================================ */

/* ----------------------- Threadpool specific --------------------------- */

/*!
 * \brief  Initialize threadpool
 * 
 * Allocates memory for the threadpool, jobqueue, semaphore and fixes 
 * pointers in jobqueue.
 * 
 * \param threadsN number of threads to be used
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
 * \param p threadpool to use
 * \return nothing
 */
void sccp_threadpool_thread_do(void *p);

/*!
 * \brief Add work to the job queue
 * 
 * Takes an action and its argument and adds it to the threadpool's job queue.
 * If you want to add to work a function with more than one arguments then
 * a way to implement this is by passing a pointer to a structure.
 * 
 * ATTENTION: You have to cast both the function and argument to not get warnings.
 * 
 * \param tp_p threadpool to which the work will be added to
 * \param function_p callback function to add as work
 * \param arg_p argument to the above function
 * \return int
 */
int sccp_threadpool_add_work(sccp_threadpool_t * tp_p, void *(*function_p) (void *), void *arg_p);

/*!
 * \brief Destroy the threadpool
 * 
 * This will 'kill' the threadpool and free up memory. If threads are active when this
 * is called, they will finish what they are doing and then they will get destroyied.
 * 
 * \param tp_p threadpool a pointer to the threadpool structure you want to destroy
 */
void sccp_threadpool_destroy(sccp_threadpool_t * tp_p);

/*!
 * \brief Return number of currently allocate threads in the threadpool
 * \param tp_p threadpool a pointer to the threadpool structure for which we would like to know the number of workers
 */
int sccp_threadpool_thread_count(sccp_threadpool_t * tp_p);

/* ------------------------- Queue specific ------------------------------ */

/*!
 * \brief Initialize queue
 * \param tp_p pointer to threadpool
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
 * \param tp_p pointer to threadpool
 * \param newjob_p pointer to the new job(MUST BE ALLOCATED)
 * \return nothing 
 */
void sccp_threadpool_jobqueue_add(sccp_threadpool_t * tp_p, sccp_threadpool_job_t * newjob_p);

/*!
 * \brief Return Number of Jobs in the Queue
 * \param tp_p pointer to threadpool
 */
int sccp_threadpool_jobqueue_count(sccp_threadpool_t * tp_p);
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
