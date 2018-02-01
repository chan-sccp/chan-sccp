/*!
 * \file        sccp_refcount.c
 * \brief       SCCP Refcount Class
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */

/*!
 * \section sccp_refcount   Reference Counted Objects
 * 
 * We started using refcounting in V4.0 to prevent possible deadlock situations where the would not even have to occur. Up till now we had been using lock to prevent objects 
 * from vanishing instead of preventing modification. As a rule there will be at most be one thread modifying a device, line, channel object in such a significant manner 
 * the session thread the device and associated line/channel belongs too). 
 *
 * In this regard locking was not necessary and could be replaced by a method to prevent vanishing objects and preventing dereferencing null pointers, leading to segmentation faults.
 * To solve that we opted to implement reference counting. Reference counting has a number of rules that need to be followed at any and all times for it to work:
 *
 * - Rule 1: During the initial creation / allocation of a refcounted object the refcount is set to 1. Example:
 *   \code
 *   channel = (sccp_channel_t *) sccp_refcount_object_alloc(sizeof(sccp_channel_t), "channel", c->id, __sccp_channel_destroy);
 *   \endcode
 *
 * - Rule 2: Functions that <b><em>return an object</em></b> (e.g. sccp_device, sccp_line, sccp_channel, sccp_event, sccp_linedevice), need to do so <b><em>with</em></b> a retained objects. 
 *   This happens when a object is created and returned to a calling function for example.
 *
 * - Rule 3: Functions that <b><em>receive an object pointer reference</em></b> via a function call expect the object <b><em>is being retained</em></b> in the calling function, during the time the called function lasts. 
 *   The object can <b><em>only</em></b> be released by the calling function not the called function,
 *
 * - Rule 4: When releasing an object the pointer we pass a referenece to the object pointer, should is nullified immediatly before return:
 *   \code
 *   sccp_device_release(&d);
 *   \endcode
 *   
 * - Rule 5:    You <b><em>cannnot</em></b> use free on a refcounted object. Destruction of the refcounted object and subsequent freeing of the occupied memory is performed by the sccp_release 
 *              when the number of reference reaches 0. To finalize the use of a refcounted object just release the object one final time, to negate the initial refcount of 1 during creation.
 * .
 * These rules need to followed to the letter !
 */

#include "config.h"
#include "common.h"
#include "sccp_atomic.h"
#include "sccp_utils.h"
#include <asterisk/cli.h>

// required for refcount inuse checking
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"

SCCP_FILE_VERSION(__FILE__, "");

//nb: SCCP_HASH_PRIME defined in config.h, default 563
#define SCCP_SIMPLE_HASH(_a) (((unsigned long)(_a)) % SCCP_HASH_PRIME)
#define SCCP_LIVE_MARKER 13
#if CS_REFCOUNT_DEBUG
#define REFCOUNT_MAX_PARENTS 3
#define REF_DEBUG_FILE_MAX_SIZE 10000000
#define REF_DEBUG_FILE "/tmp/sccp_refs"
static int __rotate_debug_file(void);
#endif
static enum sccp_refcount_runstate runState = SCCP_REF_STOPPED;

static struct sccp_refcount_obj_info {
	int (*destructor) (const void *ptr);
	char datatype[StationMaxDeviceNameSize];
	sccp_debug_category_t debugcat;
} obj_info[] = {
/* *INDENT-OFF* */
	[SCCP_REF_PARTICIPANT] = {NULL, "participant", DEBUGCAT_CONFERENCE},
	[SCCP_REF_CONFERENCE] = {NULL, "conference", DEBUGCAT_CONFERENCE},
	[SCCP_REF_EVENT] = {NULL, "event", DEBUGCAT_EVENT},
	[SCCP_REF_CHANNEL] = {NULL, "channel", DEBUGCAT_CHANNEL},
	[SCCP_REF_LINEDEVICE] = {NULL, "linedevice", DEBUGCAT_LINE},
	[SCCP_REF_LINE] = {NULL, "line", DEBUGCAT_LINE},
	[SCCP_REF_DEVICE] = {NULL, "device", DEBUGCAT_DEVICE},
#if CS_TEST_FRAMEWORK
	[SCCP_REF_TEST] = {NULL, "test", DEBUGCAT_HIGH},
#endif
/* *INDENT-ON* */
};

typedef struct refcount_object RefCountedObject;

#ifdef SCCP_ATOMIC
#define obj_lock NULL
#else
#define	obj_lock &obj->lock
#endif

struct refcount_object {
#ifndef SCCP_ATOMIC
	ast_mutex_t lock;
#endif
	volatile CAS32_TYPE refcount;
	enum sccp_refcounted_types type;
	char identifier[REFCOUNT_INDENTIFIER_SIZE];
#if CS_REFCOUNT_DEBUG
	void *parentWeakPtr[REFCOUNT_MAX_PARENTS];
#endif	
	int len;
	int alive;
	SCCP_RWLIST_ENTRY (RefCountedObject) list;
	unsigned char data[0] __attribute__((aligned(8)));
};


static ast_rwlock_t objectslock;										// general lock to modify hash table entries
static struct refcount_objentry{
	SCCP_RWLIST_HEAD (, RefCountedObject) refCountedObjects  __attribute__((aligned(8)));			//!< one rwlock per hash table entry, used to modify list
} *objects[SCCP_HASH_PRIME];											//!< objects hash table

#if CS_REFCOUNT_DEBUG
static FILE *sccp_ref_debug_log;
static volatile uint32_t ref_debug_size;
#endif

