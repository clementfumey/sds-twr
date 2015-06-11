#include "deca_types.h"
#include "deca_device_api.h"
#include "port.h"

#define MAX_FRAME_SIZE         127
#define POLL_SLEEP_DELAY					1 //ms
#define WAIT_SLEEP_DELAY					5 //ms
#define FIXED_REPLY_DELAY       			1
#define MAX_EVENT_NUMBER (10)

//Length
#define HEADER_LENGTH 8 //seqNum + panID + destddr + sourceAddr + fctCode= 1+2+2+2+1
#define MESSAGE_DATA_LATE 1
#define MESSAGE_DATA_POLL 0
#define MESSAGE_DATA_RESP 0
#define MESSAGE_DATA_FINAL 8//Two timestamp
#define CRC_LENGTH 2

#define SDSTWR_FCT_CODE_POLL 0x01
#define SDSTWR_FCT_CODE_RESP 0x02
#define SDSTWR_FCT_CODE_FINAL 0x03

//NOTE: only one transmission is allowed in 1ms when using smart tx power setting (applies for 6.81Mb data rate)
//blink tx time ~ 180us with 128 preamble length

#define FIXED_RP_REPLY_DELAY_US         (320)  //us //response delay time during ranging (Poll RX to Resp TX, Resp RX to Final TX)

enum states { INIT_STATE, TX_POLL_STATE, RX_RESP_WAIT, TX_FINAL_STATE, MAX_STATES };
enum events { NO_EVENT, SIG_RX_OKAY, SIG_TX_DONE, SIG_RX_TIMEOUT, OTHERS, TIMER_CALLBACK, MAX_EVENTS };

typedef struct
{
    uint8 seqNum;                               	//  sequence_number 02
    uint8 panID[2];                             	//  PAN ID 03-04
    uint8 destAddr[2];             					//  05-06
    uint8 sourceAddr[2];           					//  07-08
	uint8 fctCode;                               	//  sequence_number 02
    uint8 messageData[116] ; 						//  09-124 (application data and any user payload)
    uint8 fcs[2] ;                              	//  125-126  we allow space for the CRC as it is logically part of the message. However ScenSor TX calculates and adds these bytes.
} srd_msg ;

typedef struct
{
	uint8  type;			// event type
	uint8  type2;			// holds the event type - does not clear (not used to show if event has been processed)
	//uint8  broadcastmsg;	// specifies if the rx message is broadcast message
	uint16 rxLength ;

	uint64 timeStamp ;		// last timestamp (Tx or Rx)

	uint32 timeStamp32l ;		   // last tx/rx timestamp - low 32 bits
	uint32 timeStamp32h ;		   // last tx/rx timestamp - high 32 bits

	union {
			//holds received frame (after a good RX frame event)
			uint8   frame[127]; //STANDARD_FRAME_SIZE
    		srd_msg rxmsg ;
	}msgu;

	//uint32 eventtime ;
	//uint32 eventtimeclr ;
	//uint8 gotit;
}event_data_t ;

