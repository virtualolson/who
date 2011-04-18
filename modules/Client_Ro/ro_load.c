/* 
 * File:   ro_load.c
 * Author: Jason Penton
 *
 * Created on 06 April 2011, 8:55 AM
 */
#include "ro_load.h"

int load_ro(struct ro_binds *ro_bind) {
    memset(ro_bind, 0, sizeof (struct ro_binds));

    /* exported to cfg */
    if (!(ro_bind->send_ccr_request = (ro_send_ccr_request_f) find_export("RO_Send_CCR", 0, 0))) {
        LM_ERR("'RO_Send_CCR' not found\n");
        return -1;
    }
    return 1;
}

