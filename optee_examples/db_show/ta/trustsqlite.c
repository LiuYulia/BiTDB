#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "./include/cc_ta.h"
#include <string.h>
#include <utee_syscalls.h>
#include "./include/sqlite3.h"
#include <utee_defines.h>
#include <tee_api.h>
#include "./include/tzvfs_teeuser.h"
#include <stdio.h>
#include <tee_api.h>


#define TA_AES_MODE_ENCODE    1
#define TA_AES_MODE_DECODE    0
#define AES128_KEY_BIT_SIZE   128
#define AES128_KEY_BYTE_SIZE    (AES128_KEY_BIT_SIZE / 8)
// #define TA_AES_ALGO_ECB     0
// #define TA_AES_ALGO_CBC     1
// #define TA_AES_ALGO_CTR     2
#define AES_BLOCK_SIZE    16

#define AES256_KEY_BIT_SIZE   256
#define AES256_KEY_BYTE_SIZE    (AES256_KEY_BIT_SIZE / 8)

TEE_OperationHandle l_OperationHandle; 
uint32_t algo = TEE_ALG_AES_CBC_NOPAD;
uint32_t mode = TEE_MODE_ENCRYPT;
uint32_t key_size = AES128_KEY_BYTE_SIZE;
TEE_OperationHandle op_handle;
TEE_ObjectHandle key_handle;
TEE_Attribute attr = {0};

static TEE_Time start, end;
static int rec_num = 0;
char * create_sql = "CREATE TABLE SENSOR("  \
         "ID INTEGER PRIMARY KEY," \
         "TEMP           REAL," \
         "HUMLDLTY            REAL," \
         "SOUND       REAL," \
         "LIGHT         REAL, " \
		 "HIGH  REAL, " \
		 "PRESSURE REAL, " \
		 "TIME		TEXT);";

// char sql_result[4096];

TEE_Result TA_CreateEntryPoint(void)
{
	/* Nothing to do */
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	/* Nothing to do */
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t __unused param_types,
				    TEE_Param __unused params[4],
				    void __unused **session)
{
	/* Nothing to do */
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __unused *session)
{
	/* Nothing to do */
}


// char aeskey[] = {0x2FU, 0x58U, 0x7FU, 0xF0U, 0x43U, 0x83U, 0x95U, 0x3CU,
//                       0x1DU, 0x44U, 0x05U, 0x2BU, 0x61U, 0x49U, 0x17U, 0xF8U};
// char aesiv[] = {0x1DU, 0x44U, 0x05U, 0x2BU, 0x61U, 0x49U, 0x17U, 0xF8U,
//                      0x58U, 0xE0U, 0x90U, 0x43U, 0x84U, 0xA1U, 0xC1U, 0x75U};           

unsigned char aeskey[16] = "0123456789ABCDEF";

    // 初始向量
unsigned char aesiv[16] = "0123456789ABCDEF";

int aes_init(){

  // memset(key, 0xa5, sizeof(key));
  // memset(iv, 0, sizeof(iv));
  // sess = (struct aes_cipher*)malloc(sizeof(struct aes_cipher));
  MSG("AES initialization\n");
  TEE_Result res;


  if (op_handle != TEE_HANDLE_NULL)
        TEE_FreeOperation(op_handle);
  res = TEE_AllocateOperation(&op_handle,
            algo,
            mode,
            key_size * 8);

  if(res!=TEE_SUCCESS){
    return -1;
  }
  if (key_handle != TEE_HANDLE_NULL)
        TEE_FreeTransientObject(key_handle);
  res = TEE_AllocateTransientObject(TEE_TYPE_AES,
                      key_size * 8,
                      &key_handle);
  if (res != TEE_SUCCESS) {
    MSG("TEE_AllocateTransientObject failed, %x", res);
    return -1;
  }

  TEE_InitRefAttribute(&attr, TEE_ATTR_SECRET_VALUE, aeskey, key_size);
  res = TEE_PopulateTransientObject(key_handle, &attr, 1);
  if (res != TEE_SUCCESS) {
    MSG("TEE_PopulateTransientObject failed, %x", res);
    return -1;
  }
  // TEE_ResetOperation(op_handle);
  res = TEE_SetOperationKey(op_handle, key_handle);
  if (res != TEE_SUCCESS) {
    MSG("TEE_SetOperationKey failed %x", res);
    return -1;
  }
  return 0;
  // DMSG("111111111111");
  // TEE_CipherInit(op_handle, iv, key_size);
}

