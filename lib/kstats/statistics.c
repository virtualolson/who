/*
 * $Id: statistics.c 4518 2008-07-28 15:39:28Z henningw $
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2006-01-16  first version (bogdan)
 *  2006-11-28  added get_stat_var_from_num_code() (Jeffrey Magder -
 *              SOMA Networks)
 */

/*!
 * \file
 * \brief Statistics support
 */


#include <string.h>

#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../locking.h"
#include "../../hash_func.h"
#include "statistics.h"

#ifdef STATISTICS

static stats_collector *collector;


#ifdef NO_ATOMIC_OPS
#warning STATISTICS: Architecture with no support for atomic operations. \
         Using Locks!!
gen_lock_t *stat_lock = 0;
#endif

#define stat_hash(_s) (get_hash1_raw((_s)->s,(_s)->len) & (STATS_HASH_SIZE-1))



/*! \brief
 * Returns the statistic associated with 'numerical_code' and 'out_codes'.
 * Specifically:
 *
 *  - if out_codes is nonzero, then the stat_var for the number of messages 
 *    _sent out_ with the 'numerical_code' will be returned if it exists.
 *  - otherwise, the stat_var for the number of messages _received_ with the 
 *    'numerical_code' will be returned, if the stat exists. 
 */
stat_var *get_stat_var_from_num_code(unsigned int numerical_code, int out_codes)
{
	static char msg_code[INT2STR_MAX_LEN+4];
	str stat_name;

	stat_name.s = int2bstr( (unsigned long)numerical_code, msg_code, 
		&stat_name.len);
	stat_name.s[stat_name.len++] = '_';

	if (out_codes) {
		stat_name.s[stat_name.len++] = 'o';
		stat_name.s[stat_name.len++] = 'u';
		stat_name.s[stat_name.len++] = 't';
	} else {
		stat_name.s[stat_name.len++] = 'i';
		stat_name.s[stat_name.len++] = 'n';
	}

	return get_stat(&stat_name);
}



int init_stats_collector(void)
{
	/* init the collector */
	collector = (stats_collector*)shm_malloc(sizeof(stats_collector));
	if (collector==0) {
		LM_ERR("no more shm mem\n");
		goto error;
	}
	memset( collector, 0 , sizeof(stats_collector));

#ifdef NO_ATOMIC_OPS
	/* init BIG (really BIG) lock */
	stat_lock = lock_alloc();
	if (stat_lock==0 || lock_init( stat_lock )==0 ) {
		LM_ERR("failed to init the really BIG lock\n");
		goto error;
	}
#endif

	LM_DBG("statistics manager successfully initialized\n");

	return 0;
error:
	return -1;
}


void destroy_stats_collector(void)
{
	stat_var *stat;
	stat_var *tmp_stat;
	int i;

#ifdef NO_ATOMIC_OPS
	/* destroy big lock */
	if (stat_lock)
		lock_destroy( stat_lock );
#endif

	if (collector) {
		/* destroy hash table */
		for( i=0 ; i<STATS_HASH_SIZE ; i++ ) {
			for( stat=collector->hstats[i] ; stat ; ) {
				tmp_stat = stat;
				stat = stat->hnext;
				if ((tmp_stat->flags&STAT_IS_FUNC)==0 && tmp_stat->u.val)
					shm_free(tmp_stat->u.val);
				if ( (tmp_stat->flags&STAT_SHM_NAME) && tmp_stat->name.s)
					shm_free(tmp_stat->name.s);
				shm_free(tmp_stat);
			}
		}

		/* destroy sts_module array */
		if (collector->amodules)
			shm_free(collector->amodules);

		/* destroy the collector */
		shm_free(collector);
	}

	return;
}


static inline module_stats* get_stat_module( str *module)
{
	int i;

	if ( (module==0) || module->s==0 || module->len==0 )
		return 0;

	for( i=0 ; i<collector->mod_no ; i++ ) {
		if ( (collector->amodules[i].name.len == module->len) &&
		(strncasecmp(collector->amodules[i].name.s,module->s,module->len)==0) )
			return &collector->amodules[i];
	}

	return 0;
}


