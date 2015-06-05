enum states { INIT_STATE, TX_POLL_STATE, WAIT_STATE, RX_RESP_WAIT, TX_FINAL_STATE, MAX_STATES } STATES;

void (*const state_table [MAX_STATES][MAX_EVENTS]) (void) = {

    { ignore_event, ignore_event, ignore_event, ignore_event, ignore_event, send_poll }, /* procedures for state INIT_STATE */
    { ignore_event, ignore_event, sleep_mode, ignore_event, ignore_event, send_poll }, /* procedures for state TX_POLL_STATE */
    { ignore_event, ignore_event, ignore_event, ignore_event, ignore_event, rx_mode }  /* procedures for state WAIT_STATE */
    { ignore_event, send_final, ignore_event, rx_fail, rx_fail, ignore_event }, /* procedures for state RX_RESP_WAIT */
    { ignore_event, ignore_event, sleep_mode, ignore_event, ignore_event, sleep_mode }  /* procedures for state TX_FINAL_STATE */
};