static int exec_callback(void *sql_result, int argc, char **argv, char **azColName)
{
	char temp[1024];
	if(rec_num==0){
		for (int i=0; i < argc; i++) {
			snprintf(sql_result, 1024, "%s|%s", sql_result, azColName[i]);
		}
		snprintf(sql_result, 1024, "%s|\n", sql_result);
		// MSG("%s", sql_result);
	}
	rec_num ++;
	for (int i=0; i < argc; i++) {
		snprintf(sql_result, 1024, "%s|%s", sql_result, argv[i] ? argv[i] : "NULL");
	}
	snprintf(sql_result, 1024, "%s|\n", sql_result);

	return 0;
}


static int msg_callback(void *sql_result, int argc, char **argv, char **azColName)
{
	// rec_num ++;

	char temp[4096] = "";
	char buff[1024] = "";
	char buff2[1024] = "";
	strcpy(buff, "{");
	for (int i=0; i < argc; i++) {
		if(strcmp(azColName[i], "TIME")==0)
			snprintf(buff2, 1024, "\"%s\": \"%s\"}, ", azColName[i], argv[i] ? argv[i] : "NULL");
		else if(strcmp(azColName[i], "IP")==0)
			snprintf(buff2, 1024, "\"%s\": \"%s\", ", azColName[i], argv[i] ? argv[i] : "NULL");
		else
			snprintf(buff2, 1024, "\"%s\": %s, ", azColName[i], argv[i] ? argv[i] : "NULL");
		// memcpy(buff+strlen(buff), buff2, strlen(buff2));
		snprintf(buff, 1024, "%s%s", buff, buff2);
	}

	// memcpy(buff+strlen(buff), "\0");
	// MSG("%s", sql_result);
	// MSG("%s", buff);
	// memcpy(sql_result+strlen(sql_result), buff, strlen(buff));
	snprintf(sql_result, 4096, "%s%s", sql_result, buff);
	// MSG("%s", sql_result);

	// strcpy(buff, "");
	// strcpy(buff2, "");
	return 0;
}

