#include "timer.h"
#include "em_timer.h"

static char _isInit = false;
volatile uint32_t time_since_start;  // time in milliseconds


/**
 * increment the amounts of ticks by 1
 */
void SysTick_Handler(void) {
    time_since_start++;
}


/**
 * initialize the timer of the system
 */
void our_timer_init() {
    if (!_isInit) {
        _isInit = true;
        sl_sleeptimer_init();
        CMU_ClockEnable(cmuClock_RTCC, true);
        SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000);
    }
}


/**
 * @return: the current time passed in milliseconds since timer_init
 */
uint32_t cur_time() {
  return time_since_start;
}


/**
 * callback function to increase the timout_counter
 */
static void timer_handler(sl_sleeptimer_timer_handle_t *handle, void *data) {
    (void)&handle;
    (*((int*) data))++;
}


/**
 * function that gets a timer object and runs it with timeout_ms and timeout_counter to be updated each time
 * we get a time out.
 * use sl_sleeptimer_stop_timer(sl_sleeptimer_timer_handle_t *handle) to stop the timer.
 * @param handle: s1_speetimer type
 * @param timeout_ms: uint32_t type
 * @param timeout_counter: pointer to int that will be updated each timeout
 * @return
 */
int set_periodic_timer(sl_sleeptimer_timer_handle_t *handle, uint32_t timeout_ms, int* timeout_counter) {
  uint32_t timeout;
  if (sl_sleeptimer_ms32_to_tick(timeout_ms, &timeout) != SL_STATUS_OK) return -1;
  if (sl_sleeptimer_start_periodic_timer(
      handle, timeout, timer_handler, timeout_counter, 0, SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG
      ) != SL_STATUS_OK) return -1;
  return 0;
}


