#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* TA API: UUID and command IDs */
#include "bitdb_ca.h"

/* TEE resources */
struct tee_ctx {
	TEEC_Context ctx;
	TEEC_Session sess;
};

char sql_select_gps[4096] = "SELECT * FROM gps;";

void prepare_tee_session(struct tee_ctx *ctx)
{
	TEEC_UUID uuid = BITDB_TA_UUID;
	uint32_t err_origin;
	TEEC_Result res;

	/* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx->ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);
	
	/* open a session with the TA */
	res = TEEC_OpenSession(&ctx->ctx, &ctx->sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);
}

void terminate_tee_seesion(struct tee_ctx *ctx)
{
	TEEC_CloseSession(&ctx->sess);
	TEEC_FinalizeContext(&ctx->ctx);
}

/**
 * @brief      { function_description }
 *
 * @param      ctx       The context
 * @param      data      The data contains SQL statement
 * @param[in]  data_len  The data length
 *
 * @return     The teec result.
 */
TEEC_Result cmd_selectgps(struct tee_ctx *ctx, char *data, size_t data_len) {
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;

	/* Clear the TEEC_Operation struct */
	memset(&op, 0, sizeof(op));
	/* Prepare the argument */
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					TEEC_NONE, TEEC_NONE);
	op.params[0].tmpref.size = data_len;
	op.params[0].tmpref.buffer = data;

	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_BITDB_CMD_SELECT_GPS,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS){
		printf("Invoking TA_BITDB_CMD_SELECT_GPS failed.\n");
		errx(1, "cmd_selectgps failed with code 0x%x origin 0x%x",
			res, err_origin);
	}

	return res;
}

TEEC_Result cmd_tpc_test(struct tee_ctx *ctx)
{
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;

	/* Clear the TEEC_Operation struct */
	memset(&op, 0, sizeof(op));
	/* Prepare the argument */
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_BITDB_CMD_TPC_TEST,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "cmd_test failed with code 0x%x origin 0x%x",
			res, err_origin);

	return res;
}

TEEC_Result cmd_mytest_init_db(struct tee_ctx *ctx)
{
    TEEC_Operation op;
    uint32_t err_origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));

    /* init_db 无输入参数 */
    op.paramTypes = TEEC_PARAM_TYPES(
                        TEEC_NONE,
                        TEEC_NONE,
                        TEEC_NONE,
                        TEEC_NONE);

    res = TEEC_InvokeCommand(&ctx->sess,
                             TA_BITDB_CMD_MYTEST_INIT_DB,
                             &op,
                             &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("Invoke TA_BITDB_CMD_MYTEST_INIT_DB failed\n");
        errx(1, "cmd_mytest_init_db failed code 0x%x origin 0x%x",
             res, err_origin);
    }

    printf("init_db executed successfully.\n");
    return res;
}

static void random_string(char *buf, int len)
{
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < len - 1; i++)
        buf[i] = charset[rand() % (sizeof(charset) - 1)];
    buf[len - 1] = '\0';
}
TEEC_Result cmd_testselect(struct tee_ctx *ctx, char *sql, size_t sql_len, uint32_t *time_cost)
{
    TEEC_Operation op;
    uint32_t err_origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));

    op.paramTypes = TEEC_PARAM_TYPES(
                        TEEC_MEMREF_TEMP_INPUT,  /* SQL */
                        TEEC_VALUE_OUTPUT,       /* time cost */
                        TEEC_NONE,
                        TEEC_NONE);

    op.params[0].tmpref.buffer = sql;
    op.params[0].tmpref.size   = sql_len;

    res = TEEC_InvokeCommand(&ctx->sess,
                             TA_BITDB_CMD_MYTEST_SELECT,
                             &op, &err_origin);

    if (res != TEEC_SUCCESS) {
        printf("Invoke TA_BITDB_CMD_MYTEST_SELECT failed\n");
        errx(1, "cmd_testselect failed code 0x%x origin 0x%x",
             res, err_origin);
    }

    *time_cost = op.params[1].value.a;
    return res;
}
void test_select_exact_100k(struct tee_ctx *ctx)
{
    uint64_t sum = 0;
    uint32_t cost = 0;
    char sql[256];

    for (int i = 0; i < 10; i++) {
        snprintf(sql, sizeof(sql),
                 "SELECT * FROM students WHERE ID = %d;",
                 (i % 11) + 1);

        cmd_testselect(ctx, sql, strlen(sql) + 1, &cost);
        sum += cost;
    }

    printf("Exact SELECT avg = %.3f ms (100k ops)\n", sum / 100000.0);
}
void test_select_cond_100k(struct tee_ctx *ctx)
{
    uint64_t sum = 0;
    uint32_t cost = 0;
    char sql[256];

    for (int i = 0; i < 10; i++) {
        int age = 18 + (rand() % 10);

        snprintf(sql, sizeof(sql),
                 "SELECT * FROM students WHERE age >= %d;",
                 age);

        cmd_testselect(ctx, sql, strlen(sql) + 1, &cost);
        sum += cost;
    }

    printf("Conditional SELECT avg = %.3f ms (100k ops)\n", sum / 100000.0);
}
void test_select_like_100k(struct tee_ctx *ctx)
{
    uint64_t sum = 0;
    uint32_t cost = 0;
    char sql[256];
    char pattern[8];

    for (int i = 0; i < 10; i++) {

        random_string(pattern, 3);  // 随机 3 字符串

        snprintf(sql, sizeof(sql),
                 "SELECT * FROM students WHERE name LIKE '%%%s%%';",
                 pattern);

        cmd_testselect(ctx, sql, strlen(sql) + 1, &cost);
        sum += cost;
    }

    printf("LIKE SELECT avg = %.3f ms (100k ops)\n", sum / 100000.0);
}

