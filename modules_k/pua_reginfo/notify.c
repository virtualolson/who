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

#include "notify.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_content.h"
#include "../../modules_k/usrloc/usrloc.h"
#include <libxml/parser.h>

/*<?xml version="1.0"?>
<reginfo xmlns="urn:ietf:params:xml:ns:reginfo" version="0" state="full">
.<registration aor="sip:carsten@ng-voice.com" id="0xb33fa860" state="active">
..<contact id="0xb33fa994" state="active" event="registered" expires="3600">
...<uri>sip:carsten@10.157.87.36:43582;transport=udp</uri>
...<unknown-param name="+g.3gpp.cs-voice"></unknown-param>
...<unknown-param name="+g.3gpp.icsi-ref">urn0X0.0041FB74E7B54P-1022urn-70X0P+03gpp-application.ims.iari.gsma-vs</unknown-param>
...<unknown-param name="audio"></unknown-param>
...<unknown-param name="+g.oma.sip-im.large-message"></unknown-param>
...<unknown-param name="+g.3gpp.smsip"></unknown-param>
...<unknown-param name="language">en,fr</unknown-param>
...<unknown-param name="+g.oma.sip-im"></unknown-param>
...<unknown-param name="expires">600000</unknown-param>
..</contact>
.</registration>
</reginfo> */

#define STATE_ACTIVE 1
#define STATE_TERMINATED 0
#define STATE_UNKNOWN -1

#define EVENT_UNKNOWN -1
#define EVENT_REGISTERED 0
#define EVENT_UNREGISTERED 1
#define EVENT_TERMINATED 2
#define EVENT_CREATED 3
#define EVENT_REFRESHED 4
#define EVENT_EXPIRED 5

