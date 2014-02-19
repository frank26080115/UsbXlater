#include <btstack/run_loop.h>
#include <btstack/linked_list.h>
#include <btstack/hal_tick.h>
#include <btstack/hal_cpu.h>

#include <btstack/src/run_loop_private.h>
#include <btstack/src/hci.h>
#include <btstack/src/l2cap.h>
#include <btstack/btproxy_sdp.h>
#include <btstack-debug.h>

#include <stddef.h> // NULL
#include <btproxy.h>

// the run loop
static linked_list_t data_sources;

static linked_list_t timers;

static int trigger_event_received = 0;

#ifdef HAVE_TICK
static void btproxy_runloop_tick_handler(void){
    trigger_event_received = 1;
}

uint32_t btproxy_runloop_get_ticks(void){
    return systick_1ms_cnt;
}

uint32_t btproxy_runloop_ticks_for_ms(uint32_t time_in_ms){
    return time_in_ms / hal_tick_get_tick_period_in_ms();
}
#endif

/**
 * Add data_source to run_loop
 */
static void btproxy_runloop_add_data_source(data_source_t *ds){
    linked_list_add(&data_sources, (linked_item_t *) ds);
}

/**
 * Remove data_source from run loop
 */
static int btproxy_runloop_remove_data_source(data_source_t *ds){
    return linked_list_remove(&data_sources, (linked_item_t *) ds);
}

// set timer
static void btproxy_runloop_set_timer(timer_source_t *ts, uint32_t timeout_in_ms){
    uint32_t ticks = btproxy_runloop_ticks_for_ms(timeout_in_ms);
    if (ticks == 0) ticks++;
    ts->timeout = systick_1ms_cnt + ticks; 
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btproxy_runloop_add_timer(timer_source_t *ts){
#ifdef HAVE_TICK
    linked_item_t *it;
    for (it = (linked_item_t *) &timers; it->next ; it = it->next){
        if (ts->timeout < ((timer_source_t *) it->next)->timeout) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (linked_item_t *) ts;
    // log_info("Added timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    // btproxy_runloop_dump_timer();
#endif
}

/**
 * Remove timer from run loop
 */
static int btproxy_runloop_remove_timer(timer_source_t *ts){
#ifdef HAVE_TICK    
    // log_info("Removed timer %x at %u\n", (int) ts, (unsigned int) ts->timeout.tv_sec);
    return linked_list_remove(&timers, (linked_item_t *) ts);
#else
    return 0;
#endif
}

static void btproxy_runloop_dump_timer(void){
#ifdef HAVE_TICK
#ifdef ENABLE_LOG_INFO 
    linked_item_t *it;
    int i = 0;
    for (it = (linked_item_t *) timers; it ; it = it->next){
        timer_source_t *ts = (timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, (unsigned int) ts->timeout);
    }
#endif
#endif
}

/**
 * Execute run_loop once
 */
void btproxy_runloop_execute_once(void) {
    hci_run();
    l2cap_run();
    bpsdp_run();

    data_source_t *ds;

    // process data sources
    data_source_t *next;
    for (ds = (data_source_t *) data_sources; ds != NULL ; ds = next){
        next = (data_source_t *) ds->item.next; // cache pointer to next data_source to allow data source to remove itself
        ds->process(ds);
    }

#ifdef HAVE_TICK
    // process timers
    while (timers) {
        timer_source_t *ts = (timer_source_t *) timers;
        if (ts->timeout > systick_1ms_cnt) break;
        run_loop_remove_timer(ts);
        ts->process(ts);
    }
#endif
}

/**
 * trigger run loop iteration
 */
void btproxy_runloop_trigger(void){
    trigger_event_received = 1;
}

static void btproxy_runloop_execute(void){
	while (1) btproxy_runloop_execute_once();
}

static void btproxy_runloop_init(void){

    data_sources = NULL;

#ifdef HAVE_TICK
    timers = NULL;
    //systick_1ms_cnt = 0;
    hal_tick_init();
    hal_tick_set_handler(&btproxy_runloop_tick_handler);
#endif
}

const run_loop_t run_loop_btproxy = {
    &btproxy_runloop_init,
    &btproxy_runloop_add_data_source,
    &btproxy_runloop_remove_data_source,
    &btproxy_runloop_set_timer,
    &btproxy_runloop_add_timer,
    &btproxy_runloop_remove_timer,
    &btproxy_runloop_execute,
    &btproxy_runloop_dump_timer,
};