void sccp_refcount_init(void)
{
	sccp_log((DEBUGCAT_REFCOUNT + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "SCCP: (Refcount) init\n");
	pbx_rwlock_init_notracking(&objectslock);								// No tracking to safe cpu cycles
#if CS_REFCOUNT_DEBUG
	sccp_ref_debug_log = NULL;
	ref_debug_size = 0;
	__rotate_debug_file();
#endif
	runState = SCCP_REF_RUNNING;
}

void sccp_refcount_destroy(void)
{
	uint32_t hash, type;
	RefCountedObject *obj;

	pbx_log(LOG_NOTICE, "SCCP: (Refcount) Shutting Down. Checking Clean Shutdown...\n");
	int numObjects = 0;
	runState = SCCP_REF_STOPPED;

	sched_yield();												//make sure all other threads can finish their work first.

	// cleanup if necessary, if everything is well, this should not be necessary
	ast_rwlock_wrlock(&objectslock);
	for (type = 0; type < ARRAY_LEN(obj_info); type++) { 							// unwind in order of type priority
		for (hash = 0; hash < SCCP_HASH_PRIME && objects[hash]; hash++) {
			SCCP_RWLIST_WRLOCK(&(objects[hash]->refCountedObjects));
			SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&(objects[hash]->refCountedObjects), obj, list) {
				if (obj->type == type) {
					pbx_log(LOG_NOTICE, "Cleaning up [%3d]=type:%17s, id:%25s, ptr:%15p, refcount:%4d, alive:%4s, size:%4d\n", hash, (obj_info[obj->type]).datatype, obj->identifier, obj, (int) obj->refcount, SCCP_LIVE_MARKER == obj->alive ? "yes" : "no", obj->len);
					SCCP_RWLIST_REMOVE_CURRENT(list);
					if ((&obj_info[obj->type])->destructor) {
						(&obj_info[obj->type])->destructor(obj->data);
					}
#ifndef SCCP_ATOMIC
					ast_mutex_destroy(&obj->lock);
#endif
					memset(obj, 0, sizeof(RefCountedObject));
					sccp_free(obj);
					obj = NULL;
					numObjects++;
				}
			}
			SCCP_RWLIST_TRAVERSE_SAFE_END;
			SCCP_RWLIST_UNLOCK(&(objects[hash]->refCountedObjects));
			SCCP_RWLIST_HEAD_DESTROY(&(objects[hash]->refCountedObjects));

			sccp_free(objects[hash]);								// free hashtable entry
			objects[hash] = NULL;
		}
	}
	ast_rwlock_unlock(&objectslock);
	pbx_rwlock_destroy(&objectslock);
	if (numObjects) {
		pbx_log(LOG_WARNING, "SCCP: (Refcount) Note: We found %d objects which had to be forcefulfy removed during refcount shutdown, see above.\n", numObjects);
	}
#if CS_REFCOUNT_DEBUG
	if (sccp_ref_debug_log) {
		fclose(sccp_ref_debug_log);
		sccp_ref_debug_log = NULL;
		pbx_log(LOG_NOTICE, "SCCP: ref debug log file: %s closed\n", REF_DEBUG_FILE);
	}
#endif
	runState = SCCP_REF_DESTROYED;
}

int __PURE__ sccp_refcount_isRunning(void)
{
	return runState;
}

void *const sccp_refcount_object_alloc(size_t size, enum sccp_refcounted_types type, const char *identifier, void *destructor)
{
	RefCountedObject *obj;
	void *ptr = NULL;
	uint32_t hash;

	if (!runState) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_refcount_object_alloc) Not Running Yet!\n");
		return NULL;
	}

	if (!(obj = sccp_calloc(size + (sizeof *obj), 1) )) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP: obj");
		return NULL;
	}

	if (!(&obj_info[type])->destructor) {
		(&obj_info[type])->destructor = destructor;
	}
	// initialize object
	obj->len = (int)size;
	obj->type = type;
	obj->refcount = 1;
#ifndef SCCP_ATOMIC
	ast_mutex_init(&obj->lock);
#endif
	sccp_copy_string(obj->identifier, identifier, sizeof(obj->identifier));

	// generate hash
	ptr = obj->data;
	hash = SCCP_SIMPLE_HASH(ptr);

	if (!objects[hash]) {
		// create new hashtable head when necessary (should this possibly be moved to refcount_init, to avoid raceconditions ?)
		ast_rwlock_wrlock(&objectslock);
		if (!objects[hash]) {										// check again after getting the lock, to see if another thread did not create the head already
			if (!(objects[hash] = sccp_calloc(sizeof *objects[hash], 1))) {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCC: hashtable");
				sccp_free(obj);
				obj = NULL;
				ast_rwlock_unlock(&objectslock);
				return NULL;
			}
			SCCP_RWLIST_HEAD_INIT(&(objects[hash]->refCountedObjects));
			SCCP_RWLIST_INSERT_HEAD(&(objects[hash]->refCountedObjects), obj, list);
		}
		ast_rwlock_unlock(&objectslock);
	} else {
		// add object to hash table
		SCCP_RWLIST_WRLOCK(&(objects[hash]->refCountedObjects));
		SCCP_RWLIST_INSERT_HEAD(&(objects[hash]->refCountedObjects), obj, list);
		SCCP_RWLIST_UNLOCK(&(objects[hash]->refCountedObjects));
	}

	sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (alloc_obj) Creating new %s %s (%p) inside %p at hash: %d\n", (&obj_info[obj->type])->datatype, identifier, ptr, obj, hash);
	obj->alive = SCCP_LIVE_MARKER;

