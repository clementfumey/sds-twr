#include "ranging.h"
#include "deca_types.h"

enum events;

void send_poll (data_t *data) 
{
	//Prepare message
	 //NOTE the anchor address is set after receiving the ranging initialisation message
	inst->instToSleep = 1; //we'll sleep after this ranging exchange (i.e. before sending the next poll)

	inst->msg_f.messageData[FCODE] = RTLS_DEMO_MSG_TAG_POLLF;

	inst->psduLength = TAG_POLL_F_MSG_LEN + FRAME_CRTL_AND_ADDRESS_S + FRAME_CRC + EXTRA_LENGTH;

	//set frame type (0-2), SEC (3), Pending (4), ACK (5), PanIDcomp(6)
	inst->msg_f.frameCtrl[0] = 0x41 /*PID comp*/;

	//short address for both
	inst->msg_f.frameCtrl[1] = 0x8 /*dest short address (16bits)*/ | 0x80 /*src short address (16bits)*/;

	inst->msg_f.seqNum = inst->frame_sn++;


	inst->wait4ack = DWT_RESPONSE_EXPECTED; //Response is coming after 275 us...
	//500 -> 485, 800 -> 765
	dwt_writetxfctrl(inst->psduLength, 0);

	//if the response is expected there is a 1ms timeout to stop RX if no response (ACK or other frame) coming
	dwt_setrxtimeout(inst->fwtoTime_sy);  //units are us - wait for 215us after RX on

	//use delayed rx on (wait4resp timer)
	dwt_setrxaftertxdelay(inst->fixedReplyDelay_sy);  //units are ~us - wait for wait4respTIM before RX on (delay RX)

	dwt_writetxdata(inst->psduLength, (uint8 *)  &inst->msg_f, 0) ;   // write the poll frame data

	//start TX of frame
	dwt_starttx(DWT_START_TX_IMMEDIATE | inst->wait4ack);
#if (DW_IDLE_CHK==2)                
	//this is platform dependent - only program if DW EVK/EVB
	dwt_setleds(1);

	//MP bug - TX antenna delay needs reprogramming as it is not preserved
	dwt_settxantennadelay(inst->txantennaDelay) ;
	//Send Message as soon as possible
	
	data->timer = portGetTickCount() + data->txTimeouts; //set timeout time
	data->timer_en = 1; //start timer
	data->stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	data->previous_state = data->current_state;
	data->current_state = TX_POLL_STATE; /* set new state, if necessary */
}

void sleep_mode (data_t *data) 
{
		//Set Timer
		data->timer = portGetTickCount() + data->tagSleepTime_ms; //set timeout time
		data->timer_en = 1; //start timer
		data->stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
		data->previous_state = data->current_state;
		data->current_state = INIT_STATE;
}

void rx_mode (data_t *data) 
{
	dwt_setrxtimeout(time);
	dwt_rxenable();
	
	data->timer = portGetTickCount() + data->rxTimeouts; //set timeout time
	data->timer_en = 1; //start timer
	data->stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	//Put DW1000 in rx
	data->previous_state = data->current_state;
	data->current_state = RX_RESP_WAIT;
}
void send_final (data_t *data)
{
	//We have received a message handle it
	
	// Prepare answer
	
	//send message exactly X time after we received it
	
	//Set Timer
	data->timer = portGetTickCount() + data->txTimeouts; //set timeout time
	data->timer_en = 1; //start timer
	data->stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	data->previous_state = data->current_state;
	data->current_state = TX_FINAL_STATE;
}
void ignore_event (data_t *data) {
	// Log Event and don't change state 
	
	//Put timer ??

}

void rx_fail(data_t *data) 
{
	//Log the problem and depending on it change STATE APPROPRIATLY
	
	data->timer = portGetTickCount() + data->tagSleepTime_ms; //set timeout time
	data->timer_en = 1; //start timer
	data->stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
	data->previous_state = data->current_state;
	data->current_state = INIT_STATE;
}

void putevent(event_data_t newevent, data_t *data)
{
	int instance = 0;
	uint8 etype = newevent.type;

	newevent.type = 0;
	//copy event
	data->dwevent[data->dweventIdxIn] = newevent;

	//set type - this makes it a new event (making sure the event data is copied before event is set as new)
	//to make sure that the get event function does not get an incomplete event
	data->dwevent[data->dweventIdxIn].type = etype;

	data->dweventIdxIn++;

	if(MAX_EVENT_NUMBER == data->dweventIdxIn)
		data->dweventIdxIn = 0;
}

enum events get_new_event (data_t *data)
{
    return data->dwevent[data->dweventPeek].type;
}


event_data_t dw_event_g;

#pragma GCC optimize ("O0")
event_data_t* getevent(data_t *data)
{
	int indexOut = data->dweventIdxOut;

	if(data->dwevent[indexOut].type == 0) //exit with "no event"
	{
		dw_event_g.type = 0;
		dw_event_g.type2 = 0;
		return &dw_event_g;
	}

	//copy the event
	dw_event_g.type2 = data->dwevent[indexOut].type2 ;
	dw_event_g.rxLength = data->dwevent[indexOut].rxLength ;
	dw_event_g.timeStamp = data->dwevent[indexOut].timeStamp ;
	dw_event_g.timeStamp32l = data->dwevent[indexOut].timeStamp32l ;
	dw_event_g.timeStamp32h = data->dwevent[indexOut].timeStamp32h ;
	//dw_event_g.eventtime = instance_data[instance].dwevent[indexOut].eventtime ;
	//dw_event_g.eventtimeclr = instance_data[instance].dwevent[indexOut].eventtimeclr ;
	//dw_event_g.gotit = instance_data[instance].dwevent[indexOut].gotit ;

	memcpy(&dw_event_g.msgu, &data->dwevent[indexOut].msgu, sizeof(data->dwevent[indexOut].msgu));

	dw_event_g.type = data->dwevent[indexOut].type ;


	//instance_data[instance].dwevent[indexOut].gotit = x;

	//instance_data[instance].dwevent[indexOut].eventtimeclr = portGetTickCount();

	data->dwevent[indexOut].type = 0; //clear the event

	data->dweventIdxOut++;
	if(MAX_EVENT_NUMBER == data->dweventIdxOut) //wrap the counter
		data->dweventIdxOut = 0;

	data->dweventPeek = data->dweventIdxOut; //set the new peek value

	//if(dw_event.type) printf("get %d - in %d out %d @ %d\n", dw_event.type, instance_data[instance].dweventCntIn, instance_data[instance].dweventCntOut, ptime);

	//eventOutcount++;


	return &dw_event_g;
}
