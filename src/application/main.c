
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


enum events get_new_event (void)
{
    return EVENT_1;
} 

void instance_txcallback(const dwt_callback_data_t *txd)
{
	int instance = 0;
	uint8 txTimeStamp[5] = {0, 0, 0, 0, 0};

	uint8 txevent = txd->event;
	event_data_t dw_event;

	if(txevent == DWT_SIG_TX_DONE)
	{
		//NOTE - we can only get TX good (done) while here

		dwt_readtxtimestamp(txTimeStamp) ;
		dw_event.timeStamp32l = (uint32)txTimeStamp[0] + ((uint32)txTimeStamp[1] << 8) + ((uint32)txTimeStamp[2] << 16) + ((uint32)txTimeStamp[3] << 24);
		dw_event.timeStamp = txTimeStamp[4];
	    dw_event.timeStamp <<= 32;
		dw_event.timeStamp += dw_event.timeStamp32l;
		dw_event.timeStamp32h = ((uint32)txTimeStamp[4] << 24) + (dw_event.timeStamp32l >> 8);
		dw_event.rxLength = 0;
		dw_event.type2 = dw_event.type = SIG_TX_DONE ;

		instance_putevent(dw_event);

	}
	else
	{
		//Log it
	}
}

void instance_rxcallback(const dwt_callback_data_t *rxd)
{
	uint8 rxTimeStamp[5]  = {0, 0, 0, 0, 0};

    uint8 rxd_event = 0;
	uint8 fcode_index  = 0;
	event_data_t dw_event;

	//if we got a frame with a good CRC - RX OK
    if(rxd->event == DWT_SIG_RX_OKAY)
	{

        rxd_event = DWT_SIG_RX_OKAY;

		dw_event.rxLength = rxd->datalength;

		//need to process the frame control bytes to figure out what type of frame we have received
        switch(rxd->fctrl[0])
	    {
			//blink type frame
		    case 0xC5:
				if(rxd->datalength == 12)
				{
					rxd_event = SIG_RX_BLINK;
				}
				else if(rxd->datalength == 18)//blink with Temperature and Battery level indication
				{
					rxd_event = SIG_RX_BLINK;
				}
				else
					rxd_event = SIG_RX_UNKNOWN;
					break;

			//ACK type frame
			case 0x02:
				if(rxd->datalength == 5)
					rxd_event = SIG_RX_ACK;
			    break;

			//data type frames (with/without ACK request) - assume PIDC is on.
			case 0x41:
			case 0x61:
				//read the frame
				if(rxd->datalength > MAX_FRAME_SIZE)
					rxd_event = SIG_RX_UNKNOWN;

				//need to check the destination/source address mode
				if((rxd->fctrl[1] & 0xCC) == 0x88) //dest & src short (16 bits)
				{
					fcode_index = FRAME_CRTL_AND_ADDRESS_S; //function code is in first byte after source address
				}
				else if((rxd->fctrl[1] & 0xCC) == 0xCC) //dest & src long (64 bits)
				{
					fcode_index = FRAME_CRTL_AND_ADDRESS_L; //function code is in first byte after source address
				}
				else //using one short/one long
				{
					fcode_index = FRAME_CRTL_AND_ADDRESS_LS; //function code is in first byte after source address
				}
				break;

			//any other frame types are not supported by this application
			default:
				rxd_event = SIG_RX_UNKNOWN;
				break;
		}


		//read rx timestamp
		if((rxd_event == SIG_RX_ACK) || (rxd_event == SIG_RX_BLINK) || (rxd_event == DWT_SIG_RX_OKAY))
		{
			dwt_readrxtimestamp(rxTimeStamp) ;
			dw_event.timeStamp32l =  (uint32)rxTimeStamp[0] + ((uint32)rxTimeStamp[1] << 8) + ((uint32)rxTimeStamp[2] << 16) + ((uint32)rxTimeStamp[3] << 24);
			dw_event.timeStamp = rxTimeStamp[4];
			dw_event.timeStamp <<= 32;
			dw_event.timeStamp += dw_event.timeStamp32l;
			dw_event.timeStamp32h = ((uint32)rxTimeStamp[4] << 24) + (dw_event.timeStamp32l >> 8);
		
			dwt_readrxdata((uint8 *)&dw_event.msgu.frame[0], rxd->datalength, 0);  // Read Data Frame
			//dwt_readdignostics(&instance_data[instance].dwlogdata.diag);

			if(dwt_checkoverrun()) //the overrun has occured while we were reading the data - dump the frame/data
			{
				rxd_event = DWT_SIG_RX_ERROR ;
			}

		}
		
		dw_event.type2 = dw_event.type = rxd_event;
		
		//printf("rx call back %d %d FI:%08X %02x%02x event%d\n", rxd_event, instance_data[instance].ackreq, dwt_read32bitreg(0x10), rxd->fctrl[0], rxd->fctrl[1], instance_data[instance].dweventCnt);

		//Process good frames
		if(rxd_event == DWT_SIG_RX_OKAY)
		{
			//if(rxd->dblbuff == 0)  instance_readaccumulatordata();     // for diagnostic display in DecaRanging PC window
			//instance_calculatepower();

	    	instance_data[instance].stoptimer = 1;

            if(instance_data[instance].ackreq == 0) //only notify there is event if no ACK pending
	    		instance_putevent(dw_event);
			else
				instance_saveevent(dw_event);

			#if DECA_LOG_ENABLE==1
			#if DECA_KEEP_ACCUMULATOR==1
			{
				instance_data[instance].dwacclogdata.newAccumData = 1 ;
				instance_data[instance].dwacclogdata.erroredFrame = DWT_SIG_RX_NOERR ;	//no error
				processSoundingData();
			}
			#endif
				logSoundingData(DWT_SIG_RX_NOERR, dw_event.msgu.frame[fcode_index], dw_event.msgu.frame[2], &dw_event);
			#endif

			//printf("RX OK %d %x\n",instance_data[instance].testAppState, instance_data[instance].rxmsg.messageData[FCODE]);
			//printf("RX OK %d ", instance_data[instance].testAppState);
			//printf("RX time %f ecount %d\n",convertdevicetimetosecu(dw_event.timeStamp), instance_data[instance].dweventCnt);

	#if (DEEP_SLEEP == 1)
			instance_data[instance].rxmsgcount++;
	#endif
		}
		else if (rxd_event == SIG_RX_ACK)
		{
			//printf("RX ACK %d (count %d) \n", instance_data[instance].testAppState, instance_data[instance].dweventCnt);
			instance_putevent(dw_event);
			//if(rxd->dblbuff == 0) instance_readaccumulatordata();     // for diagnostic display
	#if (DEEP_SLEEP == 1)
			instance_data[instance].rxmsgcount++;
	#endif
		}
		else if (rxd_event == SIG_RX_BLINK)
		{
			instance_putevent(dw_event);
			//if(rxd->dblbuff == 0) instance_readaccumulatordata();     // for diagnostic display

			//instance_calculatepower();
			//printf("RX BLINK %d (count %d) \n", instance_data[instance].testAppState, instance_data[instance].dweventCnt);

			#if DECA_LOG_ENABLE==1
			#if DECA_KEEP_ACCUMULATOR==1
			{
				instance_data[instance].dwacclogdata.newAccumData = 1 ;
				instance_data[instance].dwacclogdata.erroredFrame = DWT_SIG_RX_NOERR ;	//no error
				processSoundingData();
			}
			#endif
				logSoundingData(DWT_SIG_RX_NOERR, 0xC5, dw_event.msgu.rxblinkmsg.seqNum, &dw_event);
			#endif

	#if (DEEP_SLEEP == 1)
				instance_data[instance].rxmsgcount++;
	#endif
		}

		/*if(instance_data[instance].mode == LISTENER) //print out the message bytes when in Listener mode
		{
			int i;
			uint8 buffer[1024];
			dwt_readrxdata(buffer, rxd->datalength, 0);  // Read Data Frame
			buffer[1023] = 0;
			instancelogrxdata(&instance_data[instance], buffer, rxd->datalength);


			printf("RX data(%d): ", rxd->datalength);
			for(i=0; i<rxd->datalength; i++)
			{
				printf("%02x", buffer[i]);
			}
			printf("\n");
		}*/

		if (rxd_event == SIG_RX_UNKNOWN) //need to re-enable the rx
		{
			//if(rxd->dblbuff == 0) instance_readaccumulatordata();     // for diagnostic display

			//dwt_readdignostics(&instance_data[instance].dwlogdata.diag);

			//instance_calculatepower();

			#if DECA_LOG_ENABLE==1
			#if DECA_KEEP_ACCUMULATOR==1
			{
				instance_data[instance].dwacclogdata.newAccumData = 1 ;
				instance_data[instance].dwacclogdata.erroredFrame = DWT_SIG_RX_NOERR ;	//no error
				processSoundingData();
			}
			#endif
				logSoundingData(DWT_SIG_RX_NOERR, dw_event.msgu.frame[fcode_index], dw_event.msgu.frame[2], &dw_event);
			#endif

			if(instance_data[instance].doublebufferon == 0)
				instancerxon(&instance_data[instance], 0, 0); //immediate enable
		}
	}
	else if (rxd->event == DWT_SIG_RX_TIMEOUT)
	{
		dw_event.type2 = dw_event.type = DWT_SIG_RX_TIMEOUT;
		dw_event.rxLength = 0;
		dw_event.timeStamp = 0;
		dw_event.timeStamp32l = 0;
		dw_event.timeStamp32h = 0;

		instance_putevent(dw_event);
		//printf("RX timeout while in %d\n", instance_data[instance].testAppState);
	}
	else //assume other events are errors
	{
		//printf("RX error %d \n", instance_data[instance].testAppState);
		if(instance_data[instance].rxautoreenable == 0)
		{
			//re-enable the receiver
#if (DECA_BADF_ACCUMULATOR == 1)
			instance_readaccumulatordata();

			dwt_readdignostics(&instance_data[instance].dwlogdata.diag);

			instance_calculatepower();
#endif
#if (DECA_ERROR_LOGGING == 1)
			#if DECA_LOG_ENABLE==1
			#if DECA_BADF_ACCUMULATOR==1
			{
				instance_data[instance].dwacclogdata.newAccumData = 1 ;
				instance_data[instance].dwacclogdata.erroredFrame = rxd->event ;	//no error
				processSoundingData();
			}
			#endif
				logSoundingData(rxd->event, 0, 0, &dw_event);
			#endif
#endif

			//for ranging application rx error frame is same as TO - as we are not going to get the expected frame
			if((instance_data[instance].mode == TAG) || (instance_data[instance].mode == TAG_TDOA))
			{
				dw_event.type = DWT_SIG_RX_TIMEOUT;
				dw_event.type2 = 0x40 | DWT_SIG_RX_TIMEOUT;
				dw_event.rxLength = 0;

				instance_putevent(dw_event);
			}
			else
			{
				instancerxon(&instance_data[instance], 0, 0); //immediate enable if anchor or listener
			}

		}
	}
}

int main(void)
{
    int i = 0;
    int toggle = 1;
    int ranging = 0;
    uint8 dataseq[40];
    double range_result = 0;
    double avg_result = 0;
    uint8 dataseq1[40];

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
	
	STATES current_state;
	STATES previous_state;
	EVENTS new_event;
	
	

	
	//init DW1000
	
	//SET INIT STATE
	
	//Launch Timer
	
	while (1){
		new_event = get_new_event (); /* get the next event to process */
		
		if (((new_event >= 0) && (new_event < MAX_EVENTS))
		&& ((current_state >= 0) && (current_state < MAX_STATES))) {

			state_table [current_state][new_event] (); /* call the action procedure */

		} else {

			/* invalid event/state - handle appropriately */
		}
    
	}
}
