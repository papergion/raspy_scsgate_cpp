/* ---------------------------------------------------------------------------
 * modulo per la gestione di mqtt  con scsgate
 * ---------------------------------------------------------------------------
*/
//
//
#ifndef _SCS_MQTT_H
#define _SCS_MQTT_H

#define CLIENTID    "Raspy_SCS_1"
#define QOS         1
#define TIMEOUT     20L	// millisecondi

// =============================================================================================
#define		_MODO "SCS"
#define		_modo "scs"
#define		ID_MY "interfaccia scs"
#define MYPFX _modo
#define     SWITCH_SET   MYPFX "/switch/set/"
#define     SWITCH_STATE MYPFX "/switch/state/"
#define     BRIGHT_SET   MYPFX "/switch/setlevel/"
#define     BRIGHT_STATE MYPFX "/switch/value/"
#define     COVER_SET    MYPFX "/cover/set/"
#define     COVER_STATE  MYPFX "/cover/state/"
#define     COVERPCT_SET    MYPFX "/cover/setposition/"
#define     COVERPCT_STATE  MYPFX "/cover/value/"
#define     SENSOR_TEMP_STATE MYPFX "/sensor/temp/state/"
#define     SENSOR_HUMI_STATE MYPFX "/sensor/humi/state/"
#define     SENSOR_PRES_STATE MYPFX "/sensor/pres/state/"

// generic:
//  scs/generic/set/<to>      <from><type><cmd>   command to send
//  scs/generic/from/<from>   <to><type><cmd>	  received
//  scs/generic/to/<to>       <from><type><cmd>   received

#define     GENERIC_SET   MYPFX "/generic/set/"
#define     GENERIC_FROM  MYPFX "/generic/from/"
#define     GENERIC_TO    MYPFX "/generic/to/"
// =============================================================================================
#define		SUBSCRIBE1 MYPFX "/+/set/+"
#define		SUBSCRIBE2 MYPFX "/+/setlevel/+"
#define		SUBSCRIBE3 MYPFX "/+/setposition/+"
#define		SUBSCRIBE4 MYPFX "/+/state/+"
// =============================================================================================
typedef struct {
    char topic[24];
    char payload[8];
    char retain;
} publish_queue;
// =============================================================================================
typedef struct {
    char busid;
    char bustype;
    char buscommand;
    char busvalue;
    char busfrom;
    char busrequest;
} bus_scs_queue;
// =============================================================================================
int MQTTconnect(char * broker, char * user, char * password, char verbose);
void MQTTverify(void);
void MQTTstop(void);
char MQTTrequest(bus_scs_queue * busdata);
char MQTTcommand(bus_scs_queue * busdata);
// ===================================================================================
void publish(char * pTopic, char * pPayload, int retain);
void publish_dequeue(void);
// ===================================================================================
#endif