xmlNodePtr xmlGetNodeByName(xmlNodePtr parent, const char *name) {
	xmlNodePtr cur = parent;
	xmlNodePtr match = NULL;
	while (cur) {
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0)
			return cur;
		match = xmlGetNodeByName(cur->children, name);
		if (match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char * xmlGetAttrContentByName(xmlNodePtr node, const char *name) {
	xmlAttrPtr attr = node->properties;
	while (attr) {
		if (xmlStrcasecmp(attr->name, (unsigned char*)name) == 0)
			return (char*)xmlNodeGetContent(attr->children);
		attr = attr->next;
	}
	return NULL;
}

int reginfo_parse_state(char * s) {
	if (s == NULL) {
		return STATE_UNKNOWN;
	}
	switch (strlen(s)) {
		case 6:
			if (strncmp(s, "active", 6) ==  0) return STATE_ACTIVE;
			break;
		case 10:
			if (strncmp(s, "terminated", 10) ==  0) return STATE_TERMINATED;
			break;
		default:
			LM_ERR("Unknown State %s\n", s);
			return STATE_UNKNOWN;
	}
	LM_ERR("Unknown State %s\n", s);
	return STATE_UNKNOWN;
}

int reginfo_parse_event(char * s) {
	if (s == NULL) {
		return EVENT_UNKNOWN;
	}
	switch (strlen(s)) {
		case 7:
			if (strncmp(s, "created", 7) ==  0) return EVENT_CREATED;
			if (strncmp(s, "expired", 7) ==  0) return EVENT_EXPIRED;
			break;
		case 9:
			if (strncmp(s, "refreshed", 9) ==  0) return EVENT_CREATED;
			break;
		case 10:
			if (strncmp(s, "registered", 10) ==  0) return EVENT_REGISTERED;
			if (strncmp(s, "terminated", 10) ==  0) return EVENT_TERMINATED;
			break;
		case 12:
			if (strncmp(s, "unregistered", 12) ==  0) return EVENT_UNREGISTERED;
			break;
		default:
			LM_ERR("Unknown Event %s\n", s);
			return EVENT_UNKNOWN;
	}
	LM_ERR("Unknown Event %s\n", s);
	return EVENT_UNKNOWN;
}

int process_body(str notify_body, udomain_t * domain) {
	xmlNodePtr doc_root = NULL;
	xmlDocPtr doc= NULL;
	xmlNodePtr registrations = NULL;
	xmlNodePtr contacts = NULL;
	xmlNodePtr uris = NULL;
	str aor = {0, 0};
	str callid = {0, 0};
	str contact_uri = {0, 0};
	int state;
	int event;
	char * expires_char;
	int expires;

	doc = xmlParseMemory(notify_body.s, notify_body.len);
	if(doc== NULL)  {
		LM_ERR("Error while parsing the xml body message, Body is:\n%.*s\n",
			notify_body.len, notify_body.s);
		return -1;
	}
	doc_root = xmlGetNodeByName(doc->children, "reginfo");
	if(doc_root == NULL) {
		LM_ERR("while extracting the reginfo node\n");
		goto error;
	}
	registrations = doc_root->children;
	while (registrations) {
		/* Only process registration sub-items */
		if (xmlStrcasecmp(registrations->name, BAD_CAST "registration") != 0)
			goto next_registration;
		state = reginfo_parse_state(xmlGetAttrContentByName(registrations, "state"));
		if (state == STATE_UNKNOWN) {
			LM_ERR("No state for this contact!\n");		
			goto next_registration;
		}
		aor.s = xmlGetAttrContentByName(registrations, "aor");
		if (aor.s == NULL) {
			LM_ERR("No AOR for this contact!\n");		
			goto next_registration;
		}
		aor.len = strlen(aor.s);
		LM_ERR("AOR %.*s has state \"%d\"\n", aor.len, aor.s, state);
		/* Now lets process the Contact's from this Registration: */
		contacts = registrations->children;
		while (contacts) {
			if (xmlStrcasecmp(contacts->name, BAD_CAST "contact") != 0)
				goto next_contact;
			callid.s = xmlGetAttrContentByName(contacts, "id");
			if (callid.s == NULL) {
				LM_ERR("No Call-ID for this contact!\n");		
				goto next_contact;
			}
			callid.len = strlen(callid.s);

			event = reginfo_parse_event(xmlGetAttrContentByName(contacts, "event"));
			if (event == EVENT_UNKNOWN) {
				LM_ERR("No event for this contact!\n");		
				goto next_contact;
			}
			expires_char = xmlGetAttrContentByName(contacts, "expires");
			if (expires_char == NULL) {
				LM_ERR("No expires for this contact!\n");		
				goto next_contact;
			}
			expires = atoi(expires_char);
			if (expires < 0) {
				LM_ERR("No valid expires for this contact!\n");		
				goto next_contact;
			}
			LM_ERR("%.*s: Event \"%d\", expires %d\n",
				callid.len, callid.s, event, expires);
			/* Now lets process the URI's from this Contact: */
			uris = contacts->children;
			while (uris) {
				if (xmlStrcasecmp(uris->name, BAD_CAST "uri") != 0)
					goto next_uri;
				contact_uri.s = (char*)xmlNodeGetContent(uris);	
				if (contact_uri.s == NULL) {
					LM_ERR("No URI for this contact!\n");		
					goto next_registration;
				}
				contact_uri.len = strlen(contact_uri.s);
				LM_ERR("Contact: %.*s\n",
					contact_uri.len, contact_uri.s);
next_uri:
				uris = uris->next;
			}
next_contact:
			contacts = contacts->next;
		}
next_registration:
		registrations = registrations->next;
	}
	/* Free the XML-Document */
    	if(doc) xmlFreeDoc(doc);

	return 1;
error:
	/* Free the XML-Document */
    	if(doc) xmlFreeDoc(doc);
	return -1;
}

int reginfo_handle_notify(struct sip_msg* msg, char* domain, char* s2) {
 	str body;

  	/* If not done yet, parse the whole message now: */
  	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
  		LM_ERR("Error parsing headers\n");
  		return -1;
  	}
	if (get_content_length(msg) == 0) {
   		LM_DBG("Content length = 0\n");
		/* No Body? Then there is no published information available, which is ok. */
   		return 1;
   	} else {
   		body.s=get_body(msg);
   		if (body.s== NULL) {
   			LM_ERR("cannot extract body from msg\n");
   			return -1;
   		}
   		body.len = get_content_length(msg);
   	}

	LM_DBG("Body is %.*s\n", body.len, body.s);
	
	return process_body(body, (udomain_t*)domain);
}

