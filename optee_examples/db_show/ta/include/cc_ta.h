#ifndef CC_TA_H
#define CC_TA_H

/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */
#define CC_TA_UUID \
	{ 0xcb917dde, 0x3abc, 0x4452, \
		{ 0x93, 0xbc, 0x02, 0x33, 0xd4, 0x8d, 0xbe, 0x22} }

/* The function IDs implemented in this TA */
#define TA_CC_CMD_DATABASE_INIT     0
#define TA_CC_CMD_GET_IP     1
#define TA_CC_CMD_WRITE_IP	2
#define TA_CC_CMD_MSG_IN	3
#define TA_CC_CMD_MSG_OUT	4
#define TA_CC_CMD_SQL_EXEC	5


#define DATABASE "/home/pi/database/sensor_data.db"

#endif /*CC_TA_H*/

