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

#include "usrloc_cb.h"
#include "pua_reginfo.h"
#include <libxml/parser.h>
#include "../pua/pua.h"

/*
Contact: <sip:carsten@10.157.87.36:44733;transport=udp>;expires=600000;+g.oma.sip-im;language="en,fr";+g.3gpp.smsip;+g.oma.sip-im.large-message;audio;+g.3gpp.icsi-ref="urn%3Aurn-7%3A3gpp-application.ims.iari.gsma-vs";+g.3gpp.cs-voice.
Call-ID: 9ad9f89f-164d-bb86-1072-52e7e9eb5025.
*/

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


str* build_reginfo_full(urecord_t * record, str uri) {
	xmlDocPtr  doc = NULL; 
	xmlNodePtr root_node = NULL;
	xmlNodePtr registration_node = NULL;
	xmlNodePtr contact_node = NULL;
	xmlNodePtr uri_node = NULL;
	str * body= NULL;
	ucontact_t * ptr;
	char buf[30];
	int buf_len;

	/* create the XML-Body */
	doc = xmlNewDoc(BAD_CAST "1.0");
	if(doc==0) {
		LM_ERR("Unable to create XML-Doc\n");
		return NULL;
	}

	root_node = xmlNewNode(NULL, BAD_CAST "reginfo");
	if(root_node==0) {
		LM_ERR("Unable to create reginfo-XML-Element\n");
		return NULL;
	}
	/* This is our Root-Element: */
    	xmlDocSetRootElement(doc, root_node);
	
	xmlNewProp(root_node, BAD_CAST "xmlns",	BAD_CAST "urn:ietf:params:xml:ns:reginfo");

	/* we set the version to 0 but it should be set to the correct value in the pua module */
	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "0");
	xmlNewProp(root_node, BAD_CAST "state", BAD_CAST "full" );

	/* Registration Node */
	registration_node =xmlNewChild(root_node, NULL, BAD_CAST "registration", NULL) ;
	if( registration_node ==NULL) {
		LM_ERR("while adding child\n");
		goto error;
	}

	/* Add the properties to this Node for AOR and ID: */
	xmlNewProp(registration_node, BAD_CAST "aor", BAD_CAST uri.s);
	buf_len = snprintf(buf, sizeof(buf), "%p", record);
	xmlNewProp(registration_node, BAD_CAST "id", BAD_CAST buf);

	/* look first for an un-expired and suported contact */
	ptr = record->contacts;
	while ((ptr) && !(VALID_CONTACT(ptr,time(0))))
		ptr = ptr->next;
	if (ptr==0)
		xmlNewProp(registration_node, BAD_CAST "state", BAD_CAST "terminated");
	else
		xmlNewProp(registration_node, BAD_CAST "state", BAD_CAST "active");

	ptr = record->contacts;
	while (ptr) {
		if (VALID_CONTACT(ptr, time(0))) {
			/* Contact-Node */
			contact_node =xmlNewChild(registration_node, NULL, BAD_CAST "contact", NULL) ;
			if( contact_node ==NULL) {
				LM_ERR("while adding child\n");
				goto error;
			}
			buf_len = snprintf(buf, sizeof(buf), "%p", ptr);
			xmlNewProp(contact_node, BAD_CAST "id", BAD_CAST buf);
			xmlNewProp(contact_node, BAD_CAST "state", BAD_CAST "active");
			xmlNewProp(contact_node, BAD_CAST "event", BAD_CAST "registered");
			buf_len = snprintf(buf, sizeof(buf), "%i", (int)(ptr->expires-time(0)));
			xmlNewProp(contact_node, BAD_CAST "expires", BAD_CAST buf);
			if (ptr->q != Q_UNSPECIFIED) {
				float q = (float)ptr->q/1000;
				buf_len = snprintf(buf, sizeof(buf), "%.3f", q);
				xmlNewProp(contact_node, BAD_CAST "q", BAD_CAST buf);
			}

			/* URI-Node */
			buf_len = snprintf(buf, sizeof(buf), "%.*s", ptr->c.len, ptr->c.s);
			uri_node = xmlNewChild(contact_node, NULL, BAD_CAST "uri", BAD_CAST buf) ;
			if(uri_node == NULL) {
				LM_ERR("while adding child\n");
				goto error;
			}
		}
		ptr = ptr->next;
	}

	/* create the body */
	body = (str*)pkg_malloc(sizeof(str));
	if(body == NULL) {
		LM_ERR("while allocating memory\n");
		return NULL;
	}
	memset(body, 0, sizeof(str));

	/* Write the XML into the body */
	xmlDocDumpFormatMemory(doc,(unsigned char**)(void*)&body->s,&body->len,1);

	/*free the document */
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return body;
error:
	if(body) {
		if(body->s) xmlFree(body->s);
		pkg_free(body);
	}
	if(doc) xmlFreeDoc(doc);
	return NULL;
}	

