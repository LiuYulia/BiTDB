#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "./include/bitdb_ta.h"
#include <string.h>
#include <utee_syscalls.h>
#include "./include/sqlite3.h"
#include "./include/tpc-test.h"
#include <utee_defines.h>
#include <tee_api.h>

static TEE_Time start, end;
char* DB_GPSNAME = "./aes_cbc_128_plain.db";
char* DB_TPC_NAME = "./TPC-H-small.db";
char* DB_TEST="./Test.db";
static int gps_rec_num = 0;
static int rec_num = 0;
int write_time = 0;
int read_time = 0;
int lseek_time = 0;
int strcspn_time = 0;
#define HASH_BUF_SZ 4096

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

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
	rec_num ++;
	// for (int i=0; i < argc; i++) {
	//   IMSG("%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");	
	// }
	return 0;
}

// int current, highwater;
static int callback_gpsselect(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
	gps_rec_num ++;
	// for (int i=0; i < argc; i++) {
	//   IMSG("%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
	// }
	
	// sqlite3_status(SQLITE_STATUS_MEMORY_USED, &current, &highwater, 0);
	// IMSG("current = %d, highwater = %d", current, highwater);

	return 0;
}


static inline uint32_t tee_time_to_ms(TEE_Time t)
{
	return t.seconds * 1000 + t.millis;
}

static inline uint32_t get_delta_time_in_ms(TEE_Time start, TEE_Time stop)
{
	return tee_time_to_ms(stop) - tee_time_to_ms(start);
}

static void random_string(char *out, size_t len)
{
    /* len 不包括 '\0' */
    uint8_t buf[64];
    TEE_GenerateRandom(buf, len);
    for (size_t i = 0; i < len; i++)
        out[i] = 'a' + (buf[i] % 26);
    out[len] = '\0';
}

TEE_Result insert_110k_records(sqlite3 *db)
{
    TEE_Result tee_ret = TEE_SUCCESS;
    char sql[512];
    sqlite3_stmt *stmt = NULL;
    int rc;

    /* 使用事务以加速插入 */
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return TEE_ERROR_GENERIC;

    /* 预编译插入语句 */
    const char *insert_sql =
        "INSERT INTO students (name, age, gender, major, grade, email, phone, address, enrollment_year) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return TEE_ERROR_GENERIC;

    char name[16], gender[8], major[16], email[32], phone[16], address[32];

    for (int i = 0; i < 110; i++) {

        /* 生成随机字段 */
        random_string(name, 10);
        random_string(gender, 4);
        random_string(major, 10);
        random_string(email, 10);
        random_string(phone, 10);
        random_string(address, 10);

        /* age, grade, enrollment_year */
        uint32_t rnd;
        TEE_GenerateRandom(&rnd, sizeof(rnd));
        int age = 18 + (rnd % 10);                 /* 18~27 */
        int grade = 1 + (rnd % 4);                 /* 1~4年级 */
        int year = 2015 + (rnd % 10);              /* 2015~2024 */

        /* 绑定参数 */
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, age);
        sqlite3_bind_text(stmt, 3, gender, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, major, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, grade);
        sqlite3_bind_text(stmt, 6, email, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, phone, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, address, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 9, year);

        /* 执行插入 */
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return TEE_ERROR_GENERIC;
        }

        /* 重置以便下次执行 */
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);

    /* 提交事务 */
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    return TEE_SUCCESS;
}