#if CS_REFCOUNT_DEBUG
	if (sccp_ref_debug_log) {
		fprintf(sccp_ref_debug_log, "%p|+1|%d|%s|%d|%s|**constructor**|%s:%s\n", ptr, ast_get_tid(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (&obj_info[obj->type])->datatype, obj->identifier);
		fflush(sccp_ref_debug_log);
	}
#endif
	//memset(ptr, 0, size);
	return (void * const)ptr;
}

#if CS_REFCOUNT_DEBUG
static int __rotate_debug_file(void)
{
	static uint32_t num_debug_files = 0;

	if (sccp_ref_debug_log) {
		if (fclose(sccp_ref_debug_log)) {
			pbx_log(LOG_ERROR, "SCCP: ref debug log file: %s close failed\n", REF_DEBUG_FILE);
			sccp_ref_debug_log = NULL;
			return -1;
		}
		sccp_ref_debug_log = NULL;
		
		num_debug_files++;
		char newfilename[SCCP_PATH_MAX];
		//snprintf(newfilename, SCCP_PATH_MAX, "%s/sccp_refs.%d.log", ast_config_AST_LOG_DIR, num_debug_files);
		snprintf(newfilename, SCCP_PATH_MAX, "%s.%d", REF_DEBUG_FILE, num_debug_files);
		if (rename(REF_DEBUG_FILE, newfilename)) {
			pbx_log(LOG_ERROR, "SCCP: ref debug log file: %s could not be moved to %s (%d)\n", REF_DEBUG_FILE, newfilename, errno);
			sccp_ref_debug_log = NULL;
			return -2;
		}
	}
	sccp_ref_debug_log = fopen(REF_DEBUG_FILE, "w");
	if (!sccp_ref_debug_log) {
		pbx_log(LOG_ERROR, "SCCP: Failed to open ref debug log file '%s'\n", REF_DEBUG_FILE);
		sccp_ref_debug_log = NULL;
		return -3;
	}
	ref_debug_size = 0;
	return 0;
}

static gcc_inline int __sccp_refcount_debug(const void *ptr, RefCountedObject * obj, int delta, const char *file, int line, const char *func)
{
	if (!sccp_ref_debug_log) {
		return -1;
	}
	int res = -1;
	static char fmt[] = "%p|%s%d|%d|%s|%d|%s|%d|%s:%s\n";

	if (!sccp_ref_debug_log || ref_debug_size > REF_DEBUG_FILE_MAX_SIZE) {			/* check debug file rotation requirement */
		ast_rwlock_wrlock(&objectslock);
		if (__rotate_debug_file() != 0) {
			ast_rwlock_unlock(&objectslock);
			return -1;
		}
		ast_rwlock_unlock(&objectslock);
	}
	if (sccp_ref_debug_log) {	
		do {
			if (ptr == NULL) {
				ref_debug_size += fprintf(sccp_ref_debug_log, fmt, NULL, "E", 0, NULL, ast_get_tid(),file, line, func, "**PTR IS NULL**", "", "");
				break;	
			}
			if (obj == NULL) {
				ref_debug_size += fprintf(sccp_ref_debug_log, fmt, NULL, "E", 0, NULL, ast_get_tid(),file, line, func, "**OBJ ALREADY DESTROYED**", "", "");
				break;
			}

			if (obj->alive != SCCP_LIVE_MARKER) {
				ref_debug_size += fprintf(sccp_ref_debug_log, fmt, ptr, "E", delta, ptr, ast_get_tid(),file, line, func, "**OBJ Already destroyed and Declared DEAD**", (&obj_info[obj->type])->datatype, obj->identifier);
				break;
			}

			res = 0;
			if (obj->refcount + delta == 0 && (&obj_info[obj->type])->destructor != NULL) {
				ref_debug_size += fprintf(sccp_ref_debug_log, "%p|-1|%d|%s|%d|%s|**destructor**|%s:%s\n", ptr, ast_get_tid(), file, line, func, (&obj_info[obj->type])->datatype, obj->identifier);
				break;
			}
			if (delta != 0) {
				ref_debug_size += fprintf(sccp_ref_debug_log, fmt, ptr, (delta < 0 ? "" : "+"), delta, ast_get_tid(), file, line, func, obj->refcount, (&obj_info[obj->type])->datatype, obj->identifier);
				break;
			}
			ref_debug_size += fprintf(sccp_ref_debug_log, fmt, ptr, "E", 0, ptr, ast_get_tid(),file, line, func, "**UNKNOWN**", "", "");
		} while (0);
		fflush(sccp_ref_debug_log);
	}
	return res;
}
#endif

static gcc_inline RefCountedObject *sccp_refcount_find_obj(const void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj = NULL;
	boolean_t found = FALSE;

	if (ptr == NULL) {
		return NULL;
	}

	int hash = SCCP_SIMPLE_HASH(ptr);

	if (objects[hash]) {
		SCCP_RWLIST_RDLOCK(&(objects[hash])->refCountedObjects);
		SCCP_RWLIST_TRAVERSE(&(objects[hash])->refCountedObjects, obj, list) {
			if (obj->data == ptr) {
				if (SCCP_LIVE_MARKER == obj->alive) {
					found = TRUE;
				} else {
#if CS_REFCOUNT_DEBUG
					__sccp_refcount_debug((void *) ptr, obj, 0, filename, lineno, func);
#endif
					sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (sccp_refcount_find_obj) %p Already declared dead (hash: %d)\n", obj, hash);
				}
				break;
			}
		}
		SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);
	}
	return found ? obj : NULL;
}

