/*
 * ro_session.h
 *
 *  Created on: 27 Feb 2013
 *      Author: jaybeepee
 */

#ifndef RO_SESSION_H_
#define RO_SESSION_H_

#include "../../locking.h"
#include "ro_session.h"
#include "ro_timer.h"

enum ro_session_event_type {
	pending, answered, no_more_credit, unknown_error
};

struct ro_session {
	volatile int ref;
	struct ro_session* next;
	struct ro_session* prev;
	str ro_session_id;
	str callid;
	str from_uri;
	str to_uri;
	unsigned int hop_by_hop;
	struct ro_tl ro_tl;
	unsigned int reserved_secs;
	unsigned int valid_for;
	unsigned int dlg_h_entry;
	unsigned int dlg_h_id;
	unsigned int h_entry;
	unsigned int h_id;
	time_t start_time;
	time_t last_event_timestamp;
	enum ro_session_event_type event_type;
	int auth_appid;
	int auth_session_type;
};

/*! entries in the main ro_session table */
struct ro_session_entry {
	struct ro_session *first; /*!< dialog list */
	struct ro_session *last; /*!< optimisation, end of the dialog list */
	unsigned int next_id; /*!< next id */
	unsigned int lock_idx; /*!< lock index */
};

/*! main ro_sesion table */
struct ro_session_table {
	unsigned int size; /*!< size of the dialog table */
	struct ro_session_entry *entries; /*!< dialog hash table */
	unsigned int locks_no; /*!< number of locks */
	gen_lock_set_t *locks; /*!< lock table */
};

/*! global ro_session table */
extern struct ro_session_table *ro_session_table;

struct ro_session* build_new_ro_session(int auth_appid, int auth_session_type,
		str *session_id, str *callid, str *from_uri, str* to_uri,
		unsigned int dlg_h_entry, unsigned int dlg_h_id,
		unsigned int requested_secs, unsigned int validity_timeout);

#endif /* RO_SESSION_H_ */