typedef struct
{
	//configuration structures
	dwt_config_t    configData ;	//DW1000 channel configuration
	dwt_txconfig_t  configTX ;		//DW1000 TX power configuration
	uint16			txantennaDelay ; //DW1000 TX antenna delay

	//timeouts and delays
	int tagSleepTime_ms; //in milliseconds
	int tagWaitTime_ms;
	//this is the delay used for the delayed transmit (when sending the ranging init, response, and final messages)
	uint64 fixedReplyDelay ;
	int fixedReplyDelay_ms ;

	// xx_sy the units are 1.0256 us
	int fixedReplyDelay_sy ;    // this is the delay used after sending a poll or response and turning on the receiver to receive response or final

	int fwtoTime_sy ;	//this is final message duration (longest out of ranging messages)
    uint32 fixedFastReplyDelay32h ; //this it the is the delay used before sending response or final message

	uint64 delayedReplyTime;		// delayed reply time of ranging-init/response/final message
	uint32 delayedReplyTime32;

    uint32 txTimeouts ;
    uint32 rxTimeouts ;
	uint8	stoptimer;				// stop/disable an active timer
    uint8	timer_en;		// enable/start a timer
    uint32	timer;			// e.g. this timer is used to timeout Tag when in deep sleep so it can send the next poll message
    
	//message structures used for transmitted messages
    srd_msg msg ;			// simple 802.15.4 frame structure (used for tx message) - using short addresses


	//Tag function address/message configuration
	uint8   eui64[8];				// devices EUI 64-bit address
	uint16  tagShortAdd ;		    // Tag's short address (16-bit) used when USING_64BIT_ADDR == 0
	uint16  anchShortAdd ;
	uint16  psduLength ;			// used for storing the frame length
    uint8   frame_sn;				// modulo 256 frame sequence number - it is incremented for each new frame transmittion
	uint16  panid ;					// panid used in the frames

	//64 bit timestamps
	//union of TX timestamps
	union {
		uint64 txTimeStamp ;		   // last tx timestamp
		uint64 tagPollTxTime ;		   // tag's poll tx timestamp
	    uint64 anchorRespTxTime ;	   // anchor's reponse tx timestamp
	}txu;
	uint64 anchorRespRxTime ;	    // receive time of response message
	uint64 tagPollRxTime ;          // receive time of poll message

	//32 bit timestamps (when "fast" ranging is used)
	uint32 tagPollTxTime32l ;      // poll tx time - low 32 bits
	uint32 tagPollRxTime32l ;      // poll rx time - low 32 bits
	uint32 anchorRespTxTime32l ;    // response tx time - low 32 bits

	uint32 anchResp1RxTime32l ;		// response 1 rx time - low 32 bits

    //diagnostic counters/data, results and logging
    uint32 tof32 ;
    uint64 tof ;
    double clockOffset ;

    uint32 blinkRXcount ;
	int txmsgcount;
	int	rxmsgcount;
	int lateTX;
	int lateRX;



	//event queue - used to store DW1000 events as they are processed by the dw_isr/callback functions
    event_data_t dwevent[MAX_EVENT_NUMBER]; //this holds any TX/RX events and associated message data
	event_data_t saved_dwevent; //holds an RX event while the ACK is being sent
    uint8 dweventIdxOut;
    uint8 dweventIdxIn;
	uint8 dweventPeek;

	int dwIDLE;
	
	enum states current_state;
	enum states previous_state;

} data_t ;

void send_poll (data_t *data);
void sleep_mode (data_t *data);
void rx_mode (data_t *data);
void send_final (data_t *data);
void ignore_event (data_t *data);
void rx_fail(data_t *data);
enum events get_new_event (data_t *data);

void (*const state_table [MAX_STATES][MAX_EVENTS]) (data_t *data) = {
    { ignore_event, ignore_event, ignore_event, ignore_event, ignore_event, send_poll }, /* procedures for state INIT_STATE */
    { ignore_event, ignore_event, rx_mode, ignore_event, ignore_event, send_poll }, /* procedures for state TX_POLL_STATE */
    { ignore_event, send_final, ignore_event, rx_fail, rx_fail, ignore_event }, /* procedures for state RX_RESP_WAIT */
    { ignore_event, ignore_event, sleep_mode, ignore_event, ignore_event, sleep_mode }  /* procedures for state TX_FINAL_STATE */
};
typedef struct {
                uint8 PGdelay;

                //TX POWER
                //31:24     BOOST_0.125ms_PWR
                //23:16     BOOST_0.25ms_PWR-TX_SHR_PWR
                //15:8      BOOST_0.5ms_PWR-TX_PHR_PWR
                //7:0       DEFAULT_PWR-TX_DATA_PWR
                uint32 txPwr[2]; //
}tx_struct;
static const uint16 rfDelays[2];
static const tx_struct txSpectrumConfig[8];