static gcc_inline void sccp_refcount_remove_obj(const void *ptr)
{
	RefCountedObject *obj = NULL;
	boolean_t cleanup_objects = FALSE;

	if (ptr == NULL) {
		return;
	}

	int hash = SCCP_SIMPLE_HASH(ptr);

	sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (sccp_refcount_remove_obj) Removing %p from hash table at hash: %d\n", ptr, hash);

	if (objects[hash]) {
		SCCP_RWLIST_WRLOCK(&(objects[hash])->refCountedObjects);
		SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&(objects[hash])->refCountedObjects, obj, list) {
			if (obj->data == ptr && SCCP_LIVE_MARKER != obj->alive) {
				SCCP_RWLIST_REMOVE_CURRENT(list);
				break;
			}
		}
		SCCP_RWLIST_TRAVERSE_SAFE_END;
		if (SCCP_RWLIST_GETSIZE(&(objects[hash])->refCountedObjects) == 0) {
			cleanup_objects = TRUE;
		}
		SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);
	}
	if (obj) {
		sched_yield();											// make sure all other threads can finish their work first.
		// should resolve lockless refcount SMP issues
		// BTW we are not allowed to sleep whilst haveing a reference
		// fire destructor
		if (obj && obj->data == ptr && SCCP_LIVE_MARKER != obj->alive) {
			sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (sccp_refcount_remove_obj) Destroying %p at hash: %d\n", obj, hash);
			if ((&obj_info[obj->type])->destructor) {
				(&obj_info[obj->type])->destructor(ptr);
			}
			memset(obj, 0, sizeof(RefCountedObject));
			sccp_free(obj);
			obj = NULL;
		}
	}
	if (cleanup_objects && runState == SCCP_REF_RUNNING && objects[hash]) {
		ast_rwlock_wrlock(&objectslock);
		SCCP_RWLIST_WRLOCK(&(objects[hash])->refCountedObjects);
		if (SCCP_RWLIST_GETSIZE(&(objects[hash])->refCountedObjects) == 0) {			/* recheck size */
			SCCP_RWLIST_HEAD_DESTROY(&(objects[hash])->refCountedObjects);
			sccp_free(objects[hash]);
			objects[hash] = NULL;
		} else {
			SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);
		}
		ast_rwlock_unlock(&objectslock);
	}
}

#if CS_REFCOUNT_DEBUG 
void sccp_refcount_gen_report(const void * const ptr, pbx_str_t **buf)
{
	pbx_str_append(buf, 0, "\n== refcount report =======================================================================\n");

	RefCountedObject *obj = sccp_refcount_find_obj(ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!obj) {
		pbx_log(LOG_NOTICE, "SCCP: (refcount_gen_report) Not Refcount Object found for %p\n", ptr);
		return;
	}
	pbx_str_append(buf, 0, " %-17.17s %-25.25s (%15p), refcount:%-4.4d, alive:%-5.5s\n", 
		(obj_info[obj->type]).datatype, 
		obj->identifier, 
		obj, 
		obj->refcount,
		SCCP_LIVE_MARKER == obj->alive ? "yes" : "no"
	);
	
	pbx_str_append(buf, 0, "== related objects =======================================================================\n");
	ast_rwlock_rdlock(&objectslock);
	RefCountedObject *rel_obj = NULL;
	for(int bucket = 0; bucket < SCCP_HASH_PRIME; bucket++) {
		if (objects[bucket]) {
			SCCP_RWLIST_RDLOCK(&(objects[bucket])->refCountedObjects);
			SCCP_RWLIST_TRAVERSE(&(objects[bucket])->refCountedObjects, rel_obj, list) {
				for (int parentIndex = 0; parentIndex < REFCOUNT_MAX_PARENTS; parentIndex++) {
					if (rel_obj->parentWeakPtr[parentIndex] && rel_obj->parentWeakPtr[parentIndex] == obj) {
						pbx_str_append(buf, 0, " %-17.17s %-25.25s (%15p), refcount:%-4.4d, alive:%-5.5s\n", 
							(obj_info[rel_obj->type]).datatype, 
							rel_obj->identifier, 
							rel_obj, 
							rel_obj->refcount,
							SCCP_LIVE_MARKER == rel_obj->alive ? "yes" : "no"
						);
					}
				}
			}
			SCCP_RWLIST_UNLOCK(&(objects[bucket])->refCountedObjects);
		}
	}
	ast_rwlock_unlock(&objectslock);
	pbx_str_append(buf, 0, "==========================================================================================\n");
}
#endif

