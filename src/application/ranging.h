
#define MAX_FRAME_SIZE         127
#define POLL_SLEEP_DELAY					1 //ms
#define WAIT_SLEEP_DELAY					5 //ms

typedef struct
{
    uint8 frameCtrl[2];                         	//  frame control bytes 00-01
    uint8 seqNum;                               	//  sequence_number 02
    uint8 panID[2];                             	//  PAN ID 03-04
    uint8 destAddr[2];             					//  05-06
    uint8 sourceAddr[2];           					//  07-08
    uint8 messageData[116] ; 						//  09-124 (application data and any user payload)
    uint8 fcs[2] ;                              	//  125-126  we allow space for the CRC as it is logically part of the message. However ScenSor TX calculates and adds these bytes.
} srd_msg ;


typedef struct
{
    INST_MODE mode;				//instance mode (tag or anchor)

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
	int rnginitW4Rdelay_sy ;	// this is the delay used after sending a blink and turning on the receiver to receive the ranging init message

	int fwtoTime_sy ;	//this is final message duration (longest out of ranging messages)
	int fwtoTimeB_sy ;	//this is the ranging init message duration

    uint32 fixedRngInitReplyDelay32h ;	//this is the delay after receiving a blink before responding with ranging init message
    uint32 fixedFastReplyDelay32h ; //this it the is the delay used before sending response or final message

	uint64 delayedReplyTime;		// delayed reply time of ranging-init/response/final message
	uint32 delayedReplyTime32;

    uint32 rxTimeouts ;
	uint8	stoptimer;				// stop/disable an active timer
    uint8	timer_en;		// enable/start a timer
    uint32	timer;			// e.g. this timer is used to timeout Tag when in deep sleep so it can send the next poll message
    
	//message structures used for transmitted messages
    srd_msg msg ;			// simple 802.15.4 frame structure (used for tx message) - using short addresses


	//Tag function address/message configuration
	uint8   eui64[8];				// devices EUI 64-bit address
	uint16  tagShortAdd ;		    // Tag's short address (16-bit) used when USING_64BIT_ADDR == 0
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
} data_t ;
