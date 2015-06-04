
/* Includes */
#include "compiler.h"
#include "port.h"
#include "deca_types.h"
#include "deca_spi.h"

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


/*
 * @fn      main()
 * @brief   main entry point
**/

/* Define the states and events. If your state machine program has multiple
source files, you would probably want to put these definitions in an "include"
file and #include it in each source file. This is because the action
procedures need to update current_state, and so need access to the state
definitions. */

enum states { INIT_STATE, TX_POLL_STATE, WAIT_STATE, RX_RESP_WAIT, TX_FINAL_STATE, MAX_STATES } STATES;
//enum events { DWT_SIG_RX_OKAY, DWT_SIG_TX_DONE, DWT_SIG_RX_TIMEOUT, DWT_OTHERS, TIMER_CALLBACK, MAX_EVENTS } EVENTS;

/* Provide the fuction prototypes for each action procedure. In a real
program, you might have a separate source file for the action procedures of 
each state. Then you could create a .h file for each of the source files, 
and put the function prototypes for the source file in the .h file. Instead 
of listing the prototypes here, you would just #include the .h files. */

void send_poll (void);
void sleep_mode (void);
void rx_mode (void);
void send_final (void);
void ignore_event (void);
void rx_fail(void);

enum events get_new_event (void);

/* Define the state/event lookup table. The state/event order must be the
same as the enum definitions. Also, the arrays must be completely filled - 
don't leave out any events/states. If a particular event should be ignored in 
a particular state, just call a "do-nothing" function. */

void (*const state_table [MAX_STATES][MAX_EVENTS]) (void) = {

    { ignore_event, ignore_event, ignore_event, ignore_event, send_poll }, /* procedures for state INIT_STATE */
    { ignore_event, sleep_mode, ignore_event, ignore_event, ignore_event }, /* procedures for state TX_POLL_STATE */
    { ignore_event, ignore_event, ignore_event, ignore_event, rx_mode }  /* procedures for state WAIT_STATE */
    { send_final, ignore_event, rx_fail, rx_fail, ignore_event }, /* procedures for state RX_RESP_WAIT */
    { ignore_event, sleep_mode, ignore_event, ignore_event, ignore_event }  /* procedures for state TX_FINAL_STATE */
};


/* In an action procedure, you do whatever processing is required for the
particular event in the particular state. Among other things, you might have
to set a new state. */

void send_poll (void) 
{
	previous_state = current_state;
	current_state = TX_POLL_STATE; /* set new state, if necessary */
}

void sleep_mode (void) 
{
	if (current_state == TX_POLL_STATE){
		//Set Timer
		
		previous_state = current_state;
		current_state = WAIT_STATE;
	}else{ // TX_FINAL_STATE
		//Set Timer
		
		previous_state = current_state;
		current_state = INIT_STATE;
	}
	
}
void rx_mode (void) 
{
	//Put DW1000 in rx
	previous_state = current_state;
	current_state = RX_RESP_WAIT;
}
void send_final (void)
{
	
	//Set Timer
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
	previous_state = current_state;
	current_state = INIT_STATE;
}


/* Return the next event to process - how this works depends on your
application. */

enum events get_new_event (void)
{
    return EVENT_1;
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