int sccp_show_refcount(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_line_total = 0;
	int bucket, prev = 0;
	RefCountedObject *obj = NULL;
	unsigned int maxdepth = 0;
	unsigned int numentries = 0;
	int check_inuse = 0;
	boolean_t inuse = FALSE;
	float fillfactor = 0.00;

	if (argc == 4) {
		if (sccp_strcaseequals(argv[3],"show")) {
			check_inuse = 1;
		} else if (sccp_strcaseequals(argv[3],"suppress")) {
			check_inuse = 2;
		}
	}

	ast_rwlock_rdlock(&objectslock);
#define CLI_AMI_TABLE_NAME Refcount
#define CLI_AMI_TABLE_PER_ENTRY_NAME Entry
#define CLI_AMI_TABLE_ITERATOR for(bucket = 0; bucket < SCCP_HASH_PRIME; bucket++)
#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
		if (objects[bucket]) {											\
			SCCP_RWLIST_RDLOCK(&(objects[bucket])->refCountedObjects);					\
			SCCP_RWLIST_TRAVERSE(&(objects[bucket])->refCountedObjects, obj, list) {			\
				char bucketstr[6];									\
				if (!s) {										\
					if (prev == bucket) {								\
						snprintf(bucketstr, sizeof(bucketstr), " +-> ");			\
					} else {									\
						snprintf(bucketstr, sizeof(bucketstr), "[%3d]", bucket);		\
					}										\
				} else {										\
					snprintf(bucketstr, sizeof(bucketstr), "%d", bucket);				\
				}											\
				inuse = FALSE;										\
				if (check_inuse && obj->alive) {							\
					union {	/* little union trick to prevent cast-alignment warning	*/		\
						unsigned char *cdata;							\
						sccp_device_t *device;							\
						sccp_line_t *line;							\
						sccp_linedevices_t *linedevice;						\
						sccp_channel_t *channel;						\
					} data = {									\
						.cdata = obj->data,							\
					};										\
					if (obj->type == SCCP_REF_DEVICE) {						\
						if (data.device && data.device->session && data.device->active_channel) {\
							inuse = TRUE;							\
						}									\
					} else if (obj->type == SCCP_REF_LINE) {					\
						if (data.line && (							\
							data.line->statistic.numberOfActiveChannels || 			\
							data.line->statistic.numberOfHeldChannels)			\
						) {									\
							inuse = TRUE;							\
						}									\
					} else if (obj->type == SCCP_REF_LINEDEVICE) {					\
						sccp_linedevices_t *linedevice = data.linedevice;			\
						if (linedevice && linedevice->device && linedevice->line &&		\
							linedevice->device->session && linedevice->device->active_channel && \
							(linedevice->line->statistic.numberOfActiveChannels ||		\
							linedevice->line->statistic.numberOfHeldChannels)		\
						) {									\
							inuse = TRUE;							\
						}									\
					} else if (obj->type == SCCP_REF_CHANNEL) {					\
						if (data.channel && data.channel->owner) {				\
							inuse = TRUE;							\
						}									\
					}										\
				}											\
				if (check_inuse == 2 && inuse) {							\
					continue;									\
				}
				
#define CLI_AMI_TABLE_AFTER_ITERATION											\
				prev = bucket;										\
				numentries++;										\
			}												\
			if (maxdepth < SCCP_RWLIST_GETSIZE(&(objects[bucket])->refCountedObjects)) {			\
				maxdepth = SCCP_RWLIST_GETSIZE(&(objects[bucket])->refCountedObjects);			\
			}												\
			SCCP_RWLIST_UNLOCK(&(objects[bucket])->refCountedObjects);					\
		}

#define CLI_AMI_TABLE_FIELDS 												\
	CLI_AMI_TABLE_FIELD(Hash,	"-5.5",		s,	5,	bucketstr)					\
	CLI_AMI_TABLE_FIELD(Type,	"-17.17",	s,	17,	(obj_info[obj->type]).datatype)			\
	CLI_AMI_TABLE_FIELD(Id,		"-25.25",	s,	25,	obj->identifier)				\
	CLI_AMI_TABLE_FIELD(Ptr,	"-15",		p,	15,	obj)						\
	CLI_AMI_TABLE_FIELD(Refc,	"-4.4",		d,	4,	obj->refcount)					\
	CLI_AMI_TABLE_FIELD(Alive,	"-5.5",		s,	5,	SCCP_LIVE_MARKER == obj->alive ? "yes" : "no")	\
	CLI_AMI_TABLE_FIELD(InUse,	"-5.5",		s,	5,	check_inuse ? (inuse ? "yes" : "no") : "off")	\
	CLI_AMI_TABLE_FIELD(Size,	"-4.4",		d,	4,	obj->len)
#include "sccp_cli_table.h"
	local_line_total++;
	ast_rwlock_unlock(&objectslock);

	// FillFactor
	fillfactor = (float) numentries / SCCP_HASH_PRIME;
	int once;
#define CLI_AMI_TABLE_NAME FillFactor
#define CLI_AMI_TABLE_PER_ENTRY_NAME Factor
#define CLI_AMI_TABLE_ITERATOR for(once=0;once<1;once++)
#define CLI_AMI_TABLE_FIELDS 												\
	CLI_AMI_TABLE_FIELD(Entries,		"-8.8",		d,	8,	numentries)				\
	CLI_AMI_TABLE_FIELD(Buckets,		"-8.8",		d,	8,	SCCP_HASH_PRIME)			\
	CLI_AMI_TABLE_FIELD(Factor,		"08.02",	f,	8,	fillfactor)				\
	CLI_AMI_TABLE_FIELD(MaxDepth,		"-8.8",		d,	8,	maxdepth)
#include "sccp_cli_table.h"
	local_line_total++;
	if (fillfactor > 1.00) {
		if (!s) {
			pbx_cli(fd, "\033[1m\033[41m\033[37mPlease keep fillfactor below 1.00. Check ./configure --with-hash-size.\033[0m\n");
		} else {
			astman_append(s, "Please keep fillfactor below 1.00. Check ./configure --with-hash-size.\r\n");
			local_line_total++;
		}
	}

	if (s) {
		totals->lines = local_line_total;
		totals->tables = 2;
	}
	return RESULT_SUCCESS;
}