static TEE_Result cmd_selectgps(uint32_t param_types,
	TEE_Param params[4]){
	
	TEE_Time t_start={};
	TEE_Time t_end={};
	
	const uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	char* data = NULL;
	uint32_t data_len = 0;
	sqlite3 *db = NULL;
	int rc;
	char *zErrMsg = NULL;

	DMSG("has been called");

	sqlite3_hard_heap_limit64(10 * 1024 * 1024);

	// void *heap_mem = malloc(8 * 1024 * 1024);
	// sqlite3_config(SQLITE_CONFIG_HEAP, heap_mem, 8 * 1024 * 1024, 1024);
	// DMSG("addr: %d\n", heap_mem);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	data = params[0].memref.buffer;
	data_len = params[0].memref.size;

	rc = sqlite3_open(DB_GPSNAME, &db);
	if(rc){
		IMSG("Can't create database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return TEE_SUCCESS;
	}
	DMSG("Database %s is opened.\n", DB_GPSNAME);

	DMSG("DATA: %s, DATA_LEN: %d.\n", data, data_len);

	TEE_GetSystemTime(&t_start);
	rc = sqlite3_exec(db, data, callback_gpsselect, 0, &zErrMsg);
	if(rc != SQLITE_OK){
		IMSG("SQL error: %s\n", zErrMsg);
		IMSG("rc = %d\n", rc);
		sqlite3_close(db);
		sqlite3_free(zErrMsg);
		return TEE_SUCCESS;
	}
	
	// MSG("write: %d ms, read: %d ms, lseek: %d ms, strcspn: %d ms.\n", write_time, read_time, lseek_time, strcspn_time);
	write_time = 0;
	read_time = 0;
	lseek_time = 0;
	strcspn_time = 0;

	TEE_GetSystemTime(&t_end);
	int cost = get_delta_time_in_ms(t_start, t_end);


	MSG("The total number of gps table's records: %d, timecost: %d\n", gps_rec_num, cost);
	gps_rec_num = 0;
	sqlite3_close(db);

	return TEE_SUCCESS;
}

static TEE_Result cmd_tpc_test(uint32_t param_types,
	TEE_Param params[4])
{
	const uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	DMSG("has been called");

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	
	sqlite3 *db = NULL;
	char* err_msg = NULL;

	sqlite3_hard_heap_limit64(10 * 1024 * 1024);

    MSG("qr, rec_num, time\n");
    // for (int j = 0; j < 1; j++) {
    for (int i = 0; i < 22; i++) {
		int rc = sqlite3_open(DB_TPC_NAME, &db);
	    if (rc != SQLITE_OK)
	    {
		    MSG("Can't open database: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
		    return TEE_SUCCESS;
	    }

        rec_num = 0;
        char * sql = qr[i];
		TEE_GetREETime(&start);

        if (i == 14)
        {
			// rc = sqlite3_exec(db, q15drop, callback, 0, &err_msg);
            rc = sqlite3_exec(db, q15view, callback, 0, &err_msg);
			if(rc != SQLITE_OK) {
				MSG("SQL error: %s\n", err_msg);
				MSG("rc = %d\n", rc);
				sqlite3_close(db);
				sqlite3_free(err_msg);
				return TEE_SUCCESS;
			}
        }
		// if (i == 12)
		// {
		// 	rc = sqlite3_exec(db, q13drop, callback, 0, &err_msg);
		// 	// rc = sqlite3_exec(db, q13view, callback, 0, &err_msg);
		// 	if(rc != SQLITE_OK) {
		// 		MSG("SQL error: %s\n", err_msg);
		// 		MSG("rc = %d\n", rc);
		// 		sqlite3_close(db);
		// 		sqlite3_free(err_msg);
		// 		return TEE_SUCCESS;
		// 	}
		// }

        rc = sqlite3_exec(db, sql, callback, 0, &err_msg);
		if(rc != SQLITE_OK) {
			MSG("SQL error: %s\n", err_msg);
			MSG("rc = %d\n", rc);
			sqlite3_close(db);
			sqlite3_free(err_msg);
			return TEE_SUCCESS;
		}
		TEE_GetREETime(&end);
        MSG("%d, %d, %d\n", 
			i + 1, rec_num, (end.seconds - start.seconds) * 1000 + (end.millis - start.millis));
		// IMSG("write: %d ms, read: %d ms, lseek: %d ms, strcspn: %d ms.\n", write_time, read_time, lseek_time, strcspn_time);
		write_time = 0;
		read_time = 0;
		lseek_time = 0;
		strcspn_time = 0;

        if (i == 14)
        {
			rc = sqlite3_exec(db, q15drop, callback, 0, &err_msg);
            // rc = sqlite3_exec(db, q15view, callback, 0, &err_msg);
			if(rc != SQLITE_OK) {
				MSG("SQL error: %s\n", err_msg);
				MSG("rc = %d\n", rc);
				sqlite3_close(db);
				sqlite3_free(err_msg);
				return TEE_SUCCESS;
			}
        }
		
		sqlite3_close(db);    
	}
    // }
    
	return TEE_SUCCESS;
}

static TEE_Result cmd_mytest_init_db(uint32_t param_types,
	TEE_Param params[4]){
	
	TEE_Time t_start={};
	TEE_Time t_end={};
	
	const uint32_t exp_param_types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

	char* data = NULL;
	uint32_t data_len = 0;
	sqlite3 *db = NULL;
	int rc;
	char *zErrMsg = NULL;
	char *errmsg = NULL;

	//DMSG("has been called");

	sqlite3_hard_heap_limit64(10 * 1024 * 1024);

	// void *heap_mem = malloc(8 * 1024 * 1024);
	// sqlite3_config(SQLITE_CONFIG_HEAP, heap_mem, 8 * 1024 * 1024, 1024);
	// DMSG("addr: %d\n", heap_mem);

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	//data = params[0].memref.buffer;
	//data_len = params[0].memref.size;


    /* 打开或创建数据库 */
    rc = sqlite3_open(DB_TEST, &db);
    if (rc != SQLITE_OK) {
        EMSG("Failed to open or create DB: %s", sqlite3_errmsg(db));
        return TEE_ERROR_GENERIC;
    }

    /* 建表 SQL */
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS students ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "age INTEGER,"
        "gender TEXT,"
        "major TEXT,"
        "grade INTEGER,"
        "email TEXT,"
        "phone TEXT,"
        "address TEXT,"
        "enrollment_year INTEGER"
        ");";

    /* 执行建表 */
    rc = sqlite3_exec(db, create_sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        EMSG("Create table error: %s", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    /* 调用你已有的插入函数
    TEE_Result ret = insert_110k_records(db);
    if (ret != TEE_SUCCESS) {
        EMSG("insert_110k_records failed");
        sqlite3_close(db);
        return ret;
    }
 */
    sqlite3_close(db);
    return TEE_SUCCESS;
}

static TEE_Result cmd_mytest_select(uint32_t param_types, TEE_Param params[4])
{
    TEE_Time t_start = {};
    TEE_Time t_end = {};

    const uint32_t exp_param_types =
        TEE_PARAM_TYPES(
            TEE_PARAM_TYPE_MEMREF_INPUT,   /* SQL 文本 */
            TEE_PARAM_TYPE_VALUE_OUTPUT,   /* 输出耗时 */
            TEE_PARAM_TYPE_NONE,
            TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    char *sql = params[0].memref.buffer;
    uint32_t sql_len = params[0].memref.size;

    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int rc;

    DMSG("SELECT SQL = %s len=%d", sql, sql_len);

    sqlite3_hard_heap_limit64(10 * 1024 * 1024);

    rc = sqlite3_open(DB_TEST, &db);
    if (rc) {
        IMSG("Open DB failed: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    rec_num = 0;

    TEE_GetSystemTime(&t_start);
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    TEE_GetSystemTime(&t_end);

    if (rc != SQLITE_OK) {
        IMSG("SELECT SQL error: %s", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    uint32_t cost = get_delta_time_in_ms(t_start, t_end);
    params[1].value.a = cost;

    sqlite3_close(db);
    return TEE_SUCCESS;
}

static TEE_Result cmd_mytest_insert(uint32_t param_types, TEE_Param params[4])
{
    TEE_Time t_start = {};
    TEE_Time t_end = {};

    const uint32_t exp_param_types =
        TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,   /* SQL 输入 */
                        TEE_PARAM_TYPE_VALUE_OUTPUT,   /* 返回耗时 */
                        TEE_PARAM_TYPE_NONE,
                        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    char *data = params[0].memref.buffer;
    uint32_t data_len = params[0].memref.size;

    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int rc;

    DMSG("INSERT SQL = %s, len = %d", data, data_len);

    /* 限制 SQLite 在 TEE 内的最大堆内存 */
    sqlite3_hard_heap_limit64(10 * 1024 * 1024);

    rc = sqlite3_open(DB_TEST, &db);
    if (rc) {
        IMSG("Open database failed: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    TEE_GetSystemTime(&t_start);
    rc = sqlite3_exec(db, data, NULL, NULL, &zErrMsg);
    TEE_GetSystemTime(&t_end);

    if (rc != SQLITE_OK) {
        IMSG("INSERT SQL error: %s", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    uint32_t cost = get_delta_time_in_ms(t_start, t_end);
    params[1].value.a = cost;   /* 返回耗时 */

    sqlite3_close(db);
    return TEE_SUCCESS;
}
static TEE_Result cmd_mytest_update(uint32_t param_types, TEE_Param params[4])
{
    TEE_Time t_start = {};
    TEE_Time t_end = {};

    const uint32_t exp_param_types =
        TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                        TEE_PARAM_TYPE_VALUE_OUTPUT,
                        TEE_PARAM_TYPE_NONE,
                        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    char *sql = params[0].memref.buffer;
    uint32_t sql_len = params[0].memref.size;

    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int rc;

    DMSG("UPDATE SQL = %s, len = %d", sql, sql_len);

    sqlite3_hard_heap_limit64(10 * 1024 * 1024);

    rc = sqlite3_open(DB_TEST, &db);
    if (rc) {
        IMSG("Open database failed: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    TEE_GetSystemTime(&t_start);
    rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);
    TEE_GetSystemTime(&t_end);

    if (rc != SQLITE_OK) {
        IMSG("UPDATE SQL error: %s", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    uint32_t cost = get_delta_time_in_ms(t_start, t_end);
    params[1].value.a = cost; /* 记录耗时 */

    sqlite3_close(db);
    return TEE_SUCCESS;
}
static TEE_Result cmd_mytest_delete(uint32_t param_types, TEE_Param params[4])
{
    TEE_Time t_start = {};
    TEE_Time t_end = {};

    const uint32_t exp_param_types =
        TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                        TEE_PARAM_TYPE_VALUE_OUTPUT,
                        TEE_PARAM_TYPE_NONE,
                        TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    char *data = params[0].memref.buffer;
    uint32_t data_len = params[0].memref.size;

    sqlite3 *db = NULL;
    char *zErrMsg = NULL;
    int rc;

    DMSG("DELETE SQL = %s, len = %d", data, data_len);

    sqlite3_hard_heap_limit64(10 * 1024 * 1024);

    rc = sqlite3_open(DB_TEST, &db);
    if (rc) {
        IMSG("Open database failed: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    TEE_GetSystemTime(&t_start);
    rc = sqlite3_exec(db, data, NULL, NULL, &zErrMsg);
    TEE_GetSystemTime(&t_end);

    if (rc != SQLITE_OK) {
        IMSG("DELETE SQL error: %s", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return TEE_ERROR_GENERIC;
    }

    uint32_t cost = get_delta_time_in_ms(t_start, t_end);
    params[1].value.a = cost;

    sqlite3_close(db);
    return TEE_SUCCESS;
}
static TEE_Result cmd_mytest_hash(uint32_t param_types, TEE_Param params[4])
{
    const uint32_t exp_param_types =
        TEE_PARAM_TYPES(
            TEE_PARAM_TYPE_VALUE_OUTPUT,  /* 返回时间 */
            TEE_PARAM_TYPE_NONE,
            TEE_PARAM_TYPE_NONE,
            TEE_PARAM_TYPE_NONE);

    if (param_types != exp_param_types)
        return TEE_ERROR_BAD_PARAMETERS;

    TEE_ObjectHandle hobj = TEE_HANDLE_NULL;
    TEE_Result res;

    res = TEE_OpenPersistentObject(
            TEE_STORAGE_PRIVATE,
            DB_TEST, strlen(DB_TEST),
            TEE_DATA_FLAG_ACCESS_READ,
            &hobj);

    if (res != TEE_SUCCESS) {
        IMSG("Open DB_TEST failed (%x)", res);
        return res;
    }

    uint8_t *buf = TEE_Malloc(HASH_BUF_SZ, 0);
    if (!buf) {
        TEE_CloseObject(hobj);
        return TEE_ERROR_OUT_OF_MEMORY;
    }

    TEE_OperationHandle op;
    uint8_t hash[32];
    uint32_t hash_len = 32;

    res = TEE_AllocateOperation(
            &op, TEE_ALG_SHA256, TEE_MODE_DIGEST, 0);
    if (res != TEE_SUCCESS) {
        TEE_Free(buf);
        TEE_CloseObject(hobj);
        return res;
    }

    TEE_Time t_start, t_end;
    TEE_GetSystemTime(&t_start);

    /* 循环读取 Secure Storage 文件并做 hash */
    for (;;) {
        uint32_t read_sz = HASH_BUF_SZ;
        res = TEE_ReadObjectData(hobj, buf, HASH_BUF_SZ, &read_sz);

        if (res == TEE_ERROR_ITEM_NOT_FOUND || read_sz == 0)
            break;  /* 文件结束 */

        if (res != TEE_SUCCESS) {
            IMSG("Read error %x", res);
            TEE_FreeOperation(op);
            TEE_Free(buf);
            TEE_CloseObject(hobj);
            return res;
        }

        TEE_DigestUpdate(op, buf, read_sz);
    }

    /* 完成 hash */
    res = TEE_DigestDoFinal(op, NULL, 0, hash, &hash_len);

    TEE_GetSystemTime(&t_end);

    TEE_FreeOperation(op);
    TEE_Free(buf);
    TEE_CloseObject(hobj);

    if (res != TEE_SUCCESS) {
        IMSG("DigestDoFinal error %x", res);
        return res;
    }

    uint32_t cost_ms =
        (t_end.seconds - t_start.seconds) * 1000 +
        (t_end.millis  - t_start.millis);

    params[0].value.a = cost_ms;

    DMSG("TEE internal DB hash cost = %u ms", cost_ms);

    return TEE_SUCCESS;
}

TEE_Result TA_InvokeCommandEntryPoint(void __unused *session,
				      uint32_t command,
				      uint32_t param_types,
				      TEE_Param params[4])
{
	switch (command) {
	case TA_BITDB_CMD_SELECT_GPS:
		return cmd_selectgps(param_types, params);
	case TA_BITDB_CMD_TPC_TEST:
		return cmd_tpc_test(param_types, params);
	case TA_BITDB_CMD_MYTEST_INIT_DB:
		return cmd_mytest_init_db(param_types, params);
	case TA_BITDB_CMD_MYTEST_SELECT:
		return cmd_mytest_select(param_types, params);
	case TA_BITDB_CMD_MYTEST_INSERT:
    	return cmd_mytest_insert(param_types, params);
	case TA_BITDB_CMD_MYTEST_UPDATE:
    	return cmd_mytest_update(param_types, params);
	case TA_BITDB_CMD_MYTEST_DELETE:
    	return cmd_mytest_delete(param_types, params);
    case TA_BITDB_CMD_MYTEST_HASH:
		return cmd_mytest_hash(param_types, params);    
	default:
		EMSG("Command ID 0x%x is not supported", command);
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
