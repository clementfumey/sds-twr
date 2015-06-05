
/* Includes */
#include "compiler.h"
#include "port.h"
#include "deca_types.h"
#include "deca_spi.h"
#includes "events.h"
#includes "states.h"
#includes "actions.h"

int instance_anchaddr = 0;
int instance_mode = ANCHOR;
static data_t data ;

void process_dwRSTn_irq(void)
{
    instance_notify_DW1000_inIDLE(1);
}

void process_deca_irq(void)
{
    do{

        instance_process_irq(0);

    }while(port_CheckEXT_IRQ() == 1); //while IRS line active (ARM can only do edge sensitive interrupts)

}

void initLCD(void)
{
    uint8 initseq[9] = { 0x39, 0x14, 0x55, 0x6D, 0x78, 0x38 /*0x3C*/, 0x0C, 0x01, 0x06 };
    uint8 command = 0x0;
    int j = 100000;

    writetoLCD( 9, 0,  initseq); //init seq
    while(j--);

    command = 0x2 ;  //return cursor home
    writetoLCD( 1, 0,  &command);
    command = 0x1 ;  //clear screen
    writetoLCD( 1, 0,  &command);
}

void initDW(void)
{
	int result;
	
    result = dwt_initialise(DWT_LOADUCODE | DWT_LOADLDO | DWT_LOADTXCONFIG | DWT_LOADANTDLY| DWT_LOADXTALTRIM) ;
    //this is platform dependant - only program if DW EVK/EVB
    dwt_setleds(3) ; //configure the GPIOs which control the leds on EVBs

    if (DWT_SUCCESS != result)
    {
        return (-1) ;   // device initialise has failed
    }
    dwt_geteui(data.eui64);
}

void txcallback(const dwt_callback_data_t *txd)
{
	uint8 txTimeStamp[5] = {0, 0, 0, 0, 0};

	uint8 txevent = txd->event;
	event_data_t new_dw_event;

	if(txevent == DWT_SIG_TX_DONE)
	{
		//NOTE - we can only get TX good (done) while here

		dwt_readtxtimestamp(txTimeStamp) ;
		new_dwt_event.timeStamp32l = (uint32)txTimeStamp[0] + ((uint32)txTimeStamp[1] << 8) + ((uint32)txTimeStamp[2] << 16) + ((uint32)txTimeStamp[3] << 24);
		new_dwt_event.timeStamp = txTimeStamp[4];
	    new_dwt_event.timeStamp <<= 32;
		new_dwt_event.timeStamp += new_dwt_event.timeStamp32l;
		new_dwt_event.timeStamp32h = ((uint32)txTimeStamp[4] << 24) + (new_dwt_event.timeStamp32l >> 8);
		new_dwt_event.rxLength = 0;
		new_dwt_event.type2 = new_dwt_event.type = SIG_TX_DONE ;

		putevent(new_dwt_event);

	}
	else
	{
		//Log it
	}
}

