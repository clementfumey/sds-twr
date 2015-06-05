
#includes "events.h"

void putevent(event_data_t newevent)
{
	int instance = 0;
	uint8 etype = newevent.type;

	newevent.type = 0;
	//copy event
	dwevent[dweventIdxIn] = newevent;

	//set type - this makes it a new event (making sure the event data is copied before event is set as new)
	//to make sure that the get event function does not get an incomplete event
	dwevent[dweventIdxIn].type = etype;

	dweventIdxIn++;

	if(MAX_EVENT_NUMBER == instance_data[instance].dweventIdxIn)
		instance_data[instance].dweventIdxIn = 0;
}

enum events get_new_event (void)
{
    return data.dwevent[dweventPeek].type;
}

event_data_t dw_event_g;


#pragma GCC optimize ("O0")
event_data_t* getevent()
{
	int indexOut = data.dweventIdxOut;

	if(data.dwevent[indexOut].type == 0) //exit with "no event"
	{
		dw_event_g.type = 0;
		dw_event_g.type2 = 0;
		return &dw_event_g;
	}

	//copy the event
	dw_event_g.type2 = data.dwevent[indexOut].type2 ;
	dw_event_g.rxLength = data.dwevent[indexOut].rxLength ;
	dw_event_g.timeStamp = data.dwevent[indexOut].timeStamp ;
	dw_event_g.timeStamp32l = data.dwevent[indexOut].timeStamp32l ;
	dw_event_g.timeStamp32h = data.dwevent[indexOut].timeStamp32h ;
	//dw_event_g.eventtime = instance_data[instance].dwevent[indexOut].eventtime ;
	//dw_event_g.eventtimeclr = instance_data[instance].dwevent[indexOut].eventtimeclr ;
	//dw_event_g.gotit = instance_data[instance].dwevent[indexOut].gotit ;

	memcpy(&dw_event_g.msgu, data.dwevent[indexOut].msgu, sizeof(data.dwevent[indexOut].msgu));

	dw_event_g.type = data.dwevent[indexOut].type ;


	//instance_data[instance].dwevent[indexOut].gotit = x;

	//instance_data[instance].dwevent[indexOut].eventtimeclr = portGetTickCount();

	data.dwevent[indexOut].type = 0; //clear the event

	data.dweventIdxOut++;
	if(MAX_EVENT_NUMBER == data.dweventIdxOut) //wrap the counter
		data..dweventIdxOut = 0;

	data.dweventPeek = data.dweventIdxOut; //set the new peek value

	//if(dw_event.type) printf("get %d - in %d out %d @ %d\n", dw_event.type, instance_data[instance].dweventCntIn, instance_data[instance].dweventCntOut, ptime);

	//eventOutcount++;


	return &dw_event_g;
}
