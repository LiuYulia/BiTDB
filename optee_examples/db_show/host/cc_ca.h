#ifndef CC_CA_H
#define CC_CA_H

#define CC_TA_UUID { 0xcb917dde, 0x3abc, 0x4452, \
	{0x93, 0xbc, 0x02, 0x33, 0xd4, 0x8d, 0xbe, 0x22}}

#define TA_CC_CMD_DATABASE_INIT     0
#define TA_CC_CMD_GET_IP     1
#define TA_CC_CMD_WRITE_IP	2
#define TA_CC_CMD_MSG_IN	3
#define TA_CC_CMD_MSG_OUT	4
#define TA_CC_CMD_SQL_EXEC	5


#define SQL_PORT 8083
#define MSG_OUT_PORT 8081
#define MSG_IN_PORT 8082
#define MAX_EVENTS 10
#define IP "192.168.50.251"
#define DATABASE "/home/pi/database/sensor_data.db"
#define MAX_DATABASE_SIZE 1*1024


#endif
