#includes "actions.h"

void send_poll (void) 
{
	data.timer = portGetTickCount() + data.txTimeouts; //set timeout time
	data.timer_en = 1; //start timer
	data.stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	previous_state = current_state;
	current_state = TX_POLL_STATE; /* set new state, if necessary */
}

void sleep_mode (void) 
{
	if (current_state == TX_POLL_STATE){
		//Set Timer
		data.timer = portGetTickCount() + data.tagWaitTime_ms; //set timeout time
		data.timer_en = 1; //start timer
		data.stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
		previous_state = current_state;
		current_state = WAIT_STATE;
	}else{ // TX_FINAL_STATE
		//Set Timer
		data.timer = portGetTickCount() + data.tagSleepTime_ms; //set timeout time
		data.timer_en = 1; //start timer
		data.stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
		previous_state = current_state;
		current_state = INIT_STATE;
	}
	
}
void rx_mode (void) 
{
	data.timer = portGetTickCount() + data.rxTimeouts; //set timeout time
	data.timer_en = 1; //start timer
	data.stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	//Put DW1000 in rx
	previous_state = current_state;
	current_state = RX_RESP_WAIT;
}
void send_final (void)
{
	
	//Set Timer
	data.timer = portGetTickCount() + data.txTimeouts; //set timeout time
	data.timer_en = 1; //start timer
	data.stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	previous_state = current_state;
	current_state = TX_FINAL_STATE;
}
void ignore_event (void) {
	// Log Event and don't change state 
	
	//Put timer ??

}

void rx_fail(void) 
{
	//Log the problem and depending on it change STATE APPROPRIATLY
	
	data.timer = portGetTickCount() + data.tagSleepTime_ms; //set timeout time
	data.timer_en = 1; //start timer
	data.stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	previous_state = current_state;
	current_state = INIT_STATE;
}
