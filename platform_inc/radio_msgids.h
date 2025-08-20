#ifndef _RADIO_MSGIDS_H_
#define _RADIO_MSGIDS_H_

/*
** CCSDS V1 Command Message IDs (MID) must be 0x18xx
*/
#define RADIO_CMD_MID 0x18FC /* TODO: Change this for your app */

/*
** This MID is for commands telling the app to publish its telemetry message
*/
#define RADIO_REQ_HK_MID 0x18FD /* TODO: Change this for your app */

/*
** CCSDS V1 Telemetry Message IDs must be 0x08xx
*/
#define RADIO_HK_TLM_MID     0x08FC /* TODO: Change this for your app */
#define RADIO_DEVICE_TLM_MID 0x08FD /* TODO: Change this for your app */

#endif /* _RADIO_MSGIDS_H_ */