static inline module_stats* add_stat_module( char *module)
{
	module_stats *amods;
	module_stats *mods;
	int len;

	if ( (module==0) || ((len = strlen(module))==0 ) )
		return 0;

	amods = (module_stats*)shm_realloc( collector->amodules,
			(collector->mod_no+1)*sizeof(module_stats) );
	if (amods==0) {
		LM_ERR("no more shm memory\n");
		return 0;
	}

	collector->amodules = amods;
	collector->mod_no++;

	mods = &amods[collector->mod_no-1];
	memset( mods, 0, sizeof(module_stats) );

	mods->name.s = module;
	mods->name.len = len;

	return mods;
}


int register_stat( char *module, char *name, stat_var **pvar, int flags)
{
	module_stats* mods;
	stat_var *stat;
	stat_var *it;
	str smodule;
	int hash;

	if (module==0 || name==0 || pvar==0) {
		LM_ERR("invalid parameters module=%p, name=%p, pvar=%p \n", 
				module, name, pvar);
		goto error;
	}

	stat = (stat_var*)shm_malloc(sizeof(stat_var));
	if (stat==0) {
		LM_ERR("no more shm memory\n");
		goto error;
	}
	memset( stat, 0, sizeof(stat_var));

	if ( (flags&STAT_IS_FUNC)==0 ) {
		stat->u.val = (stat_val*)shm_malloc(sizeof(stat_val));
		if (stat->u.val==0) {
			LM_ERR("no more shm memory\n");
			goto error1;
		}
#ifdef NO_ATOMIC_OPS
		*(stat->u.val) = 0;
#else
		atomic_set(stat->u.val,0);
#endif
		*pvar = stat;
	} else {
		stat->u.f = (stat_function)(pvar);
	}

	/* is the module already recorded? */
	smodule.s = module;
	smodule.len = strlen(module);
	mods = get_stat_module(&smodule);
	if (mods==0) {
		mods = add_stat_module(module);
		if (mods==0) {
			LM_ERR("failed to add new module\n");
			goto error2;
		}
	}

	/* fill the stat record */
	stat->mod_idx = collector->mod_no-1;

	stat->name.s = name;
	stat->name.len = strlen(name);
	stat->flags = flags;


	/* compute the hash by name */
	hash = stat_hash( &stat->name );

	/* link it */
	if (collector->hstats[hash]==0) {
		collector->hstats[hash] = stat;
	} else {
		it = collector->hstats[hash];
		while(it->hnext)
			it = it->hnext;
		it->hnext = stat;
	}
	collector->stats_no++;

	/* add the statistic also to the module statistic list */
	if (mods->tail) {
		mods->tail->lnext = stat;
	} else {
		mods->head = stat;
	}
	mods->tail = stat;
	mods->no++;

	return 0;
error2:
	if ( (flags&STAT_IS_FUNC)==0 ) {
		shm_free(*pvar);
		*pvar = 0;
	}
error1:
	shm_free(stat);
error:
	*pvar = 0;
	return -1;
}



int register_module_stats(char *module, stat_export_t *stats)
{
	int ret;

	if (module==0 || module[0]==0 || !stats || !stats[0].name)
		return 0;

	for( ; stats->name ; stats++) {
		ret = register_stat( module, stats->name, stats->stat_pointer,
			stats->flags);
		if (ret!=0) {
			LM_CRIT("failed to add statistic\n");
			return -1;
		}
	}

	return 0;
}



stat_var* get_stat( str *name )
{
	stat_var *stat;
	int hash;

	if (name==0 || name->s==0 || name->len==0)
		return 0;

	/* compute the hash by name */
	hash = stat_hash( name );

	/* and look for it */
	for( stat=collector->hstats[hash] ; stat ; stat=stat->hnext ) {
		if ( (stat->name.len==name->len) &&
		(strncasecmp( stat->name.s, name->s, name->len)==0) )
			return stat;
	}

	return 0;
}

#endif /*STATISTICS*/

