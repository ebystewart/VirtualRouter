#ifndef __WHEEL_TIMER_H__
#define __WHEEL_TIMER_H__

#include <stdint.h>
#include <pthread.h>
#include "../glueThread/glthread.h"

typedef struct _wheel_timer_elem_t wheel_timer_elem_t;
typedef void (*app_call_back)(void *arg, uint32_t sizeof_arg);
typedef struct _wheel_timer_t wheel_timer_t;

typedef struct slotlist_{
    glthread_t slots;
    pthread_mutex_t slot_mutex;
}slotlist_t;

typedef enum{
    WTELEM_CREATE,
    WTELEM_RESCHED,
    WTELEM_DELETE,
    WTELEM_SCHEDULED,
    WTELEM_UNKNOWN
}wt_opcode_t;

struct _wheel_timer_elem_t{
    wt_opcode_t opcode;
    int time_interval;
    int new_time_interval;
    int execute_cycle_no;
    int slot_no;
    app_call_back app_callback;
    void *arg;
    int arg_size;
    char is_recurrence;
    glthread_t glue;
    slotlist_t slotlist_head;
    glthread_t reschedule_glue;
    unsigned int N_scheduled;
    wheel_timer_t *wt;
};

GLTHREAD_TO_STRUCT(glthread_to_wt_elem, wheel_timer_elem_t, glue);
GLTHREAD_TO_STRUCT(glthread_reschedule_glue_to_wt_elem, wheel_timer_elem_t, reschedule_glue);

struct _wheel_timer_t {
	int current_clock_tic;
	int clock_tic_interval;
	int wheel_size;
	int current_cycle_no;
	pthread_t wheel_thread;
    slotlist_t reschd_list;
    unsigned int no_of_wt_elem;
    slotlist_t slotlist[0];
};

/* Gives the absolute slot number since the time ET has started */
#define GET_WT_CURRENT_ABS_SLOT_NO(wt) ((wt->current_cycle_no * wt->wheel_size) + wt->current_clock_tic)

#define WT_UPTIME(wt_ptr) (GET_WT_CURRENT_ABS_SLOT_NO(wt_ptr) * wt_ptr->clock_tic_interval)

#define WT_SLOTLIST(wt_ptr, index) (&(wt_ptr->slotlist[index])) 

#endif