void reginfo_usrloc_cb(ucontact_t* c, int type, void* param) {
	str* body= NULL;
	publ_info_t* publ= NULL;
	int size= 0;
	str content_type;
	udomain_t * domain;
	urecord_t * record;
	int res;
	str uri = {NULL, 0};
	char* at = NULL;
	struct sip_uri remote_uri;

	content_type.s = "application/reginfo+xml";
	content_type.len = 23;
	
	/* Debug Output: */
	LM_DBG("AOR: %.*s (%.*s)\n", c->aor->len, c->aor->s, c->domain->len, c->domain->s);
	if(type & UL_CONTACT_INSERT) LM_DBG("type= UL_CONTACT_INSERT\n");
	else if(type & UL_CONTACT_UPDATE) LM_DBG("type= UL_CONTACT_UPDATE\n");
	else if(type & UL_CONTACT_EXPIRE) LM_DBG("type= UL_CONTACT_EXPIRE\n");
	else if(type & UL_CONTACT_DELETE) LM_DBG("type= UL_CONTACT_DELETE\n");
	else LM_DBG("Unknown Type %i\n", type);

	/* Get the UDomain for this account */
	ul.get_udomain(c->domain->s, &domain);
	/* Get the URecord for this AOR */
	res = ul.get_urecord(domain, c->aor, &record);
	if (res > 0) {
		LM_ERR("' %.*s (%.*s)' Not found in usrloc\n", c->aor->len, c->aor->s, c->domain->len, c->domain->s);
		return;
	}

	/* Create AOR to be published */
	/* Search for @ in the AOR. In case no domain was provided, we will add the "default domain" */
	at = memchr(record->aor.s, '@', record->aor.len);
	if (!at) {
		uri.len = record->aor.len + default_domain.len + 6;
		uri.s = (char*)pkg_malloc(sizeof(char) * uri.len);
		if(uri.s == NULL) {
			LM_ERR("Error allocating memory for URI!\n");
			goto error;
		}
		uri.len = snprintf(uri.s, uri.len, "sip:%.*s@%.*s", record->aor.len, record->aor.s, default_domain.len, default_domain.s);
	} else {
		uri.len = record->aor.len + 6;
		uri.s = (char*)pkg_malloc(sizeof(char) * uri.len);
		if(uri.s == NULL) {
			LM_ERR("Error allocating memory for URI!\n");
			goto error;
		}
		uri.len = snprintf(uri.s, uri.len, "sip:%.*s", record->aor.len, record->aor.s);
	}

	/* send PUBLISH only if the receiver (entity, RURI) is local*/
	if (parse_uri(uri.s, uri.len, &remote_uri) < 0) {
		LM_ERR("failed to parse the URI %.*s\n", uri.len, uri.s);
		return;
	}
	if (!check_self(&(remote_uri.host), 0, 0)) {
		LM_DBG("do not send PUBLISH to external URI %.*s\n", uri.len, uri.s);
		return;
	}
	
	/* Build the XML-Body: */
	body = build_reginfo_full(record, uri);

	if(body == NULL || body->s == NULL) {
		LM_ERR("Error on creating XML-Body for publish\n");
		goto error;
	}
	LM_DBG("XML-Body:\n%.*s\n", body->len, body->s);

	size = sizeof(publ_info_t)
	 + sizeof(str) 		/* pres_uri */
	 + sizeof(str) 		/* Body-Str */
	 + (body->len		/* The body of the message */
	 + 18 + c->aor->len 	/* AOR */
	 + c->domain->len	/* Domain */
	 + uri.len 		/* pres_uri->s */
         + content_type.len	/* content_type.s */
	) * sizeof(char); 

	publ= (publ_info_t*)pkg_malloc(size);
	if(publ== NULL) {
		LM_ERR("no more memory\n");
		goto error;
	}
	memset(publ, 0, size);
	size = sizeof(publ_info_t);

	publ->pres_uri = (str*)((char*)publ + size);
	size += sizeof(str);
	publ->pres_uri->s = (char*)publ+ size;
	memcpy(publ->pres_uri->s, uri.s, uri.len);
	publ->pres_uri->len= uri.len;
	size += uri.len;

	publ->body = (str*)((char*)publ + size);
	size += sizeof(str);

	publ->body->s= (char*)publ + size;
	memcpy(publ->body->s, body->s, body->len);
	publ->body->len= body->len;
	size+= body->len;

	publ->id.s= (char*)publ + size;
	memcpy(publ->id.s, "REGINFO_PUBLISH.", 16);
	memcpy(publ->id.s+16, c->aor->s, c->aor->len);
	memcpy(publ->id.s+16+c->aor->len, "@", 1);
	memcpy(publ->id.s+16+c->aor->len+1, c->domain->s, c->domain->len);
	publ->id.len = 16 + c->aor->len + 1 + c->domain->len;
	size += publ->id.len;

	publ->content_type.s = (char*)publ + size;
	memcpy(publ->content_type.s, content_type.s, content_type.len);
	publ->content_type.len = content_type.len;
	size += content_type.len;
	publ->expires = 3600;
	
	/* make UPDATE_TYPE, as if this "publish dialog" is not found 
	   by pua it will fallback to INSERT_TYPE anyway */
	publ->flag|= UPDATE_TYPE;

	publ->source_flag |= REGINFO_PUBLISH;
	publ->event |= REGINFO_EVENT;
	publ->extra_headers= NULL;
	if(pua.send_publish(publ) < 0) {
		LM_ERR("Error while sending publish\n");
	}	
error:
	if(publ) pkg_free(publ);
	if (uri.s) pkg_free(uri.s);
	if(body) {
		if(body->s) xmlFree(body->s);
		pkg_free(body);
	}

	return;
}	