TEEC_Result cmd_testinsert(struct tee_ctx *ctx, char *sql, size_t sql_len, uint32_t *time_cost)
{
    TEEC_Operation op;
    uint32_t err_origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));

    op.paramTypes = TEEC_PARAM_TYPES(
                        TEEC_MEMREF_TEMP_INPUT,
                        TEEC_VALUE_OUTPUT,
                        TEEC_NONE,
                        TEEC_NONE);

    op.params[0].tmpref.buffer = sql;
    op.params[0].tmpref.size   = sql_len;

    res = TEEC_InvokeCommand(&ctx->sess,
                             TA_BITDB_CMD_MYTEST_INSERT,
                             &op, &err_origin);

    if (res != TEEC_SUCCESS) {
        printf("Invoke TA_BITDB_CMD_TEST_INSERT failed\n");
        errx(1, "cmd_testinsert failed code 0x%x origin 0x%x",
             res, err_origin);
    }

    *time_cost = op.params[1].value.a;   // ms
    return res;
}
void test_insert_100k(struct tee_ctx *ctx)
{
    uint64_t sum = 0;
    uint32_t cost = 0;
    char sql[256];
    char name[16], email[32], major[16];

    for (int i = 0; i < 10; i++) {

        random_string(name, 10);
        random_string(email, 10);
        random_string(major, 8);

        snprintf(sql, sizeof(sql),
                 "INSERT INTO students (name, age, gender, major, grade, "
                 "email, phone, address, enrollment_year) "
                 "VALUES ('%s', 20, 'M', '%s', 1, '%s@a.com', "
                 "'123456', 'addr', 2020);",
                 name, major, email);

        cmd_testinsert(ctx, sql, strlen(sql) + 1, &cost);
        sum += cost;
    }

    printf("INSERT avg cost = %.3f ms (100k ops)\n", sum / 100000.0);
}

TEEC_Result cmd_testupdate(struct tee_ctx *ctx, char *sql, size_t sql_len, uint32_t *time_cost)
{
    TEEC_Operation op;
    uint32_t err_origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));

    op.paramTypes = TEEC_PARAM_TYPES(
                        TEEC_MEMREF_TEMP_INPUT,
                        TEEC_VALUE_OUTPUT,
                        TEEC_NONE,
                        TEEC_NONE);

    op.params[0].tmpref.buffer = sql;
    op.params[0].tmpref.size   = sql_len;

    res = TEEC_InvokeCommand(&ctx->sess,
                             TA_BITDB_CMD_MYTEST_UPDATE,
                             &op, &err_origin);

    if (res != TEEC_SUCCESS) {
        printf("Invoke TA_BITDB_CMD_TEST_UPDATE failed\n");
        errx(1, "cmd_testupdate failed code 0x%x origin 0x%x",
             res, err_origin);
    }

    *time_cost = op.params[1].value.a;
    return res;
}
void test_update_100k(struct tee_ctx *ctx)
{
    uint64_t sum = 0;
    uint32_t cost = 0;
    char sql[256];
    char major[16];

    for (int i = 0; i < 10; i++) {

        random_string(major, 8);

        snprintf(sql, sizeof(sql),
                 "UPDATE students SET major='%s' WHERE ID=%d;",
                 major, (i % 10) + 1);

        cmd_testupdate(ctx, sql, strlen(sql) + 1, &cost);
        sum += cost;
    }

    printf("UPDATE avg cost = %.3f ms (100k ops)\n", sum / 100000.0);
}