#ifdef CS_EXPERIMENTAL
int sccp_refcount_force_release(long findobj, char *identifier)
{
	uint32_t hash;
	RefCountedObject *obj = NULL;
	void *ptr = NULL;

	ast_rwlock_rdlock(&objectslock);
	for (hash = 0; hash < SCCP_HASH_PRIME; hash++) {
		if (objects[hash]) {
			SCCP_RWLIST_RDLOCK(&(objects[hash]->refCountedObjects));
			SCCP_RWLIST_TRAVERSE(&(objects[hash]->refCountedObjects), obj, list) {
				if (sccp_strequals(obj->identifier, identifier) && (long) obj == findobj) {
					ptr = obj->data;
				}
			}
			SCCP_RWLIST_UNLOCK(&(objects[hash]->refCountedObjects));
		}
	}
	ast_rwlock_unlock(&objectslock);
	if (ptr) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_1 "Forcefully releasing one instance of %s\n", identifier);
		sccp_refcount_release((const void ** const)&ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
		return 1;
	}
	return 0;
}
#endif

void sccp_refcount_updateIdentifier(const void * const ptr, const char * const identifier)
{
	RefCountedObject *obj = sccp_refcount_find_obj(ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!obj) {
		pbx_log(LOG_ERROR, "SCCP: (updateIdentifief) Refcount Object %p could not be found\n", ptr);
		return;
	}
	sccp_copy_string(obj->identifier, identifier, REFCOUNT_INDENTIFIER_SIZE);
}

#if CS_REFCOUNT_DEBUG 
void sccp_refcount_addWeakParent(const void * const ptr, const void * const parentWeakPtr)
{
	RefCountedObject *obj = sccp_refcount_find_obj(ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!obj) {
		pbx_log(LOG_ERROR, "SCCP: (addWeakParent) Refcount Object %p could not be found\n", ptr);
		return;
	}
	RefCountedObject *parent = sccp_refcount_find_obj(parentWeakPtr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!parent) {
		pbx_log(LOG_ERROR, "SCCP: (addWeakParent) Refcount Parent Object %p could not be found\n", parentWeakPtr);
		return;
	}
	for (int x = 0; x < REFCOUNT_MAX_PARENTS; x++) {
		if (obj->parentWeakPtr[x] && obj->parentWeakPtr[x] == parent) {
			break;
		}
		if (!obj->parentWeakPtr[x]) {
			obj->parentWeakPtr[x] = parent;
			break;
		}
	}
}

void sccp_refcount_removeWeakParent(const void * const ptr, const void * const parentWeakPtr)
{
	RefCountedObject *obj = sccp_refcount_find_obj(ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!obj) {
		pbx_log(LOG_ERROR, "SCCP: (removeWeakParent) Refcount Object %p could not be found\n", ptr);
		return;
	}
	RefCountedObject *parent = sccp_refcount_find_obj(parentWeakPtr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!parent) {
		pbx_log(LOG_ERROR, "SCCP: (removeWeakParent) Refcount Parent Object %p could not be found\n", parentWeakPtr);
		return;
	}
	for (int x = 0; x < REFCOUNT_MAX_PARENTS; x++) {
		if (obj->parentWeakPtr[x] && obj->parentWeakPtr[x] == parent) {
			obj->parentWeakPtr[x] = NULL;
			break;
		}
	}
}
#endif

