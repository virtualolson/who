/*
 * pua_reginfo module - Presence-User-Agent Handling of reg events
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "subscribe.h"
#include "../../pvar.h"

int reginfo_subscribe(struct sip_msg* msg, char* uri, char* s2) {
	char uri_buf[512];
	int uri_len = 511;
	
	if (pv_printf(msg, (pv_elem_t*)uri, uri_buf, &uri_len) < 0) {
		LM_ERR("cannot print uri into the format\n");
		return -1;
	}

	LM_ERR("Subscribing to %.*s\n", uri_len, uri_buf);

	return 1;
}

int fixup_subscribe(void** param, int param_no) {
	pv_elem_t *model;
	str s;
	if (param_no == 1) {
		if(*param) {
			s.s = (char*)(*param); s.len = strlen(s.s);
			if(pv_parse_format(&s, &model)<0) {
				LM_ERR("wrong format[%s]\n",(char*)(*param));
				return E_UNSPEC;
			}
			*param = (void*)model;
			return 1;
		}
		LM_ERR("null format\n");
		return E_UNSPEC;
	} else return 1;
}

