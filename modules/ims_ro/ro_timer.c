/* 
 * File:   ro_timer.c
 * Author: Jason Penton
 *
 * Created on 06 April 2011, 1:37 PM
 */

#include <stdio.h>
#include <stdlib.h>

#include "../../mem/shm_mem.h"
#include "ro_timer.h"
#include "ro_session_hash.h"

/*! global dialog timer */
struct ro_timer *roi_timer = 0;
/*! global dialog timer handler */
ro_timer_handler timer_hdl = 0;

/*!
 * \brief Initialize the ro_session timer handler
 * Initialize the ro_session timer handler, allocate the lock and a global
 * timer in shared memory. The global timer handler will be set on success.
 * \param hdl ro_session timer handler
 * \return 0 on success, -1 on failure
 */
int init_ro_timer(ro_timer_handler hdl) {
    roi_timer = (struct ro_timer*) shm_malloc(sizeof (struct ro_timer));
    if (roi_timer == 0) {
        LM_ERR("no more shm mem\n");
        return -1;
    }
    memset(roi_timer, 0, sizeof (struct ro_timer));

    roi_timer->first.next = roi_timer->first.prev = &(roi_timer->first);

    roi_timer->lock = lock_alloc();
    if (roi_timer->lock == 0) {
        LM_ERR("failed to alloc lock\n");
        goto error0;
    }

    if (lock_init(roi_timer->lock) == 0) {
        LM_ERR("failed to init lock\n");
        goto error1;
    }

    timer_hdl = hdl;
    return 0;
error1:
    lock_dealloc(roi_timer->lock);
error0:
    shm_free(roi_timer);
    roi_timer = 0;
    return -1;
}

/*!
 * \brief Destroy global ro_session timer
 */
void destroy_ro_timer(void) {
    if (roi_timer == 0)
        return;

    lock_destroy(roi_timer->lock);
    lock_dealloc(roi_timer->lock);

    shm_free(roi_timer);
    roi_timer = 0;
}

/*!
 * \brief Helper function for insert_ro_session_timer
 * \see insert_ro_session_timer
 * \param tl ro_session timer list
 */
static inline void insert_ro_timer_unsafe(struct ro_tl *tl) {
    struct ro_tl* ptr;

    /* insert in sorted order */
    for (ptr = roi_timer->first.prev; ptr != &roi_timer->first; ptr = ptr->prev) {
        if (ptr->timeout <= tl->timeout)
            break;
    }

    LM_DBG("inserting %p for %d\n", tl, tl->timeout);
    tl->prev = ptr;
    tl->next = ptr->next;
    tl->prev->next = tl;
    tl->next->prev = tl;
}

/*!
 * \brief Insert a ro_session timer to the list
 * \param tl ro_session timer list
 * \param interval timeout value in seconds
 * \return 0 on success, -1 when the input timer list is invalid
 */
int insert_ro_timer(struct ro_tl *tl, int interval) {
    lock_get(roi_timer->lock);

    LM_DBG("inserting timer for interval [%i]\n", interval);
    if (tl->next != 0 || tl->prev != 0) {
        lock_release(roi_timer->lock);
        LM_CRIT("Trying to insert a bogus dlg tl=%p tl->next=%p tl->prev=%p\n",
                tl, tl->next, tl->prev);
        return -1;
    }
    tl->timeout = get_ticks() + interval;
    insert_ro_timer_unsafe(tl);

    lock_release(roi_timer->lock);

    return 0;
}

/*!
 * \brief Helper function for remove_ro_session_timer
 * \param tl ro_session timer list
 * \see remove_ro_session_timer
 */
static inline void remove_ro_timer_unsafe(struct ro_tl *tl) {
    tl->prev->next = tl->next;
    tl->next->prev = tl->prev;
}

/*!
 * \brief Remove a ro_session timer from the list
 * \param tl ro_session timer that should be removed
 * \return 1 when the input timer is empty, 0 when the timer was removed,
 * -1 when the input timer list is invalid
 */
int remove_ro_timer(struct ro_tl *tl) {
    lock_get(roi_timer->lock);

    if (tl->prev == NULL && tl->timeout == 0) {
        lock_release(roi_timer->lock);
        return 1;
    }

    if (tl->prev == NULL || tl->next == NULL) {
        LM_CRIT("bogus tl=%p tl->prev=%p tl->next=%p\n",
                tl, tl->prev, tl->next);
        lock_release(roi_timer->lock);
        return -1;
    }

    remove_ro_timer_unsafe(tl);
    tl->next = NULL;
    tl->prev = NULL;
    tl->timeout = 0;

    lock_release(roi_timer->lock);
    return 0;
}

/*!
 * \brief Update a ro_session timer on the list
 * \param tl dialog timer
 * \param timeout new timeout value in seconds
 * \return 0 on success, -1 when the input list is invalid
 * \note the update is implemented as a remove, insert
 */
int update_ro_timer(struct ro_tl *tl, int timeout) {
    lock_get(roi_timer->lock);

    if (tl->next) {
        if (tl->prev == 0) {
            lock_release(roi_timer->lock);
            return -1;
        }
        remove_ro_timer_unsafe(tl);
    }

    tl->timeout = get_ticks() + timeout;
    insert_ro_timer_unsafe(tl);

    lock_release(roi_timer->lock);
    return 0;
}

/*!
 * \brief Helper function for ro_timer_routine
 * \param time time for expiration check
 * \return list of expired credit reservations on sessions on success, 0 on failure
 */
static inline struct ro_tl* get_expired_ro_sessions(unsigned int time) {
    struct ro_tl *tl, *end, *ret;

    lock_get(roi_timer->lock);

    if (roi_timer->first.next == &(roi_timer->first) || roi_timer->first.next->timeout > time) {
        lock_release(roi_timer->lock);
        return 0;
    }

    end = &roi_timer->first;
    tl = roi_timer->first.next;
    LM_DBG("start with tl=%p tl->prev=%p tl->next=%p (%d) at %d and end with end=%p end->prev=%p end->next=%p\n", tl, tl->prev, tl->next, tl->timeout, time, end, end->prev, end->next);
    while (tl != end && tl->timeout <= time) {
        LM_DBG("getting tl=%p tl->prev=%p tl->next=%p with %d\n", tl, tl->prev, tl->next, tl->timeout);
        tl->prev = 0;
        tl->timeout = 0;
        tl = tl->next;
    }
    LM_DBG("end with tl=%p tl->prev=%p tl->next=%p and d_timer->first.next->prev=%p\n", tl, tl->prev, tl->next, roi_timer->first.next->prev);

    if (tl == end && roi_timer->first.next->prev) {
        ret = 0;
    } else {
        ret = roi_timer->first.next;
        tl->prev->next = 0;
        roi_timer->first.next = tl;
        tl->prev = &roi_timer->first;
    }

    lock_release(roi_timer->lock);

    return ret;
}

/*!
 * \brief Timer routine for expiration of credit reservations
 * Timer handler for expiration of credit reservations on a session, runs the global timer handler on them.
 * \param time for expiration checks
 * \param attr unused
 */
void ro_timer_routine(unsigned int ticks, void * attr) {

    struct ro_tl *tl, *ctl;

    tl = get_expired_ro_sessions(ticks);

    while (tl) {
        ctl = tl;
        tl = tl->next;
        ctl->next = NULL;
        LM_DBG("Ro Session Timer firing: tl=%p next=%p\n", ctl, tl);
        timer_hdl(ctl);
    }
}

