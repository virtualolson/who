/*
 * ro_session.c
 *
 *  Created on: 27 Feb 2013
 *      Author: jaybeepee
 */
#include <stdlib.h>
#include <string.h>
#include "../../dprint.h"
#include "ro_session.h"
#include "ro_timer.h"


struct ro_session* build_new_ro_session(int auth_appid,
		int auth_session_type, str *session_id, str *callid, str *from_uri,
		str* to_uri, unsigned int dlg_h_entry, unsigned int dlg_h_id,
		unsigned int requested_secs, unsigned int validity_timeout) {

	LM_DBG("Building Ro Session **********");
	char *p;
	unsigned int len = session_id->len + callid->len + from_uri->len
			+ to_uri->len + sizeof(struct ro_session);
	struct ro_session *new_ro_session = (struct ro_session*) shm_malloc(len);

	if (!new_ro_session) {
		LM_ERR("no more shm mem.\n");
		shm_free(new_ro_session);
		return 0;
	}

	memset(new_ro_session, 0, len);

	new_ro_session->auth_appid = auth_appid;
	new_ro_session->auth_session_type = auth_session_type;

	new_ro_session->ro_tl.next = new_ro_session->ro_tl.prev;
	new_ro_session->ro_tl.timeout = 0; //requested_secs;

	new_ro_session->reserved_secs = requested_secs;
	new_ro_session->valid_for = validity_timeout;

	new_ro_session->hop_by_hop = 1;
	new_ro_session->next = 0;
	new_ro_session->dlg_h_entry = dlg_h_entry;
	new_ro_session->dlg_h_id = dlg_h_id;

	new_ro_session->h_entry = dlg_h_entry; /* we will use the same entry ID as the dlg - saves us using our own hash function */
	new_ro_session->h_id = 0;
	new_ro_session->ref = 0;

	p = (char*) (new_ro_session + 1);
	new_ro_session->callid.s = p;
	new_ro_session->callid.len = callid->len;
	memcpy(p, callid->s, callid->len);
	p += callid->len;

	new_ro_session->ro_session_id.s = p;
	new_ro_session->ro_session_id.len = session_id->len;
	memcpy(p, session_id->s, session_id->len);
	p += session_id->len;

	new_ro_session->from_uri.s = p;
	new_ro_session->from_uri.len = from_uri->len;
	memcpy(p, from_uri->s, from_uri->len);
	p += from_uri->len;

	new_ro_session->to_uri.s = p;
	new_ro_session->to_uri.len = to_uri->len;
	memcpy(p, to_uri->s, to_uri->len);
	p += to_uri->len;

	if (p != (((char*) new_ro_session) + len)) {
		LM_ERR("buffer overflow\n");
		shm_free(new_ro_session);
		return 0;
	}

	return new_ro_session;

}

