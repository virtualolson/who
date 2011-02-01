/**
 * $Id$
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "util_file.h"
#include <stdio.h>

str file_path =	str_init(DEFAULT_FILE_PATH);

int pv_parse_filename(pv_spec_p sp, str *in)
{
	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
	sp->pvp.pvn.u.isname.name.s = *in;

	return 0;
}

int pv_get_file(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str value;
	char path[512];
	int path_len;
	FILE *file_in=NULL;
	long size;

	if (param==NULL || param->pvn.type!=PV_NAME_INTSTR || param->pvn.u.isname.type!=AVP_NAME_STR || param->pvn.u.isname.name.s.s==NULL) {
		LM_CRIT("BUG - bad parameters\n");
		return -1;
	}

	/* Create the filename */
	if (param->pvn.u.isname.name.s.s[0] == '/') {
		path_len = snprintf(path, 511, "%.*s", param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);
	} else {
		if (file_path.s[file_path.len-1] == '/')
			path_len = snprintf(path, 511, "%.*s%.*s", file_path.len, file_path.s,
				param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);
		else
			path_len = snprintf(path, 511, "%.*s/%.*s", file_path.len, file_path.s,
				param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);
	}
	LM_DBG("Requesting file %.*s\n", path_len, path);
	
	/* Open the filename for reading: */
	file_in = fopen(path, "rb");
	if(!file_in) {
		LM_WARN("Unable to open file %.*s!\n", path_len, path);
		return -1;
	}	
	
	/* Get the filesze */
	fseek (file_in, 0, SEEK_END);
	size = ftell(file_in);
	rewind (file_in);

	/* Allocate memory for the destination: */
	value.s = shm_malloc(size);
	if (value.s == NULL)
		return -1;

	LM_DBG("size is %li\n", size);

	/* Read from the file: */
	value.len = fread(value.s, 1, size, file_in);
	if (value.len != size) {
		LM_WARN("Error reading file!\n");
		shm_free(value.s);
		value.s = 0;
	}
	
	/* Close the file */
	fclose(file_in);

	if (value.s)
		return pv_get_strval(msg, param, res, &value);

	return 0;
}

int pv_set_file(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val)
{
	char path[512];
	int path_len;
	FILE *file_out=NULL;
	long size;

	if (param==NULL || param->pvn.type!=PV_NAME_INTSTR || param->pvn.u.isname.type!=AVP_NAME_STR || param->pvn.u.isname.name.s.s==NULL ) {
		LM_CRIT("BUG - bad parameters\n");
		return -1;
	}

	/* if value, must be string */
	if ( !(val->flags&PV_VAL_STR)) {
		LM_ERR("non-string values are not supported\n");
		return -1;
	}

	/* Create the filename: */
	if (param->pvn.u.isname.name.s.s[0] == '/') {
		path_len = snprintf(path, 511, "%.*s", param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);
	} else {
		if (file_path.s[file_path.len-1] == '/')
			path_len = snprintf(path, 511, "%.*s%.*s", file_path.len, file_path.s,
				param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);
		else
			path_len = snprintf(path, 511, "%.*s/%.*s", file_path.len, file_path.s,
				param->pvn.u.isname.name.s.len, param->pvn.u.isname.name.s.s);
	}
	LM_DBG("Requesting file %.*s\n", path_len, path);

	/* Open file for reading */
	file_out = fopen (path, "wb");
	if(!file_out) {
		LM_WARN("Unable to open file %.*s!\n", path_len, path);
		fclose(file_out);
		return -1;
	}

	/* Write the contents to the file */
	size = fwrite(val->rs.s, 1, val->rs.len, file_out);
	if (size != val->rs.len) {
		LM_WARN("Unable to write into file %.*s!\n", path_len, path);
		fclose(file_out);
		return -1;
	}
	
	/* Close the file */
	fclose (file_out);

	return 0;
}

