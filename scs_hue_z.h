/* ---------------------------------------------------------------------------
 * modulo per la gestione di hue  con scsgate
 * ---------------------------------------------------------------------------
*/
//
//
#ifndef _SCS_HUE_H
#define _SCS_HUE_H

// =============================================================================================
typedef struct {
    char * name;
    char state;
    unsigned char value;	// percentuale da 0 a 255
    uint16_t busaddress;
} hue_device_t;


typedef struct {
    char hueid;
    char huecommand;
    char huevalue;		// percentuale da 0 a 255
    char pctvalue;		// percentuale da 0 a 100
} hue_scs_queue;

// =============================================================================================
void setState(char id, char state, char value);
char addDevice(const char * device_name, unsigned char initvalue, uint16_t busaddr);
char addDevice(const char * device_name, uint16_t busaddr);
int  HUE_start(char verb);
void HUE_stop(void);
// ===================================================================================
#endif