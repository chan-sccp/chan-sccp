
/*!
 * \file 	sccp_refcount.h
 * \brief 	SCCP RefCount Header
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_REFCOUNT_H
#    define __SCCP_REFCOUNT_H

typedef struct refcount_object RefCountedObject;

void sccp_refcount_init(void);
void sccp_refcount_destroy(void);
int sccp_refcount_isRunning(void);
int sccp_refcount_schedule_cleanup(const void *data);
void *sccp_refcount_object_alloc(size_t size, const char *datatype, const char *identifier, void *destructor);
void sccp_refcount_updateIdentifier(void *ptr, char *identifier);
inline void *sccp_refcount_retain(void *ptr, const char *filename, int lineno, const char *func);
inline void *sccp_refcount_release(const void *ptr, const char *filename, int lineno, const char *func);
#endif										// __SCCP_REFCOUNT_H
