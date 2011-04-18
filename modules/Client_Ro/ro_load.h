/* 
 * File:   ro_load.h
 * Author: Jason Penton
 *
 * Created on 06 April 2011, 8:48 AM
 */

#ifndef RO_LOAD_H
#define	RO_LOAD_H

#include "../../sr_module.h"

#define NO_SCRIPT	-1


typedef int (*ro_send_ccr_request_f)(unsigned int);

struct ro_binds {
    ro_send_ccr_request_f send_ccr_request;
};

typedef int(*load_ro_f)(struct ro_binds *ro_bind);

int load_ro(struct ro_binds *ro_bind);

static inline int load_ro_api(struct ro_binds* ro_bind) {
    load_ro_f load_ro;

    /* import the TM auto-loading function */
    load_ro = (load_ro_f) find_export("load_ro", NO_SCRIPT, 0);

    if (load_ro == NULL) {
        LM_WARN("Cannot import load_tm function from tm module\n");
        return -1;
    }

    /* let the auto-loading function load all TM stuff */
    if (load_ro(ro_bind) == -1) {
        return -1;
    }
    return 0;
}

#endif	/* RO_LOAD_H */