TEEC_Result cmd_testdelete(struct tee_ctx *ctx, char *sql, size_t sql_len, uint32_t *time_cost)
{
    TEEC_Operation op;
    uint32_t err_origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));

    op.paramTypes = TEEC_PARAM_TYPES(
                        TEEC_MEMREF_TEMP_INPUT,
                        TEEC_VALUE_OUTPUT,
                        TEEC_NONE,
                        TEEC_NONE);

    op.params[0].tmpref.buffer = sql;
    op.params[0].tmpref.size   = sql_len;

    res = TEEC_InvokeCommand(&ctx->sess,
                             TA_BITDB_CMD_MYTEST_DELETE,
                             &op, &err_origin);

    if (res != TEEC_SUCCESS) {
        printf("Invoke TA_BITDB_CMD_TEST_DELETE failed\n");
        errx(1, "cmd_testdelete failed code 0x%x origin 0x%x",
             res, err_origin);
    }

    *time_cost = op.params[1].value.a;
    return res;
}
void test_delete_100k(struct tee_ctx *ctx)
{
    uint64_t sum = 0;
    uint32_t cost = 0;
    char sql[256];

    for (int i = 0; i < 10; i++) {

        snprintf(sql, sizeof(sql),
                 "DELETE FROM students WHERE ID=%d;",
                 (i % 10) + 1);

        cmd_testdelete(ctx, sql, strlen(sql) + 1, &cost);
        sum += cost;
    }

    printf("DELETE avg cost = %.3f ms (100k ops)\n", sum / 100000.0);
}

TEEC_Result cmd_testhash(struct tee_ctx *ctx, uint32_t *time_cost)
{
    TEEC_Operation op;
    uint32_t err_origin;
    TEEC_Result res;

    memset(&op, 0, sizeof(op));

    op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_OUTPUT,  /* 返回时间 */
        TEEC_NONE,
        TEEC_NONE,
        TEEC_NONE);

    res = TEEC_InvokeCommand(
            &ctx->sess,
            TA_BITDB_CMD_MYTEST_HASH,
            &op, &err_origin);

    if (res != TEEC_SUCCESS)
        errx(1, "cmd_testhash failed 0x%x origin 0x%x",
             res, err_origin);

    *time_cost = op.params[0].value.a;
    return res;
}


int main(int argc, char **argv)
{
	struct tee_ctx ctx;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s command\n", argv[0]);
		return(1);
	}

	prepare_tee_session(&ctx);

	if (memcmp(argv[1], "selectgps", 9) == 0) {
		cmd_selectgps(&ctx, sql_select_gps, strlen(sql_select_gps) + 1);
	}
	else if (memcmp(argv[1], "tpc", 3) == 0) {
		cmd_tpc_test(&ctx);
	}
	else if (memcmp(argv[1], "initdb", 3) == 0) {
		cmd_mytest_init_db(&ctx);
	}
	else if (memcmp(argv[1], "testselect", 10) == 0) {
    	test_select_exact_100k(&ctx);
    	test_select_cond_100k(&ctx);
    	test_select_like_100k(&ctx);
	}
	else if (memcmp(argv[1], "testinsert", 10) == 0) {
    	test_insert_100k(&ctx);
	}
	else if (memcmp(argv[1], "testupdate", 10) == 0) {
    	test_update_100k(&ctx);
	}
	else if (memcmp(argv[1], "testdelete", 10) == 0) {
    	test_delete_100k(&ctx);
	}
    else if (memcmp(argv[1], "testhash", 8) == 0) {
		uint32_t cost = 0;
        cmd_testhash(&ctx, &cost);
        printf("TPC-H DB hash time = %u ms\n", cost);
	}
	else {
		printf("Invalid command!\n");
	}

	terminate_tee_seesion(&ctx);

	return 0;
}