gcc_inline void * const sccp_refcount_retain(const void * const ptr, const char *filename, int lineno, const char *func)
{
#if CS_REFCOUNT_DEBUG
	pbx_assert(ptr != NULL);
#else
	if (ptr == NULL) {											// soft failure
		pbx_log(LOG_WARNING, "SCCP: (refcount_retain) tried to retain a NULL pointer\n");
		usleep(10);
		return NULL;
	}
#endif	
	RefCountedObject *obj = NULL;
	volatile int refcountval;
	int newrefcountval;

	if (do_expect((obj = sccp_refcount_find_obj(ptr, filename, lineno, func)) != NULL)) {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug(ptr, obj, 1, filename, lineno, func);
#endif
		// ANNOTATE_HAPPENS_BEFORE(&obj->refcount);
		refcountval = ATOMIC_INCR((&obj->refcount), 1, &obj->lock);
		// ANNOTATE_HAPPENS_AFTER(&obj->refcount);
		newrefcountval = refcountval + 1;
		
		if (dont_expect( (sccp_globals->debug & (((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT))) == ((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT))) {
			pbx_log(__LOG_VERBOSE, __FILE__, 0, "", " %-15.15s:%-4.4d (%-35.35s) %*.*s> %*s refcount increased %.2d  +> %.2d for %10s: %s (%p)\n", filename, lineno, func, refcountval, refcountval, "--------------------", 20 - refcountval, " ", refcountval, newrefcountval, (&obj_info[obj->type])->datatype, obj->identifier, obj);
		}
		return (void * const) obj->data;	/* regular exit */
	} 
#if CS_REFCOUNT_DEBUG
	__sccp_refcount_debug((void *) ptr, NULL, 1, filename, lineno, func);
#endif
	pbx_log(__LOG_VERBOSE, __FILE__, 0, "retain", "SCCP: (%-15.15s:%-4.4d (%-35.35s)) ALARM !! trying to retain %p with invalid memory reference! this should never happen !\n", filename, lineno, func, obj);
	pbx_log(LOG_ERROR, "SCCP: (release) Refcount Object %p could not be found (Major Logic Error). Please report to developers\n", ptr);
	#ifdef DEBUG
	sccp_do_backtrace();
	#endif
	return NULL;
}

/*!
 * \brief reduces the refcount of the object passed in, if refcount reaches 0 the object will be put on the list of object to be destroyed.
 * release takes a pointer to a pointer to the object being released, the pointer to the object will be set to NULL after the release has been processed 
 */
gcc_inline void * const sccp_refcount_release(const void * * const ptr, const char *filename, int lineno, const char *func)
{
#if CS_REFCOUNT_DEBUG
	pbx_assert(ptr != NULL && *ptr != NULL);
#else
	if (ptr == NULL || *ptr == NULL) {									// soft failure
		pbx_log(LOG_WARNING, "SCCP: (refcount_release) tried to release a NULL pointer\n");
		usleep(10);
		return NULL;
	}
#endif
	RefCountedObject *obj = NULL;
	volatile int refcountval;
	int newrefcountval, alive;
	sccp_debug_category_t debugcat;

	if (do_expect( (obj = sccp_refcount_find_obj(*ptr, filename, lineno, func)) != NULL && obj->refcount > 0)) {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug((void *) *ptr, obj, -1, filename, lineno, func);
#endif
		debugcat = (&obj_info[obj->type])->debugcat;
		//do {
		//	refcountval = obj->refcount;
		//	newrefcountval = refcountval - 1;
		//} while (refcountval > 0 && (refcountval != CAS32(&obj->refcount, refcountval, newrefcountval, &obj->lock)));
		// ANNOTATE_HAPPENS_BEFORE(&obj->refcount);
		refcountval = ATOMIC_DECR((&obj->refcount), 1, &obj->lock);
		// ANNOTATE_HAPPENS_AFTER(&obj->refcount);
		
		newrefcountval = refcountval - 1;
		if (dont_expect(newrefcountval == 0)) {
			alive = ATOMIC_DECR(&obj->alive, SCCP_LIVE_MARKER, &obj->lock);
			sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: %-15.15s:%-4.4d (%-35.35s)) (release) Finalizing %p (%p) (alive:%d)\n", filename, lineno, func, obj, *ptr, alive);
			sccp_refcount_remove_obj(*ptr);
		} else {
			if (dont_expect( (sccp_globals->debug & ((debugcat + DEBUGCAT_REFCOUNT))) == (debugcat ^ DEBUGCAT_REFCOUNT))) {
				pbx_log(__LOG_VERBOSE, __FILE__, 0, "", " %-15.15s:%-4.4d (%-35.35s) <%*.*s %*s refcount decreased %.2d  <- %.2d for %10s: %s (%p)\n", filename, lineno, func, newrefcountval, newrefcountval, "--------------------", 20 - newrefcountval, " ", newrefcountval, refcountval, (&obj_info[obj->type])->datatype, obj->identifier, obj);
			}
		}
		*ptr = NULL;
		return NULL;	/* regular exit */
	}
#if CS_REFCOUNT_DEBUG
	__sccp_refcount_debug((void *) *ptr, NULL, -1, filename, lineno, func);
#endif
	pbx_log(__LOG_VERBOSE, __FILE__, 0, "release", "SCCP (%-15.15s:%-4.4d (%-35.35s)) ALARM !! trying to release a %p with invalid memory reference! this should never happen !\n", filename, lineno, func, obj);
	pbx_log(LOG_ERROR, "SCCP: (release) Refcount Object %p could not be found (Major Logic Error). Please report to developers\n", *ptr);
	#ifdef DEBUG
	sccp_do_backtrace();
	#endif
	*ptr = NULL;
	return NULL;
}

gcc_inline void sccp_refcount_replace(const void * * const replaceptr, const void *const newptr, const char *filename, int lineno, const char *func)
{
	if (!replaceptr || (&newptr == replaceptr)) {								// nothing changed
		return;
	}
	if (do_expect(newptr !=NULL)) {
		const void *tmpNewPtr = sccp_refcount_retain(newptr, filename, lineno, func);			// retain new one first
		if (do_expect(tmpNewPtr != NULL)) {
			const void *oldPtr = *replaceptr;
			*replaceptr = tmpNewPtr;
			if (do_expect(oldPtr != NULL)) {							// release previous one after
				sccp_refcount_release(&oldPtr, filename, lineno, func);				// explicit release
			}
		}
	} else if (do_expect(*replaceptr != NULL)) {								// release previous only
		sccp_refcount_release(replaceptr, filename, lineno, func);					// explicit release
	}
}

/*
 * \brief refence autorelease helper
 * Used together with the cleanup attribute, to handle the automatic reference release of an object when we leave the scope in which the 
 * reference was defined. 
 */
gcc_inline void sccp_refcount_autorelease(void *refptr)
{
	auto_ref_t *ref = refptr;
	if (ref && ref->ptr && *ref->ptr) {
		sccp_refcount_release(ref->ptr, ref->file, ref->line, ref->func);				// explicit release
	}
}

#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
#define NUM_LOOPS 50
#define NUM_OBJECTS 5000
#define NUM_THREADS 10
struct refcount_test {
	char *str;
	int id;
	int loop;
	unsigned int threadid;
};

struct refcount_test **object;

static void refcount_test_destroy(struct refcount_test *obj)
{
	sccp_free(object[obj->id]->str);
	object[obj->id]->str = NULL;
};

static void *refcount_test_thread(void *data)
{
	enum ast_test_result_state *test_result = data;
	struct refcount_test *obj = NULL, *obj1 = NULL;
	int loop, objloop;
	int random_object;
	int threadid = (unsigned int) pthread_self();
	
	*test_result = AST_TEST_PASS;

	pbx_log(LOG_NOTICE, "%d: Thread running...\n", threadid);
	for (loop = 0; loop < NUM_LOOPS; loop++) {
		for (objloop = 0; objloop < NUM_OBJECTS; objloop++) {
			random_object = rand() % NUM_OBJECTS;
			if ((obj = sccp_refcount_retain(object[random_object], __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
				if ((obj1 = sccp_refcount_retain(obj, __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
					if ((obj1 = sccp_refcount_release((const void ** const)&obj1, __FILE__, __LINE__, __PRETTY_FUNCTION__)) != NULL) {
						pbx_log(LOG_NOTICE, "%d: release obj1 failed\n", threadid);
						*test_result = AST_TEST_FAIL;
						break;
					}
				} else {
					pbx_log(LOG_NOTICE, "%d: retain obj1 failed\n", threadid);
					*test_result = AST_TEST_FAIL;
					break;
				}
				if ((obj = sccp_refcount_release((const void ** const)&obj, __FILE__, __LINE__, __PRETTY_FUNCTION__)) != NULL) {
					pbx_log(LOG_NOTICE, "%d: release obj failed\n", threadid);
					*test_result = AST_TEST_FAIL;
					break;
				}
			} else {
				pbx_log(LOG_NOTICE, "%d: retain obj failed\n", threadid);
				*test_result = AST_TEST_FAIL;
				break;
			}
		}
		if (*test_result == AST_TEST_FAIL) {
			break;
		}
		if (loop % 10) {
			pbx_log(LOG_NOTICE, "%d: loop:%d: retained/released %d objects\n", threadid, loop, loop * NUM_OBJECTS);
		}
	}
	pbx_log(LOG_NOTICE, "%d: Thread finished: %s\n", threadid, *test_result ? "Success" : "Failed");
	return NULL;
}


AST_TEST_DEFINE(sccp_refcount_tests)
{
	int thread;
	
	switch(cmd) {
		case TEST_INIT:
			info->name = "refcount";
			info->category = "/channels/chan_sccp/";
			info->summary = "chan-sccp-b refcount test";
			info->description = "chan-sccp-b refcount tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	
	pthread_t t[NUM_THREADS];
	int loop;
	char id[23];
	enum ast_test_result_state test_result[NUM_THREADS] = {AST_TEST_PASS};
	
	object = sccp_malloc(sizeof(struct refcount_test) * NUM_OBJECTS);

	pbx_test_status_update(test, "Executing chan-sccp-b refcount tests...\n");
	pbx_test_status_update(test, "Create %d objects to work on...\n", NUM_OBJECTS);
	for (loop = 0; loop < NUM_OBJECTS; loop++) {
		snprintf(id, sizeof(id), "%d/%d", loop, (unsigned int) pthread_self());
		object[loop] = (struct refcount_test *) sccp_refcount_object_alloc(sizeof(struct refcount_test), SCCP_REF_TEST, id, refcount_test_destroy);
		pbx_test_validate(test, object[loop] != NULL);
		object[loop]->id = loop;
		object[loop]->threadid = (unsigned int) pthread_self();
		object[loop]->str = pbx_strdup(id);
		//pbx_test_status_update(test, "created %d'\n", object[loop]->id);
	}
	sleep(1);

	pbx_test_status_update(test, "Run multithreaded retain/release/destroy at random in %d loops and %d threads...\n", NUM_LOOPS, NUM_THREADS);
	for (thread = 0; thread < NUM_THREADS; thread++) {
		pbx_pthread_create(&t[thread], NULL, refcount_test_thread, &test_result[thread]);
	}
	for (thread = 0; thread < NUM_THREADS; thread++) {
		pthread_join(t[thread], NULL);
		pbx_test_validate(test, test_result[thread] == AST_TEST_PASS);
		pbx_test_status_update(test, "thread %d finished with %s\n", thread, test_result[thread] == AST_TEST_PASS ? "Success" : "Failure");
	}
	sleep(1);

	pbx_test_status_update(test, "Finalize test / cleanup...\n");
	for (loop = 0; loop < NUM_OBJECTS; loop++) {
		if (object[loop]) {
			//pbx_test_status_update(test, "Final Release %d, thread: %d\n", object[loop]->id, (unsigned int) pthread_self());
			object[loop] = sccp_refcount_release((const void ** const)&object[loop], __FILE__, __LINE__, __PRETTY_FUNCTION__);
			pbx_test_validate(test, object[loop] == NULL);
		}
	}
	sleep(1);

	/* peer directly inside refcounted objects to see if there are any stranded refcounted objects, which should have been destroyed */
	ast_rwlock_rdlock(&objectslock);
	RefCountedObject *obj = NULL;
	for (loop = 0; loop < SCCP_HASH_PRIME; loop++) {
		if (objects[loop]) {
			SCCP_RWLIST_RDLOCK(&(objects[loop])->refCountedObjects);
			SCCP_RWLIST_TRAVERSE(&(objects[loop])->refCountedObjects, obj, list) {
				pbx_test_validate(test, obj->type != SCCP_REF_TEST);
			}
			SCCP_RWLIST_UNLOCK(&(objects[loop])->refCountedObjects);
		}
	}
	ast_rwlock_unlock(&objectslock);
	sccp_free(object);
	return AST_TEST_PASS;
}

static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(sccp_refcount_tests);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(sccp_refcount_tests);
}
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
