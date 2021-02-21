/* ---------------------------------------------------------------------------
 * modulo per la gestione di mqtt  con scsgate
 * ---------------------------------------------------------------------------
*/
#ifndef _SCS_MQTT_DISC_H
#define _SCS_MQTT_DISC_H

#define CLIENTID    "Raspy_SCS_1"
#define QOS         1
#define TIMEOUT     10L	// millisecondi

// =============================================================================================
#define		_MODO "SCS"
#define		_modo "scs"
#define		ID_MY "interfaccia scs"
#define MYPFX _modo
// =============================================================================================
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
#define NEW_SWITCH_TOPIC "homeassistant/switch/"
#define NEW_LIGHT_TOPIC  "homeassistant/light/"
#define NEW_COVER_TOPIC  "homeassistant/cover/"
#define NEW_CONFIG_TOPIC "/config"
#define NEW_COVERPCT_TOPIC  "homeassistant/cover/"
#define NEW_SENSOR_TOPIC  "homeassistant/sensor/"
#define NEW_GENERIC_TOPIC "homeassistant/generic/"
#define NEW_DEVICE_NAME  "{\"name\": \""
#define NEW_SWITCH_SET   "\",\"command_topic\": \"" SWITCH_SET
#define NEW_SWITCH_STATE "\",\"state_topic\": \"" SWITCH_STATE
#define NEW_LIGHT_SET    NEW_SWITCH_SET
#define NEW_LIGHT_STATE  NEW_SWITCH_STATE
#define NEW_BRIGHT_SET   "\",\"brightness_command_topic\": \"" BRIGHT_SET
#define NEW_BRIGHT_STATE "\",\"brightness_state_topic\": \"" BRIGHT_STATE
#define NEW_COVER_SET    "\",\"command_topic\": \"" COVER_SET
#define NEW_COVER_STATE  "\",\"state_topic\": \"" COVER_STATE
#define NEW_COVERPCT_SET    "\",\"set_position_topic\": \"" COVERPCT_SET
#define NEW_COVERPCT_STATE  "\",\"position_topic\": \"" COVERPCT_STATE
#define NEW_SENSOR_TEMP_STATE "\",\"state_topic\": \"" SENSOR_TEMP_STATE
#define NEW_SENSOR_TEMP_UNIT "\",\"unit_of_measurement\": \"°C"
#define NEW_SENSOR_HUMI_STATE "\",\"state_topic\": \"" SENSOR_HUMI_STATE
#define NEW_SENSOR_HUMI_UNIT "\",\"unit_of_measurement\": \"%"
#define NEW_SENSOR_PRES_STATE "\",\"state_topic\": \"" SENSOR_PRES_STATE
#define NEW_SENSOR_PRES_UNIT "\",\"unit_of_measurement\": \"mBar"
#define NEW_ALARM_SWITCH_STATE "\",\"state_topic\": \"" ALARM_SWITCH_STATE    // armed or disarmed
#define NEW_ALARM_ZONE_STATE "\",\"state_topic\": \"" ALARM_ZONE_STATE
// generic:
//  scs/generic/set/<to>      <from><type><cmd>   command to send
//  scs/generic/from/<from>   <to><type><cmd>	  received
//  scs/generic/to/<to>       <from><type><cmd>   received
#define NEW_GENERIC_SET   "\",\"command_topic\": \"" GENERIC_SET
#define NEW_GENERIC_FROM  "\",\"from_topic\": \"" GENERIC_FROM
#define NEW_GENERIC_TO    "\",\"to_topic\": \"" GENERIC_FROM

#define NEW_DEVICE_END   "\"}"
// =============================================================================================
int  MQTTconnect(char * broker, char * user, char * password, char verbose);
void MQTTverify(void);
void MQTTstop(void);
char MQTTpublishDiscover(char * nomeDevice, char * descrizione, char tipo);
// ===================================================================================
#endif