void rxcallback(const dwt_callback_data_t *rxd)
{
	uint8 rxTimeStamp[5]  = {0, 0, 0, 0, 0};

    uint8 rxd_event = 0;
	uint8 fcode_index  = 0;
	event_data_t new_dwt_event;

	//if we got a frame with a good CRC - RX OK
    if(rxd->event == DWT_SIG_RX_OKAY)
	{

        rxd_event = SIG_RX_OKAY;

		new_dwt_event.rxLength = rxd->datalength;

		//need to process the frame control bytes to figure out what type of frame we have received
        //switch(rxd->fctrl[0])
	    //{
			////blink type frame
		    //case 0xC5:
				//if(rxd->datalength == 12)
				//{
					//rxd_event = SIG_RX_BLINK;
				//}
				//else if(rxd->datalength == 18)//blink with Temperature and Battery level indication
				//{
					//rxd_event = SIG_RX_BLINK;
				//}
				//else
					//rxd_event = SIG_RX_UNKNOWN;
					//break;

			////ACK type frame
			//case 0x02:
				//if(rxd->datalength == 5)
					//rxd_event = SIG_RX_ACK;
			    //break;

			////data type frames (with/without ACK request) - assume PIDC is on.
			//case 0x41:
			//case 0x61:
				////read the frame
				//if(rxd->datalength > MAX_FRAME_SIZE)
					//rxd_event = SIG_RX_UNKNOWN;

				////need to check the destination/source address mode
				//if((rxd->fctrl[1] & 0xCC) == 0x88) //dest & src short (16 bits)
				//{
					//fcode_index = FRAME_CRTL_AND_ADDRESS_S; //function code is in first byte after source address
				//}
				//else if((rxd->fctrl[1] & 0xCC) == 0xCC) //dest & src long (64 bits)
				//{
					//fcode_index = FRAME_CRTL_AND_ADDRESS_L; //function code is in first byte after source address
				//}
				//else //using one short/one long
				//{
					//fcode_index = FRAME_CRTL_AND_ADDRESS_LS; //function code is in first byte after source address
				//}
				//break;

			////any other frame types are not supported by this application
			//default:
				//rxd_event = SIG_RX_UNKNOWN;
				//break;
		//}


		//read rx timestamp
		if (rxd_event == SIG_RX_OKAY)
		{
			dwt_readrxtimestamp(rxTimeStamp) ;
			new_dwt_event.timeStamp32l =  (uint32)rxTimeStamp[0] + ((uint32)rxTimeStamp[1] << 8) + ((uint32)rxTimeStamp[2] << 16) + ((uint32)rxTimeStamp[3] << 24);
			new_dwt_event.timeStamp = rxTimeStamp[4];
			new_dwt_event.timeStamp <<= 32;
			new_dwt_event.timeStamp += new_dwt_event.timeStamp32l;
			new_dwt_event.timeStamp32h = ((uint32)rxTimeStamp[4] << 24) + (new_dwt_event.timeStamp32l >> 8);
		
			dwt_readrxdata((uint8 *)&new_dwt_event.msgu.frame[0], rxd->datalength, 0);  // Read Data Frame
			//dwt_readdignostics(&instance_data[instance].dwlogdata.diag);

			if(dwt_checkoverrun()) //the overrun has occured while we were reading the data - dump the frame/data
			{
				rxd_event = OTHERS ;
			}

		}
		
		new_dwt_event.type2 = new_dwt_event.type = rxd_event;
		
		//printf("rx call back %d %d FI:%08X %02x%02x event%d\n", rxd_event, instance_data[instance].ackreq, dwt_read32bitreg(0x10), rxd->fctrl[0], rxd->fctrl[1], instance_data[instance].dweventCnt);

		//Process good frames
		if(rxd_event == SIG_RX_OKAY)
		{
	    	putevent(new_dwt_event);
		}
		else if (rxd_event == SIG_RX_ACK)
		{

		}
		else if (rxd_event == SIG_RX_BLINK)
		{
		}

		if (rxd_event == OTHERS) //need to re-enable the rx
		{
			dwt_rxenable(0);
		}
	}
	else if (rxd->event == DWT_SIG_RX_TIMEOUT)
	{
		new_dwt_event.type2 = new_dwt_event.type = SIG_RX_TIMEOUT;
		new_dwt_event.rxLength = 0;
		new_dwt_event.timeStamp = 0;
		new_dwt_event.timeStamp32l = 0;
		new_dwt_event.timeStamp32h = 0;

		putevent(new_dwt_event);
		//printf("RX timeout while in %d\n", instance_data[instance].testAppState);
	}
	else //assume other events are errors
	{

	}
}

int main(void)
{
    uint8 dataseq[40];
	STATES current_state;
	STATES previous_state;
	EVENTS new_event;
	

    led_off(LED_ALL); //turn off all the LEDs

    peripherals_init();

    spi_peripheral_init();

    Sleep(1000); //wait for LCD to power on

    initLCD();

    memset(dataseq, 40, 0);
    memcpy(dataseq, (const uint8 *) "HELLO WORLD     ", 16);
    writetoLCD( 40, 1, dataseq); //send some data
    
    led_on(LED_ALL); //turn off all the LEDs
    Sleep(10);
	led_off(LED_ALL);
	
	if(port_IS_TAG_pressed() == 0){
		instance_mode = TAG;
        led_on(LED_PC7);
    }else{
		led_on(LED_PC6);
    }
	

	
	

	
	//init DW1000
	initDW();
	//SET INIT STATE
	current_state = INIT_STATE;
	//Launch Timer
	data.timer = portGetTickCount() + data.tagSleepTime_ms; //set timeout time
	data.timer_en = 1; //start timer
	data.stoptimer = 0 ; //clear the flag - timer can run if instancetimer_en set (set above)
		
	while (1){
		new_event = get_new_event (); /* get the next event to process */
		
		if (((new_event > 0) && (new_event < MAX_EVENTS))
		&& ((current_state >= 0) && (current_state < MAX_STATES))) {

			state_table [current_state][new_event] (); /* call the action procedure */

		} else {

			/* invalid event/state - handle appropriately */
		}
		
		//check if timer has expired
		if((data.timer_en == 1) && (data.stoptimer == 0))
		{
			if(data.timer < portGetTickCount())
			{
				event_data_t dw_event;
				data.timer_en = 0;
				dw_event.rxLength = 0;
				if(current_state == INIT_STATE || current_state == WAIT_STATE || current_state == TX_POLL_STATE){
					dw_event.type = TIMER_CALLBACK;
					dw_event.type2 = TIMER_CALLBACK;
				}else if(current_state == RX_RESP_WAIT){
					dw_event.type = SIG_RX_TIMEOUT;
					dw_event.type2 = SIG_RX_TIMEOUT;
				}
				putevent(dw_event);
			}
		}
    
	}
}
