/* ---------------------------------------------------------------------------
 * modulo per la gestione di mqtt  con scsgate
 * ---------------------------------------------------------------------------
*/
//
//
// =============================================================================================
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <errno.h>
#include "MQTTClient.h"
#include "scs_mqtt_y.h"
#include <sys/stat.h>
#include <vector>
// =============================================================================================
#include <functional>
// =============================================================================================
using namespace std;
// =============================================================================================
static int  mqttTimer = 0;
static char mqVerbose = 0;
static char mqttopen = 0;  //0=spento    1=richiesto    2=in corso     3=connesso
static char domoticMode = 'H';
static char mqttAddress[24];
static int	volatile MQTTbusy = 0;
// =============================================================================================
extern std::vector<bus_scs_queue> _schedule_b;
	   std::vector<publish_queue> _publish_b;
// =============================================================================================
MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;
// =============================================================================================
void delivered(void *context, MQTTClient_deliveryToken dt);
// =============================================================================================
void mqSleep(int millisec);
void uqSleep(int microsec);
// =============================================================================================











// =============================================================================================
void mqSleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void uqSleep(int microsec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = microsec * 1000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void sqSleep(int sec) {
	int s = sec * 100;
    while (s-- > 0)
	{
		mqSleep(10);
	}
}
// ===================================================================================
int processMessage(char * topicName, char * payLoad)
// ===================================================================================
// è arrivato un messaggio MQTT
// input: topic e payload           
// output: se necessario topic di stato
//         coda di esecuzione scs  schedule_b
// ===================================================================================
{
  char dev[5];
  char *ch;
  char devtype = 0;
  char device = 0xFF;
  char from = 0x00;
  char request = 0x12;
  char reply = 0;
  char command = 0xFF;
  char value = 0;
  publish_queue to_publish;
        
  strcpy(to_publish.payload, payLoad);

// --------------------------------------- SWITCHES -------------------------------------------
  if (memcmp(topicName, SWITCH_SET, sizeof(SWITCH_SET) - 1) == 0)
  {
    devtype = 1; // switch
    strcpy(to_publish.topic, SWITCH_STATE);
    reply = 1;
    dev[0] = *(topicName + sizeof(SWITCH_SET) - 1);
    dev[1] = *(topicName + sizeof(SWITCH_SET));
    dev[2] = 0;
    device = (char)strtoul(dev, &ch, 16);
    if (memcmp(payLoad, "ON", 2) == 0)
      command = 0x00;
    else if (memcmp(payLoad, "OFF", 3) == 0)
      command = 0x01;

	if (mqVerbose) fprintf(stderr,"switch %s  cmd %x \n", dev, command);
  }
  // ----------------------------------------------------------------------------------------------
  else
    // --------------------------------------- LIGHTS DIMM ------------------------------------------
  if (memcmp(topicName, BRIGHT_SET, sizeof(BRIGHT_SET) - 1) == 0)
  {
    devtype = 3; // light
    strcpy(to_publish.topic, BRIGHT_STATE);
    reply = 1;
    dev[0] = *(topicName + sizeof(BRIGHT_SET) - 1);
    dev[1] = *(topicName + sizeof(BRIGHT_SET));
    dev[2] = 0;
    device = (char)strtoul(dev, &ch, 16);
    if (memcmp(payLoad, "ON", 2) == 0)
      command = 0x00;
    else if (memcmp(payLoad, "OFF", 3) == 0)
      command = 0x01;
   else
      {
	    command = 0xFF;
        value = atoi(payLoad);    // percentuale
		if (mqVerbose) fprintf(stderr,"light %s  cmd %x val %d\n", dev, command, value);
      }
    }
  // ----------------------------------------------------------------------------------------------
    else
  // --------------------------------------- COVER ------------------------------------------------
  if (memcmp(topicName, COVER_SET, sizeof(COVER_SET) - 1) == 0)
  {
    devtype = 8; // cover
    strcpy(to_publish.topic, COVER_STATE);
    reply = 1;
    dev[0] = *(topicName + sizeof(COVER_SET) - 1);
    dev[1] = *(topicName + sizeof(COVER_SET));
    dev[2] = 0;
    device = (char)strtoul(dev, &ch, 16);

    if (memcmp(payLoad, "ON", 2) == 0)
      command = 0x09;
    else if (memcmp(payLoad, "CLOSE", 5) == 0)
	{
	  command = 0x09;
      if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
	      strcpy(to_publish.payload, "closed");
	}
    else if (memcmp(payLoad, "OFF", 3) == 0)
      command = 0x08;
    else if (memcmp(payLoad, "OPEN", 4) == 0)
	{
	  command = 0x08;
      if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
	      strcpy(to_publish.payload, "open");
	}
    else if (memcmp(payLoad, "STOP", 4) == 0)
      command = 0x0A;

	if (mqVerbose) fprintf(stderr,"cover %s  cmd %x \n", dev, command);
  }
  // ----------------------------------------------------------------------------------------------
    else
  // --------------------------------------- COVERPCT ---------------------------------------------
  if (memcmp(topicName, COVERPCT_SET, sizeof(COVERPCT_SET) - 1) == 0)
  {

    devtype = 9; // cover
    strcpy(to_publish.topic, COVERPCT_STATE);
    reply = 0;
    dev[0] = *(topicName + sizeof(COVERPCT_SET) - 1);
    dev[1] = *(topicName + sizeof(COVERPCT_SET));
    dev[2] = 0;
    device = (char)strtoul(dev, &ch, 16);

    if (memcmp(payLoad, "STOP", 4) == 0)
	{
//    command = 0;
	  command = 0x0A;
	}
	else if (memcmp(payLoad, "ON", 2) == 0)
	{
//      command = 2;
	  command = 0x09;
	}
    else if (memcmp(payLoad, "CLOSE", 5) == 0)
	{
//	  command = 2;
	  command = 0x09;
      if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
	      strcpy(to_publish.payload, "closed");
	}

    else if (memcmp(payLoad, "OFF", 3) == 0)
	{
//      command = 1;
	  command = 0x08;
	}
    else if (memcmp(payLoad, "OPEN", 4) == 0)
	{
//	  command = 1;
	  command = 0x08;
      if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
	      strcpy(to_publish.payload, "open");
	}
	else
	{
	  command = 0xFF;
	  value = atoi(payLoad);    // percentuale
	}

	if (mqVerbose) fprintf(stderr,"cover %s  cmd %x \n", dev, command);

  }
  // ----------------------------------------------------------------------------------------------
    else
  // --------------------------------------- GENERIC ----------------------------------------------
  if (memcmp(topicName, GENERIC_SET, sizeof(GENERIC_SET) - 1) == 0)
  {
    devtype = 11; // generic
//  strcpy(to_publish.topic, GENERIC_STATE);
    reply = 0;
    dev[0] = *(topicName + sizeof(GENERIC_SET) - 1);
    dev[1] = *(topicName + sizeof(GENERIC_SET));
    dev[2] = 0;
    device = (char)strtoul(dev, &ch, 16);

    *(payLoad+6) = 0;        // packet: ffttcc ->  <from> <type> <command>
    command = (char)strtoul(payLoad+4, &ch, 16);
    *(payLoad+4) = 0;
    request = (char)strtoul(payLoad+2, &ch, 16);
    *(payLoad+2) = 0;
    from = (char)strtoul(payLoad, &ch, 16);
  }
  // ----------------------------------------------------------------------------------------------
  else
  // ------------------------------------ SWITCHES STATE-------------------------------------------
  if (memcmp(topicName, SWITCH_STATE, sizeof(SWITCH_STATE) - 1) == 0)
  {
    devtype = 1; // switch
    reply = 0;
    dev[0] = *(topicName + sizeof(SWITCH_STATE) - 1);
    dev[1] = *(topicName + sizeof(SWITCH_STATE));
    dev[2] = 0;
    device = (char)strtoul(dev, &ch, 16);
    if (memcmp(payLoad, "ON", 2) == 0)
      command = 0x00;
    else if (memcmp(payLoad, "OFF", 3) == 0)
      command = 0x01;
	request = 0x15;
	if (mqVerbose) fprintf(stderr,"switch %s  STATO %x \n", dev, command);
  }
  // ----------------------------------------------------------------------------------------------

  // ----------------------------------------------------------------------------------------------
  if ((reply == 1) && (device != 0) && (devtype != 0))
  { // device valido 
	  strcat(to_publish.topic, dev);
	  if (mqVerbose) 	fprintf(stderr,"pub schedule\n");
	  to_publish.retain = 1;
	  _publish_b.push_back(to_publish);
  }       // device valido
  // ----------------------------------------------------------------------------------------------

  if ((device != 0) && (devtype != 0))
  {
	// schedulare azione (_command) su devices (id)  con valore (value)
	  bus_scs_queue schedule;

	  schedule.busid = device;
	  schedule.bustype = devtype;
	  schedule.buscommand = command;
	  schedule.busvalue = value;
	  schedule.busfrom = from;
	  schedule.busrequest = request;
	// push
	  _schedule_b.push_back(schedule);
  }
  return 0;
}
// ===================================================================================



// ===================================================================================
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
// ===================================================================================
// è arrivato un messaggio MQTT - processo
// ===================================================================================
{
	(void) context;
	(void) topicLen;

    int i;
    char* payloadptr;
    char* payLoad;

	if (mqVerbose>1) printf("Message arrived - topic: %s   message: ", topicName);

    payloadptr = (char*)message->payload;
    payLoad = (char*)message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        if (mqVerbose) putchar(*payloadptr);
        payloadptr++;
    }
	*payloadptr = 0;
    if (mqVerbose) putchar('\n');

	processMessage(topicName, payLoad);

	MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
// ===================================================================================
void connlost(void *context, char *cause)
// ===================================================================================
// connessione persa - hangup broker
// ===================================================================================
{
	(void) context;
    if (mqVerbose) printf("\nConnection lost - cause: %s\n", cause);
	mqttopen = 1;
}
// ===================================================================================
int MQTTconnect(char * broker, char * user, char * password, char verbose)
// ===================================================================================
// connessione broker MQTT e sottoscrizione topics
// ===================================================================================
{
    int rc;
	if (*broker == 0) return 0;

	_publish_b.clear();

	mqVerbose = verbose;
	strcpy(mqttAddress, broker);

    if (mqVerbose) fprintf(stderr,"MQTT connection at %s ...\n", mqttAddress);
	mqttopen = 1;

	MQTTClient_create(&client, mqttAddress, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);

	conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
//	set_user_name(user);
//	set_password(password);
	conn_opts.username = (const char *) user;
	conn_opts.password = (const char *) password;
//	conn_opts.username = "pippo";
//	conn_opts.password = "paperino";

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);	// MQTT ASINCRONO - procedure di callback x ---, connection lost, message arrived, delivery complete

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        if (mqVerbose) fprintf(stderr,"MQTT - Failed to connect, return code %d\n", rc);
		return 0;
    }
	mqttopen = 2;

	MQTTClient_subscribe(client, SUBSCRIBE1, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE2, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE3, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE4, QOS);

    if (mqVerbose) fprintf(stderr,"MQTT connected\n");
	mqttopen = 3;
	return 1;
}
// ===================================================================================
void MQTTreconnect(void)
// ===================================================================================
// tentativo di riconnessione broker MQTT e sottoscrizione topics
// ===================================================================================
{
    int rc;
    if (mqVerbose) fprintf(stderr,"MQTT reconnection at %s ...\n", mqttAddress);

	conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        if (mqVerbose) fprintf(stderr,"MQTT - Failed to reconnect, return code %d\n", rc);
        return;
    }

	MQTTClient_subscribe(client, SUBSCRIBE1, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE2, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE3, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE4, QOS);

    if (mqVerbose) fprintf(stderr,"MQTT - reconnected\n");
	mqttopen = 3;
}
// ===================================================================================
void publish(char * pTopic, char * pPayload, int retain)
// ===================================================================================
// pubblicazione topic/payload
// ===================================================================================
{
	if (mqttopen != 3)  
		return;

	if (mqVerbose>1) fprintf(stderr,".");
	while (MQTTbusy)
	{
		mqSleep(1);
	}

	if (mqVerbose>1) fprintf(stderr,"publish topic: %s    payload: %s \n",pTopic,pPayload);

	pubmsg.payload = pPayload;
    pubmsg.payloadlen = (int)strlen(pPayload);
    pubmsg.qos = QOS;
    pubmsg.retained = retain;

	MQTTbusy = 1;
    MQTTClient_publishMessage(client, pTopic, &pubmsg, &token);
    MQTTClient_waitForCompletion(client, token, TIMEOUT);
}

