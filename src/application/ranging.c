#include "ranging.h"
#include "deca_types.h"

enum events;

void send_poll (data_t *data) 
{
	//Prepare message
	data->msg.seqNum = data->frame_sn++;
	data->msg.panID[0] = (data->panid) & 0xff;
    data->msg.panID[1] = data->panid >> 8;
    
	data->msg.fctCode = SDSTWR_FCT_CODE_POLL;
	
	//No late information as it's a poll
	data->psduLength = HEADER_LENGTH + MESSAGE_DATA_POLL + CRC_LENGTH;

	dwt_writetxfctrl(data->psduLength, 0);

	//if the response is expected there is a 1ms timeout to stop RX if no response (ACK or other frame) coming
	dwt_setrxtimeout(data->fwtoTime_sy);  //units are us - wait for 215us after RX on

	//use delayed rx on (wait4resp timer)
	//dwt_setrxaftertxdelay(inst->fixedReplyDelay_sy);  //units are ~us - wait for wait4respTIM before RX on (delay RX)

	dwt_writetxdata(data->psduLength, (uint8 *)  &data->msg, 0) ;   // write the poll frame data

	//start TX of frame
	dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
	             
	//this is platform dependent - only program if DW EVK/EVB
	dwt_setleds(1);

	//MP bug - TX antenna delay needs reprogramming as it is not preserved
	dwt_settxantennadelay(data->txantennaDelay) ;
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
	dwt_setrxtimeout(data->rxTimeouts);
	//Put DW1000 in rx
	dwt_rxenable(0);

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


void config_data(data_t *data)
{
    int use_otpdata = DWT_LOADANTDLY | DWT_LOADXTALTRIM;
    uint32 power = 0;

    data->configData.chan = 5 ;
    data->configData.rxCode =  9;
    data->configData.txCode = 9 ;
    data->configData.prf = DWT_PRF_64M ;
    data->configData.dataRate = DWT_BR_6M8 ;
    data->configData.txPreambLength = DWT_PLEN_128 ;
    data->configData.rxPAC = DWT_PAC8 ;
    data->configData.nsSFD = 0 ;
    data->configData.phrMode = DWT_PHRMODE_STD ;
    data->configData.sfdTO = (129 + 8 - 8);

    //enable gating gain for 6.81Mbps data rate
    if(data->configData.dataRate == DWT_BR_6M8)
        data->configData.smartPowerEn = 1;
    else
        data->configData.smartPowerEn = 0;

    //configure the channel parameters
    dwt_configure(&data->configData, use_otpdata) ;

    data->configTX.PGdly = txSpectrumConfig[data->configData.chan].PGdelay ;

    //firstly check if there are calibrated TX power value in the DW1000 OTP
    power = dwt_getotptxpower(data->configData.prf, data->configData.chan);

    if((power == 0x0) || (power == 0xFFFFFFFF)) //if there are no calibrated values... need to use defaults
    {
        power = txSpectrumConfig[data->configData.chan].txPwr[data->configData.prf- DWT_PRF_16M];
    }

    //Configure TX power
    //if smart power is used then the value as read from OTP is used directly
    //if smart power is used the user needs to make sure to transmit only one frame per 1ms or TX spectrum power will be violated
    if(data->configData.smartPowerEn == 1)
    {
        data->configTX.power = power;
    }
	else //if the smart power is not used, then the low byte value (repeated) is used for the whole TX power register
    {
        uint8 pow = power & 0xFF ;
        data->configTX.power = (pow | (pow << 8) | (pow << 16) | (pow << 24));
    }
    dwt_setsmarttxpower(data->configData.smartPowerEn);

    //configure the tx spectrum parameters (power and PG delay)
    dwt_configuretxrf(&data->configTX);

    //check if to use the antenna delay calibration values as read from the OTP
    if((use_otpdata & DWT_LOADANTDLY) == 0)
    {
        data->txantennaDelay = rfDelays[data->configData.prf - DWT_PRF_16M];
        // -------------------------------------------------------------------------------------------------------------------
        // set the antenna delay, we assume that the RX is the same as TX.
        dwt_setrxantennadelay(data->txantennaDelay);
        dwt_settxantennadelay(data->txantennaDelay);
    }
    else
    {
        //get the antenna delay that was read from the OTP calibration area
        data->txantennaDelay = dwt_readantennadelay(data->configData.prf) >> 1;

        // if nothing was actually programmed then set a reasonable value anyway
        if (data->txantennaDelay == 0)
        {
            data->txantennaDelay = rfDelays[data->configData.prf - DWT_PRF_16M];
            // -------------------------------------------------------------------------------------------------------------------
            // set the antenna delay, we assume that the RX is the same as TX.
            dwt_setrxantennadelay(data->txantennaDelay);
            dwt_settxantennadelay(data->txantennaDelay);
        }


    }

    if(data->configData.txPreambLength == DWT_PLEN_64) //if preamble length is 64
	{
    	SPI_ConfigFastRate(SPI_BaudRatePrescaler_16); //reduce SPI to < 3MHz

		dwt_loadopsettabfromotp(0);

		SPI_ConfigFastRate(SPI_BaudRatePrescaler_4); //increase SPI to max
    }

	//config is needed before reply delays are configured

}

// convert microseconds to device time
uint64 convertmicrosectodevicetimeu (double microsecu)
{
    uint64 dt;
    long double dtime;

    dtime = (microsecu / (double) DWT_TIME_UNITS) / 1e6 ;

    dt =  (uint64) (dtime) ;

    return dt;
}

// -------------------------------------------------------------------------------------------------------------------
// convert microseconds to device time
uint32 convertmicrosectodevicetimeu32 (double microsecu)
{
    uint32 dt;
    long double dtime;

    dtime = (microsecu / (double) DWT_TIME_UNITS) / 1e6 ;

    dt =  (uint32) (dtime) ;

    return dt;
}


double convertdevicetimetosec(int32 dt)
{
    double f = 0;

    f =  dt * DWT_TIME_UNITS ;  // seconds #define TIME_UNITS          (1.0/499.2e6/128.0) = 15.65e-12

    return f ;
}

double convertdevicetimetosec8(uint8* dt)
{
    double f = 0;

    uint32 lo = 0;
    int8 hi = 0;

    memcpy(&lo, dt, 4);
    hi = dt[4] ;

    f = ((hi * 65536.00 * 65536.00) + lo) * DWT_TIME_UNITS ;  // seconds #define TIME_UNITS          (1.0/499.2e6/128.0) = 15.65e-12

    return f ;
}
