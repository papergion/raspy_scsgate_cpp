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
#include "scs_mqtt_disc.h"
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
//static char domoticMode = 'H';
static char mqttAddress[32];
// =============================================================================================
MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;
// =============================================================================================
void publish(char * pTopic, char * pPayload, int retain);
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
//    char* payLoad;

	if (mqVerbose>1) printf("Message arrived - topic: %s   message: ", topicName);

    payloadptr = (char*)message->payload;
//    payLoad = (char*)message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        if (mqVerbose) putchar(*payloadptr);
        payloadptr++;
    }
	*payloadptr = 0;
    if (mqVerbose) putchar('\n');

//	processMessage(topicName, payLoad);

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

	mqVerbose = verbose;
	strcpy(mqttAddress, broker);

    if (mqVerbose) fprintf(stderr,"MQTT connection at %s ...\n", mqttAddress);
	mqttopen = 1;

	MQTTClient_create(&client, mqttAddress, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);

	conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
	conn_opts.username = (const char *) user;
	conn_opts.password = (const char *) password;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        if (mqVerbose) fprintf(stderr,"MQTT - Failed to connect, return code %d\n", rc);
		return 0;
    }
	mqttopen = 2;

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

	if (mqVerbose>1) printf("topic: %s    payload: %s \n",pTopic,pPayload);

	pubmsg.payload = pPayload;
    pubmsg.payloadlen = (int)strlen(pPayload);
    pubmsg.qos = QOS;
    pubmsg.retained = retain;
    MQTTClient_publishMessage(client, pTopic, &pubmsg, &token);
    MQTTClient_waitForCompletion(client, token, TIMEOUT);
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
		MQTTClient_disconnect(client, 10000);
		MQTTClient_destroy(&client);
	}
}
// ===================================================================================
char MQTTpublishDiscover(char * addrDevice, char * nomeDevice, char tipo)
// ===================================================================================
// input: dati del dispositivo da censire
// ===================================================================================
{
	char rc = 1;
// ---------------------------------------------------------------------------------------------------------------------------------
	if (mqttopen != 3)	return 0xFF;

 // START pubblicazione stato device        [0xF5] [y] 32 00 12 01
	char topic[64];
	char payload[512];

//	printf("MQTTdisc %02x t:%02x - %s \n",*addrDevice,tipo,nomeDevice);

	switch (tipo)
	{
	case 1:  // define new device SWITCH <<<-----------------------------------------------
		strcpy(topic,NEW_SWITCH_TOPIC);
		strcat(topic,addrDevice);
		strcat(topic,NEW_CONFIG_TOPIC);

		strcpy(payload,NEW_DEVICE_NAME);
		strcat(payload,nomeDevice);
		strcat(payload,NEW_SWITCH_SET);
		strcat(payload,addrDevice);
		strcat(payload,NEW_SWITCH_STATE);
		strcat(payload,addrDevice);
		strcat(payload,NEW_DEVICE_END);
		break;

	case 3:  // define new light LIGHT <<<-----------------------------------------------
	case 4:  // define new light LIGHT <<<-----------------------------------------------
		strcpy(topic,NEW_LIGHT_TOPIC);
		strcat(topic,addrDevice);
		strcat(topic,NEW_CONFIG_TOPIC);

		strcpy(payload,NEW_DEVICE_NAME);
		strcat(payload,nomeDevice);
		strcat(payload,NEW_LIGHT_SET);
		strcat(payload,addrDevice);
		strcat(payload,NEW_LIGHT_STATE);
		strcat(payload,addrDevice);
		strcat(payload,NEW_BRIGHT_SET);
		strcat(payload,addrDevice);
		strcat(payload,NEW_BRIGHT_STATE);
		strcat(payload,addrDevice);
		strcat(payload,NEW_DEVICE_END);
		break;

	case 8:  // define new device COVER <<<-----------------------------------------------
	case 18:
		strcpy(topic,NEW_COVER_TOPIC);
		strcat(topic,addrDevice);
		strcat(topic,NEW_CONFIG_TOPIC);

		strcpy(payload,NEW_DEVICE_NAME);
		strcat(payload,nomeDevice);
		strcat(payload,NEW_COVER_SET);
		strcat(payload,addrDevice);
		strcat(payload,NEW_COVER_STATE);
		strcat(payload,addrDevice);
		strcat(payload,NEW_DEVICE_END);
		break;

	case 9:  // define new device COVERPCT <<<-----------------------------------------------
	case 19:
		strcpy(topic,NEW_COVERPCT_TOPIC);
		strcat(topic,addrDevice);
		strcat(topic,NEW_CONFIG_TOPIC);

		strcpy(payload,NEW_DEVICE_NAME);
		strcat(payload,nomeDevice);
		strcat(payload,NEW_COVER_SET);
		strcat(payload,addrDevice);
		strcat(payload,NEW_COVERPCT_SET);
		strcat(payload,addrDevice);
		strcat(payload,NEW_COVERPCT_STATE);
		strcat(payload,addrDevice);
		strcat(payload,NEW_DEVICE_END);
		break;

	case 11:  // define new device GENERIC <<<-----------------------------------------------
		strcpy(topic,NEW_GENERIC_TOPIC);
		strcat(topic,addrDevice);
		strcat(topic,NEW_CONFIG_TOPIC);

		strcpy(payload,NEW_DEVICE_NAME); 
		strcat(payload,nomeDevice);
		strcat(payload,NEW_GENERIC_SET);
		strcat(payload,addrDevice);
		strcat(payload,NEW_GENERIC_FROM);
		strcat(payload,addrDevice);
		strcat(payload,NEW_GENERIC_TO);
		strcat(payload,addrDevice);
		strcat(payload,NEW_DEVICE_END);
		break;

	case 15:  // define new device THERMO <<<-----------------------------------------------
		strcpy(topic,NEW_SENSOR_TOPIC);
		strcat(topic,addrDevice);
		strcat(topic,NEW_CONFIG_TOPIC);

		strcpy(payload,NEW_DEVICE_NAME); 
		strcat(payload,nomeDevice);
		strcat(payload,NEW_SENSOR_TEMP_STATE);
		strcat(payload,addrDevice);
		strcat(payload,NEW_SENSOR_TEMP_UNIT);
		strcat(payload,NEW_DEVICE_END);
		break;

	default:
		rc = 0;
		break;
	}

	if (topic[0])
	{
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
	}
	return rc;
}
// ===================================================================================