// ===================================================================================
void publish_dequeue(void)
// ===================================================================================
{
	if ((_publish_b.size() > 0) && (MQTTbusy == 0))
	{
		pubmsg.payload = _publish_b[0].payload;
		pubmsg.payloadlen = (int)strlen(_publish_b[0].payload);
		pubmsg.qos = QOS;
		pubmsg.retained = _publish_b[0].retain;
		MQTTbusy = 2;
		MQTTClient_publishMessage(client, (_publish_b[0].topic), &pubmsg, &token);
		MQTTClient_waitForCompletion(client, token, TIMEOUT);
		_publish_b.erase(_publish_b.begin());
	}
}
// ===================================================================================
void delivered(void *context, MQTTClient_deliveryToken dt)
// ===================================================================================
// è arrivato un messaggio MQTT - processo
// ===================================================================================
{
	(void) context;
	(void) dt;
	if (mqVerbose>1) fprintf(stderr,"Delivered!\n");
	MQTTbusy = 0;
}
// ===================================================================================
void MQTTverify(void)
// ===================================================================================
// verifica connessione broker - se disconnesso tenta la riconnessione
// ===================================================================================
{
	if ((mqttopen) && (mqttopen != 3))  
	{ 
		mqttTimer++;
		mqSleep(10);	// 10msecs wait
		if (mqttTimer > 1000)  // 10 sec
		{
			MQTTreconnect();
			mqttTimer = 0;
		}
	}
	else
		mqttTimer = 0;
}
// ===================================================================================
void MQTTstop(void)
// ===================================================================================
// ferma connessione al broker
// ===================================================================================
{
	if (mqttopen == 3)  
	{
		MQTTClient_unsubscribe(client, SUBSCRIBE1);
		MQTTClient_unsubscribe(client, SUBSCRIBE2);
		MQTTClient_unsubscribe(client, SUBSCRIBE3);
		MQTTClient_unsubscribe(client, SUBSCRIBE4);

		MQTTClient_disconnect(client, 10000);
		MQTTClient_destroy(&client);
	}
}
// ===================================================================================
char MQTTrequest(bus_scs_queue * busdata)
// ===================================================================================
// è arrivato un messaggio SCS
// input: dati del messaggio e tipo dispositivo censito
// output: se necessario topic di stato
// return: tipo dispositivo individuato
// ===================================================================================
{
// ---------------------------------------------------------------------------------------------------------------------------------
	if (mqttopen != 3)	return 0xFF;

 // START pubblicazione stato device        [0xF5] [y] 32 00 12 01
	char device = 0;
	char devtype = 0;
	char action;
	char topic[32];
	char payload[64];
	char nomeDevice[4];

	device = busdata->busid;
	action = busdata->buscommand;

	sprintf(nomeDevice, "%02X", device);  // to

	printf("MQTTR %02x t:%02x c:%02x v:%d\n",busdata->busid,busdata->bustype,busdata->buscommand,busdata->busvalue);


// ================================ INTERPRETAZIONE STATO ===========================================

  // ----------------------------------pubblicazione stati termostato SCS ----------------------------------
	if (busdata->bustype == 15)	// 0x0F = termostato  
	{	      
		devtype = 15;
		strcpy(topic, SENSOR_TEMP_STATE);
		strcat(topic, nomeDevice);
      	sprintf(payload, "%03u", busdata->busvalue);
		payload[4]= 0;
		payload[3]= payload[2];
		payload[2]= ',';		// virtual decimal point
		publish(topic, payload, 1);
		printf("%s -> %s\n",topic,payload);
	}
	else
  // ----------------------------------pubblicazione stati GENERIC device SCS (to & from)------------------------
	if (busdata->bustype == 11)  
	{	          // device generic censito
		devtype = 11;
		strcpy(topic, GENERIC_TO);
		strcat(topic, nomeDevice);
      	sprintf(payload, "%02X%02X%02X", busdata->busfrom, busdata->busrequest, busdata->buscommand);
		publish(topic, payload, 1);
		printf("%s -> %s\n",topic,payload);
	}
	else
	if (busdata->bustype == 12)  
	{
		devtype = 12;
		sprintf(nomeDevice, "%02X", busdata->busfrom);  // to
		strcpy(topic, GENERIC_FROM);
		strcat(topic, nomeDevice);
		sprintf(payload, "%02X%02X%02X", busdata->busid, busdata->busrequest, busdata->buscommand);
		publish(topic, payload, 1);
		printf("%s -> %s\n",topic,payload);
    }
	else
	if ((busdata->bustype == 9) && (action > 0x7F))
	{
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F3] u <address> <position%>
		devtype = 9;
		sprintf(payload, "%03u", busdata->busvalue);     // position
		strcpy(topic, COVERPCT_STATE);
		strcat(topic, nomeDevice);
		publish(topic, payload, 1);
		printf("%s -> %s\n",topic,payload);
    }
	else
	if ((busdata->bustype == 3) && (action > 0x7F))
	{
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F3] u <address> <position%>
		devtype = 3;
		sprintf(payload, "%03u", busdata->busvalue);     // position
		strcpy(topic, BRIGHT_STATE);
		strcat(topic, nomeDevice);
		publish(topic, payload, 1);
		printf("%s -> %s\n",topic,payload);
    }
	else

	if (busdata->busrequest == 0x12)  // <-comando----------------------------
	// SCS ridotto [0xF5] [y] 32 00 12 01
	{
		switch (action)
		{
		  case 0:
			strcpy(payload,"ON");
			strcpy(topic, SWITCH_STATE);
			devtype = 1;
			break;
		  case 1:
			strcpy(payload,"OFF");
			strcpy(topic, SWITCH_STATE);
			devtype = 1;
			break;
		  case 2:
		  case 3:
		  case 4:
			sprintf(payload, "%02u", action);
			strcpy(topic, BRIGHT_STATE);
			devtype = 3;
			break;
		  case 8:
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"open");
			else
				strcpy(payload,"OFF");
			strcpy(topic, COVER_STATE);
			devtype = 8;
			break;
		  case 9:
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"closed");
			else
				strcpy(payload,"ON");
			strcpy(topic, COVER_STATE);
			devtype = 8;
			break;
		  case 0x0A:
			strcpy(payload,"STOP");
			strcpy(topic, COVER_STATE);
			devtype = 8;
			break;

		  default:
			if ((action & 0x0F) == 0x0D) // da 0x1D a 0x9D
			{
				devtype = 3;
				action >>= 4;
				action *= 10;	// percentuale 10-90

				int pct = action;
				pct *= 255;      // da 0 a 25500
				pct /= 100;      // da 0 a 100
				action = (char) pct;
			}
			break;
		}

	    if ((device != 0) && (devtype != 0))
	    { // device valido 
			strcat(topic, nomeDevice);
			publish(topic, payload, 1);
			printf("%s -> %s\n",topic,payload);
	    }       // device valido
	} // <-rx_buffer[4] == 0x12---------------------comando----------------------------
	// ----------------------------------------------------------------------------------------------------
	else
		printf("no action %02x   request %02x\n",action,busdata->busrequest);
	
	
	// ----------------------------------------------------------------------------------------------------
	if (devtype == 0)
	{
		strcpy(topic, "NO_TOPIC");
		sprintf(payload, "to %02X, from %02X, req %02X, cmd %02X, val %d", busdata->busid, busdata->busfrom, busdata->busrequest, busdata->buscommand, busdata->busvalue);
		publish(topic, payload, 0); 
		printf("%s -> %s\n",topic,payload);
	} 
