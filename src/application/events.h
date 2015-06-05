
#define MAX_EVENT_NUMBER (10)

enum events { NO_EVENT, SIG_RX_OKAY, SIG_TX_DONE, SIG_RX_TIMEOUT, OTHERS, TIMER_CALLBACK, MAX_EVENTS } EVENTS;
enum events get_new_event (void);

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
			uint8   frame[STANDARD_FRAME_SIZE];
    		srd_msg_dlsl rxmsg_ll ; //64 bit addresses
			srd_msg_dssl rxmsg_sl ;
			srd_msg_dlss rxmsg_ls ;
			srd_msg_dsss rxmsg_ss ; //16 bit addresses
			ack_msg rxackmsg ; //holds received ACK frame
			iso_IEEE_EUI64_blink_msg rxblinkmsg;
			iso_IEEE_EUI64_blinkdw_msg rxblinkmsgdw;
	}msgu;

	//uint32 eventtime ;
	//uint32 eventtimeclr ;
	//uint8 gotit;
}event_data_t ;
