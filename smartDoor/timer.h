#ifndef TIMER_H_
#define TIMER_H_
#include "em_cmu.h"
#include "sl_sleeptimer.h"

/**
 * initialize the timer of the system
 */
void our_timer_init();

/**
 * return the current time passed in milliseconds since program starting.
 */
uint32_t cur_time();

/**
 * function that gets a timer object and runs it with timeout_ms and timeout_counter to be updated each time
 * we get a time out.
 * use sl_sleeptimer_stop_timer(sl_sleeptimer_timer_handle_t *handle) to stop the timer.
 * @param handle: s1_speetimer type
 * @param timeout_ms: uint32_t type
 * @param timeout_counter: pointer to int that will be updated each timeout
 * @return
 */
int set_periodic_timer(sl_sleeptimer_timer_handle_t *handle, uint32_t timeout_ms, int* timeout_counter);

#endif /* TIMER_H_ */