static TEE_Result cmd_database_init(uint32_t param_types,
	TEE_Param params[4])
{
	const uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	MSG("Database initialization");
	sqlite3 *db = NULL;
	int rc;
	char *zErrMsg = NULL;
	rc = sqlite3_open(DATABASE, &db);
	if(rc != SQLITE_OK){
		MSG("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return TEE_SUCCESS;
	}
	MSG("%s", create_sql);
	rc = sqlite3_exec(db, create_sql, NULL, 0, &zErrMsg);
	if(rc != SQLITE_OK){
		MSG("SQL error: %s\n", zErrMsg);
		sqlite3_close(db);
		sqlite3_free(zErrMsg);
		return TEE_SUCCESS;
	}
	char sql_result[4096] = "";
	rc = sqlite3_exec(db, "select *  from sqlite_master where type='table';", exec_callback, sql_result, &zErrMsg);
	if(rc != SQLITE_OK){
		MSG("SQL error: %s\n", zErrMsg);
		sqlite3_close(db);
		sqlite3_free(zErrMsg);
		return TEE_SUCCESS;
	}
	MSG("%s", sql_result);

	sqlite3_close(db);
	
	return TEE_SUCCESS;
}

int sql_count = -1;
static TEE_Result cmd_insert(uint32_t param_types,
	TEE_Param params[4])
{
	const uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	char *data;
	int data_len;

	data = params[0].memref.buffer;
	data_len = params[0].memref.size;

	MSG("Insert value\n");
	MSG("%s", data);
	sqlite3 *db = NULL;
	int rc;
	char *zErrMsg = NULL;
	rc = sqlite3_open(DATABASE, &db);
	if(rc != SQLITE_OK){
		MSG("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return TEE_SUCCESS;
	}

	if(sql_count==-1){
    	sqlite3_stmt *stmt;
		rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM SENSOR;", -1, &stmt, 0);
    	if (rc != SQLITE_OK) {
        	// fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        	sqlite3_finalize(stmt);
        	sqlite3_close(db);
			sqlite3_free(zErrMsg);
        	return TEE_SUCCESS;
    	}
		rc = sqlite3_step(stmt);
    	if (rc == SQLITE_ROW) {
        	sql_count = sqlite3_column_int(stmt, 0);
        	MSG("Total rows in the table: %d\n", sql_count);
    	}
		sqlite3_finalize(stmt);
	}
	if(sql_count>10000){
		rc = sqlite3_exec(db, "DELETE FROM SENSOR WHERE ID IN (SELECT ID FROM SENSOR ORDER BY ID LIMIT 100);", NULL, 0, &zErrMsg);
		if(rc != SQLITE_OK){
			MSG("SQL error: %s\n", zErrMsg);
			sqlite3_close(db);
			sqlite3_free(zErrMsg);
			return TEE_SUCCESS;
		}
		sql_count -= 100;
	}

	char sql_result[4096] = "";
	rc = sqlite3_exec(db, data, NULL, 0, &zErrMsg);
	if(rc != SQLITE_OK){
		MSG("SQL error: %s\n", zErrMsg);
		sqlite3_close(db);
		sqlite3_free(zErrMsg);
		return TEE_SUCCESS;
	}
	sql_count += 1;
	sqlite3_close(db);
	
	return TEE_SUCCESS;
}

void addPadding(unsigned char *input, size_t *inlen) {
    size_t padlen = AES_BLOCK_SIZE - (*inlen % AES_BLOCK_SIZE);
	MSG("Pkcs7 padding\n");
    for (size_t i = inlen; i < inlen + padlen; ++i) {
        input[i] = padlen;
		// MSG("%02x", input[i]);
    }
	*inlen += padlen;
}

static TEE_Result cmd_get_msg(uint32_t param_types,
	TEE_Param params[4], void *session)
{
	const uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	char *data, *sql;
	int data_len, sql_len;

	sql = params[0].memref.buffer;
	sql_len = params[0].memref.size;

	data = params[1].memref.buffer;
	data_len = params[1].memref.size;

	MSG("Get sensor data");
	sqlite3 *db = NULL;
	int rc;
	char *zErrMsg = NULL;
	rc = sqlite3_open(DATABASE, &db);
	if(rc != SQLITE_OK){
		MSG("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return TEE_SUCCESS;
	}

	// memset(sql_result, "", 4096);
	char sql_result[4096];
	strcpy(sql_result, "[");
	// MSG("%s", sql_result);
	rc = sqlite3_exec(db, sql, msg_callback, &sql_result, &zErrMsg);
	if(rc != SQLITE_OK){
		MSG("SQL error: %s\n", zErrMsg);
		sqlite3_close(db);
		sqlite3_free(zErrMsg);
		return TEE_SUCCESS;
	}

	// memcpy(sql_result+strlen(sql_result), "]", strlen("]"));
	snprintf(sql_result, 4096, "%s%s", sql_result, "]");
	// MSG("%s", sql_result);
	// int outsize = 0;

	// struct aes_cipher *sess;
	// sess = (struct aes_cipher *)session;

	// if (op_handle != TEE_HANDLE_NULL)
	// 	TEE_FreeOperation(op_handle);


	rc = aes_init();
	if(rc==-1)
		return TEE_ERROR_NOT_SUPPORTED;
  	TEE_CipherInit(op_handle, aesiv, AES_BLOCK_SIZE);

	// MSG("sql_result1 %d", strlen(sql_result));
	int re_len = strlen(sql_result);
	// MSG("sql_result1 %d", re_len);
	// addPadding((unsigned char *)sql_result, &re_len);
	size_t padlen = AES_BLOCK_SIZE - (re_len % AES_BLOCK_SIZE);
	// MSG("padlen %d ", padlen);
    for (size_t i = re_len; i < re_len + padlen; ++i) {
        sql_result[i] = padlen;
		// MSG("%02x", sql_result[i]);
    }
	re_len += padlen;

	// MSG("sql_result2 %d", re_len);
	
	rc = TEE_CipherDoFinal(op_handle, sql_result, re_len, data, &data_len);
 	if(rc != TEE_SUCCESS)
    {
        MSG("Do the final aes operation fail %x %d %d\n", rc, params[1].memref.size, strlen(params[1].memref.buffer));
        return rc;
    }
	// for(int i=0;i<re_len;i++)
	// 	MSG("%02x %c", sql_result[i], sql_result[i]);
	// MSG("data_len %d", data_len);
	params[2].value.a = data_len;

	// data_len = strlen(sql_result);
	// memcpy(data, sql_result, data_len);
	sqlite3_close(db);
	
	return TEE_SUCCESS;
}



static TEE_Result cmd_sql_exec(uint32_t param_types, TEE_Param params[4], void *session){
	const uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_MEMREF_OUTPUT);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	char *db_name, *sql, *data;
	int data_len, sql_len, db_len;

	db_name = params[0].memref.buffer;
	db_len = params[0].memref.size;

	sql = params[1].memref.buffer;
	sql_len = params[1].memref.size;

	data = params[3].memref.buffer;
	data_len = params[3].memref.size;

	MSG("Execute sql statement: %s", db_name, sql);

	sqlite3 *db = NULL;
	int rc;
	char *zErrMsg = NULL;
	rc = sqlite3_open(db_name, &db);
	if(rc != SQLITE_OK){
		MSG("Can't open database: %s\n", sqlite3_errmsg(db));
		goto err;
	}
	rec_num = 0;

	char sql_result[4096] = "";
	rc = sqlite3_exec(db, sql, exec_callback, sql_result, &zErrMsg);
	if(rc != SQLITE_OK){
		MSG("SQL error: %s\n", zErrMsg);

		memcpy(sql_result, zErrMsg, strlen(zErrMsg));
		// params[2].value.a = strlen(zErrMsg);
		// goto err;
	}

	int re_len = strlen(sql_result);

	// MSG("1111%s %d", sql_result, re_len);

	if(re_len>4096){
		memcpy(sql_result, "Select success, but too long to show", 37);
		// params[2].value.a = 37;
		// goto err;
	}
	if(re_len==0){
		memcpy(sql_result, "Success!", 9);
		re_len = 8;
		// MSG("2222%s %d", sql_result, re_len);

		// params[2].value.a = 8;
		// goto err;
	}

	rc = aes_init();
	if(rc==-1)
		return TEE_ERROR_NOT_SUPPORTED;
	TEE_CipherInit(op_handle, aesiv, AES_BLOCK_SIZE);

	size_t padlen = AES_BLOCK_SIZE - (re_len % AES_BLOCK_SIZE);
	// MSG("padlen %d ", padlen);
    for (size_t i = re_len; i < re_len + padlen; ++i) {
        sql_result[i] = padlen;
    }
	re_len += padlen;

	rc = TEE_CipherDoFinal(op_handle, sql_result, re_len, data, &data_len);
 	if(rc != TEE_SUCCESS)
    {
        MSG("Do the final aes operation fail %x %d %d\n", rc, params[3].memref.size, strlen(params[3].memref.buffer));
        return rc;
    }

	// data_len = strlen(sql_result);
	params[2].value.a = data_len;
	// memcpy(data, sql_result, data_len);

	sqlite3_close(db);
	
	return TEE_SUCCESS;

err:
	sqlite3_close(db);
	sqlite3_free(zErrMsg);
	return TEE_SUCCESS;
}



TEE_Result TA_InvokeCommandEntryPoint(void __unused *session,
				      uint32_t command,
				      uint32_t param_types,
				      TEE_Param params[4])
{

	switch (command) {
	case TA_CC_CMD_DATABASE_INIT:
		return cmd_database_init(param_types, params);
	case TA_CC_CMD_GET_IP:
	    return cmd_get_msg(param_types, params, session);
	case TA_CC_CMD_WRITE_IP:
		return cmd_insert(param_types, params);
	case TA_CC_CMD_MSG_IN:
		return cmd_insert(param_types, params);
	case TA_CC_CMD_MSG_OUT:
		return cmd_get_msg(param_types, params, session);
	case TA_CC_CMD_SQL_EXEC:
		return cmd_sql_exec(param_types, params, session);
	default:
		MSG("Command ID 0x%x is not supported", command);
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