//	printf("end mqttrequest\n");
	return devtype;
}
// ===================================================================================


// ===================================================================================
char MQTTcommand(bus_scs_queue * busdata)
// ===================================================================================
// richiesta di pubblicazione comando in mqtt
// input: dati del messaggio e tipo dispositivo censito
// output: topic di comando
// return: tipo dispositivo individuato
// ===================================================================================
{
// ---------------------------------------------------------------------------------------------------------------------------------
	if (mqttopen != 3)	return 0xFF;

 // START pubblicazione comando device        [0xF5] [y] 32 00 12 01
	char device = 0;
	char devtype = 0;
	char action;
	char topic[32];
	char payload[64];
	char nomeDevice[4];

	device = busdata->busid;
	action = busdata->buscommand;

	sprintf(nomeDevice, "%02X", device);  // to

	printf("MQTTC %02x t:%02x c:%02x v:%d\n",busdata->busid,busdata->bustype,busdata->buscommand,busdata->busvalue);


// ================================ INTERPRETAZIONE COMANDO/STATO ===========================================

  // ----------------------------------pubblicazione stati GENERIC device SCS (to & from)------------------------
	if (busdata->bustype == 11)  
	{	          // device generic censito
		devtype = 11;
		strcpy(topic, GENERIC_SET);
		strcat(topic, nomeDevice);
      	sprintf(payload, "%02X%02X%02X", busdata->busfrom, busdata->busrequest, busdata->buscommand);
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
	}
	else
	if (busdata->bustype == 12)  
	{
		devtype = 12;
		sprintf(nomeDevice, "%02X", busdata->busfrom);  // to
		strcpy(topic, GENERIC_SET);
		strcat(topic, nomeDevice);
		sprintf(payload, "%02X%02X%02X", busdata->busid, busdata->busrequest, busdata->buscommand);
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
    }
	else
	if ((busdata->bustype == 9) && (action > 0x7F))
	{
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F3] u <address> <position%>
		devtype = 9;
		sprintf(payload, "%03u", busdata->busvalue);     // position
		strcpy(topic, COVERPCT_SET);
		strcat(topic, nomeDevice);
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
    }
	else
	if ((busdata->bustype == 3) && (action > 0x7F))
	{
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F3] u <address> <position%>
		devtype = 3;
		sprintf(payload, "%03u", busdata->busvalue);     // position
		strcpy(topic, BRIGHT_SET);
		strcat(topic, nomeDevice);
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
    }
	else

	if (busdata->busrequest == 0x12)  // <-comando----------------------------
	// SCS ridotto [0xF5] [y] 32 00 12 01
	{
		switch (action)
		{
		  case 0:
			strcpy(payload,"ON");
			strcpy(topic, SWITCH_SET);
			devtype = 1;
			break;
		  case 1:
			strcpy(payload,"OFF");
			strcpy(topic, SWITCH_SET);
			devtype = 1;
			break;
		  case 2:
		  case 3:
		  case 4:
			sprintf(payload, "%02u", action);
			strcpy(topic, BRIGHT_SET);
			devtype = 3;
			break;
		  case 8:
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"OPEN");
			else
				strcpy(payload,"OFF");
			strcpy(topic, COVER_SET);
			devtype = 8;
			break;
		  case 9:
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"CLOSE");
			else
				strcpy(payload,"ON");
			strcpy(topic, COVER_SET);
			devtype = 8;
			break;
		  case 0x0A:
			strcpy(payload,"STOP");
			strcpy(topic, COVER_SET);
			devtype = 8;
			break;

		  default:
			if ((action & 0x0F) == 0x0D) // da 0x1D a 0x9D
			{
				devtype = 3;
				action >>= 4;
				action *= 10;	// percentuale 10-90

				int pct = action;
				pct *= 255;      // da 0 a 25500
				pct /= 100;      // da 0 a 100
				action = (char) pct;
			}
			break;
		}

	    if ((device != 0) && (devtype != 0))
	    { // device valido 
			strcat(topic, nomeDevice);
			publish(topic, payload, 0);
			printf("%s -> %s\n",topic,payload);
	    }       // device valido
	} // <-rx_buffer[4] == 0x12---------------------comando----------------------------
	// ----------------------------------------------------------------------------------------------------
	else
		printf("no action %02x   request %02x\n",action,busdata->busrequest);
	
	
	// ----------------------------------------------------------------------------------------------------
	if (devtype == 0)
	{
		strcpy(topic, "NO_TOPIC");
		sprintf(payload, "to %02X, from %02X, req %02X, cmd %02X, val %d", busdata->busid, busdata->busfrom, busdata->busrequest, busdata->buscommand, busdata->busvalue);
		publish(topic, payload, 0); 
		printf("%s -> %s\n",topic,payload);
	} 
//	printf("end mqttrequest\n");
	return devtype;
}
// ===================================================================================
