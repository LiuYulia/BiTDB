#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include <pthread.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* TA API: UUID and command IDs */
#include "cc_ca.h"
/* TEE resources */
struct tee_ctx {
	TEEC_Context ctx;
	TEEC_Session sess;
};



void prepare_tee_session(struct tee_ctx *ctx)
{
	TEEC_UUID uuid = CC_TA_UUID;
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


TEEC_Result get_iP(const int fd, struct tee_ctx *ctx)
{
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;


	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT,
					TEEC_VALUE_INOUT, TEEC_NONE);

	char * sql = "SELECT * FROM IP_TABLE ORDER BY ID DESC LIMIT 5;";
	op.params[0].tmpref.buffer = sql;
	op.params[0].tmpref.size = strlen(sql);

	op.params[1].tmpref.size = 4096;
	char data[4096] = "";
	op.params[1].tmpref.buffer = data;
	op.params[2].value.a = 0;

	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_CC_CMD_GET_IP,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS){
		close(fd);
		errx(1, "database_init failed with code 0x%x origin 0x%x",
			res, err_origin);	
	}

	memcpy(data, op.params[1].tmpref.buffer, op.params[1].tmpref.size);

    // char response[8192] = "HTTP/1.1 200 OK\r\n"
    //                         "Content-Type: text/html\r\n"
    //                         "\r\n";

	char hex_data[4096]="";
	char temp[100]="";
	for(int i=0;i<op.params[2].value.a;i++){
		sprintf(temp, "%02x", data[i]);
		strcat(hex_data, temp);
	}

	char response[8192] = "";
	sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s", strlen(hex_data), hex_data);
	// strcat(response, data);

	// printf("%s\n", response);
    send(fd, response, strlen(response), 0);
	return res;
}

TEEC_Result write_ip(struct tee_ctx *ctx, char * ip, int port){
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;


	// 获取当前时间
    time_t current_time;
    time(&current_time);

    // 将时间格式化为字符串
    struct tm *time_info;
    time_info = localtime(&current_time);

    char time_str[24]= ""; // 为了包含毫秒，我们将缓冲区大小设置为 24
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    // 获取毫秒部分
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int milliseconds = tv.tv_usec / 1000;

    // 将毫秒部分添加到字符串中
    snprintf(time_str + 19, 5, ".%03d", milliseconds);

	char sql[500] = "";
	sprintf(sql, "INSERT INTO IP_TABLE (IP, PORT, TIME) VALUES (\"%s\", %d, \"%s\");",ip, port, time_str);
	printf("%s\n", sql);

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					TEEC_NONE, TEEC_NONE);

	op.params[0].tmpref.size = strlen(sql);
	op.params[0].tmpref.buffer = sql;
	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_CC_CMD_WRITE_IP,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "write_ip failed with code 0x%x origin 0x%x",
			res, err_origin);	

	return res;
}


TEEC_Result msg_in(const int fd, struct tee_ctx *ctx){
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;
	char sql[1000] = "";

	printf("Write sensor data\n");

	char buff[1024] = "";
	int ret = recv(fd, buff, 1024, 0);
	if (ret == -1)
		errx("read data failed with code 0x%x", ret);
	send(fd, "200", 3, 0);
	sprintf(sql, "INSERT INTO SENSOR (TEMP, HUMLDLTY, SOUND, HIGH, PRESSURE, LIGHT, TIME) VALUES (%s);", buff);

	// printf("%s\n", sql);
	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					TEEC_NONE, TEEC_NONE);
	
	op.params[0].tmpref.size = strlen(sql);
	op.params[0].tmpref.buffer = sql;

	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_CC_CMD_MSG_IN,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS){
		close(fd);
		errx(1, "msg_in failed with code 0x%x origin 0x%x",
			res, err_origin);
	}
	return res;
}

TEEC_Result msg_out(const int fd, struct tee_ctx *ctx, char *sql){
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;
	printf("Get sensor data\n");

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT,
					TEEC_VALUE_INOUT, TEEC_NONE);
	op.params[0].tmpref.buffer = sql;
	op.params[0].tmpref.size = strlen(sql);
	op.params[1].tmpref.size = 4096;
	char data[4096] = "";
	op.params[1].tmpref.buffer = data;
	op.params[2].value.a = 0;

	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_CC_CMD_MSG_OUT,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS){
		close(fd);
		errx(1, "msg_out failed with code 0x%x origin 0x%x",
			res, err_origin);	
	}
	memcpy(data, op.params[1].tmpref.buffer, op.params[1].tmpref.size);

	char hex_data[4096]="";
	char temp[100]="";
	for(int i=0;i<op.params[2].value.a;i++){
		// if(data[i]==0&&i%16==0) break;
		sprintf(temp, "%02x", data[i]);
		strcat(hex_data, temp);
	}

	char response[8192] = "";
	sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s", strlen(hex_data), hex_data);

    send(fd, response, strlen(response), 0);
	return res;
}

static void replace_percent_with_space(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '+') {
            str[i] = ' '; // 将+替换为空格
        }
    }
}

static void http_recieve(int fd, struct tee_ctx *ctx){
	char http_request[4096] = "";
    ssize_t bytes_received = recv(fd, http_request, sizeof(http_request) - 1, 0);
	// printf("%s\n", http_request);
	char param[10][100];
	const char *referer_start = strstr(http_request, "GET");
    if (referer_start) {
        // 在Referer字段中查找查询字符串的起始位置
        const char *query_start = strchr(referer_start, '?');
        if (query_start) {
            query_start++; // 跳过'?'字符

            // 解析查询参数
            char *token;
            char *saveptr;

            token = strtok_r(query_start, "&", &saveptr);
			int i = 0;
            while (token != NULL) {
                char param_name[32] = "";
                char param_value[32] = ""; // 初始化为空字符串

                // 使用sscanf解析参数名称和值
                sscanf(token, "%31[^=]=%31s", param_name, param_value);

                // printf("Parameter Name: %s\n", param_name);
                // printf("Parameter Value: %s\n", param_value);

                token = strtok_r(NULL, "&", &saveptr);
				strcpy(param[i], param_value);
				i++;
            }
			char sql[4096] = "";
			if(strcmp(param[0], "0")==0){
				strcpy(sql, "SELECT * FROM SENSOR ORDER BY ID DESC LIMIT 20;");
			}
			else if(strcmp(param[0], "1")==0){
				if(strcmp(param[1], "0")==0)
					strcpy(sql, "SELECT TEMP, TIME FROM SENSOR ORDER BY ID DESC LIMIT 20;");
				else if (strcmp(param[1], "1")==0)
					strcpy(sql, "SELECT HUMLDLTY, TIME FROM SENSOR ORDER BY ID DESC LIMIT 20;");
				else if (strcmp(param[1], "2")==0)
					strcpy(sql, "SELECT SOUND, TIME FROM SENSOR ORDER BY ID DESC LIMIT 20;");
				else if (strcmp(param[1], "3")==0)
					strcpy(sql, "SELECT LIGHT, TIME FROM SENSOR ORDER BY ID DESC LIMIT 20;");
				else if (strcmp(param[1], "4")==0)
					strcpy(sql, "SELECT HIGH, TIME FROM SENSOR ORDER BY ID DESC LIMIT 20;");
				else if (strcmp(param[1], "5")==0)
					strcpy(sql, "SELECT PRESSURE, TIME FROM SENSOR ORDER BY ID DESC LIMIT 20;");
			}
			else if (strcmp(param[0], "2")==0)
			{		
				replace_percent_with_space(param[2]);
				replace_percent_with_space(param[1]);
				snprintf(sql, 4096, "SELECT * FROM SENSOR WHERE TIME>\"%s\" AND TIME<\"%s\";", param[1], param[2]);
			}
			msg_out(fd, ctx, sql);
        } else {
            printf("No query string found in Referer\n");
        }
    } else {
        printf("Referer not found in HTTP request\n");
    }
}


TEEC_Result database_init(struct tee_ctx *ctx){
	if(access(DATABASE, F_OK)==0){
		return TEEC_SUCCESS;
	}

	printf("Database initialization\n");
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_CC_CMD_DATABASE_INIT,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "database_init failed with code 0x%x origin 0x%x",
			res, err_origin);	
	return res;
}

TEEC_Result sql_exec(int fd, struct tee_ctx *ctx, char* db_name, char *sql){
	TEEC_Operation op;
	uint32_t err_origin;
	TEEC_Result res;
	printf("Execute sql statement: %s\n", sql);
	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_INPUT,
					TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT);

	op.params[0].tmpref.buffer = db_name;
	op.params[0].tmpref.size = strlen(db_name);
	op.params[1].tmpref.buffer = sql;
	op.params[1].tmpref.size = strlen(sql);
	op.params[2].value.a = 0;

	op.params[3].tmpref.size = 4096;
	char data[4096] = "";
	op.params[3].tmpref.buffer = data;

	res = TEEC_InvokeCommand(&ctx->sess,
				 TA_CC_CMD_SQL_EXEC,
				 &op, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "sql_exec failed with code 0x%x origin 0x%x",
			res, err_origin);	

	printf("%s %s\n", db_name, sql);

	char hex_data[4096]="";
	char temp[100]="";
	for(int i=0;i<op.params[2].value.a;i++){
		sprintf(temp, "%02x", data[i]);
		strcat(hex_data, temp);
	}
	if(fd!=-1){
		char response[8192] = "";
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s", strlen(hex_data), hex_data);
	// printf("%s\n", response);
	// strcat(response, data);
    	send(fd, response, strlen(response), 0);
	}
	return res;
}

void http_sql_exec(int fd, struct tee_ctx *ctx){
	char http_request[4096] = "";
    ssize_t bytes_received = recv(fd, http_request, sizeof(http_request) - 1, 0);
	printf("%s\n", http_request);
	char param[10][500];
	// const char *referer_start = strstr(http_request, "POST");

	const char *db_start = strstr(http_request, "{\"db\":\"");

    if (db_start) {
        // 移动指针到db值的起始位置
        db_start += strlen("{\"db\":\"");

        // 使用strchr函数找到db值的结束位置
        const char *db_end = strchr(db_start, '\"');

        if (db_end) {
            // 计算db值的长度
            size_t db_length = db_end - db_start;

            // 分配内存保存db值并复制
            char db_value[db_length + 1];
            strncpy(db_value, db_start, db_length);
            db_value[db_length] = '\0';

			char db_name[100] = "";
			snprintf(db_name, 100, "/home/pi/database/%s", db_value);
			memcpy(param[0], db_name, strlen(db_name));
			
            // 打印提取的db值
            printf("Extracted db value: %s\n", db_name);
        } else {
            printf("No db value found in the HTTP request.\n");
        }
    } else {
        printf("No db value found in the HTTP request.\n");
    }

	const char *sql_start = strstr(http_request, "\"sql\":\"");

    if (sql_start) {
        // 移动指针到sql值的起始位置
        sql_start += strlen("\"sql\":\"");

        // 使用strchr函数找到sql值的结束位置
        const char *sql_end = strchr(sql_start, '\"');

        if (sql_end) {
            // 计算sql值的长度
            size_t sql_length = sql_end - sql_start;

            // 分配内存保存sql值并复制
            char sql_value[sql_length + 1];
            strncpy(sql_value, sql_start, sql_length);
            sql_value[sql_length] = '\0';
			replace_percent_with_space(sql_value);
			memcpy(param[1], sql_value, sql_length+1);
            // 打印提取的sql值
            printf("Extracted sql value: %s\n", sql_value);
        } else {
            printf("No sql value found in the HTTP request.\n");
        }
    } else {
        printf("No sql value found in the HTTP request.\n");
    }

	sql_exec(fd, ctx, param[0], param[1]);

}

// 
void listenPort1(void *arg) {
	int port = SQL_PORT;
    struct tee_ctx *ctx = (struct tee_ctx*) arg;
    int sockfd, new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;

    // 创建套接字
    int res;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd==-1)
        errx(1, "Socket creation failed with code 0x%x", sockfd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(IP);


    res = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(res==-1){
		close(sockfd);
        errx(1, "Socket binding failed with code 0x%x", res);
	}
    res = listen(sockfd, 5);

    if(res==-1){
		close(sockfd);
        errx(1, "Socket listening failed with code 0x%x", res);
	}
    
    printf("Server linstening on port %d\n", port);

    // 接受连接并处理
    while (1) {
        addr_len = sizeof(client_addr);
        new_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);

		// write_ip(ctx, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        // get_iP(new_sock, ctx);
		http_sql_exec(new_sock, ctx);
    	close(new_sock);
    }
}

// 获取数据库数据
void listenPort2(void *arg) {
	int port = MSG_OUT_PORT;
    struct tee_ctx *ctx = (struct tee_ctx*) arg;
    int sockfd, new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;

    // 创建套接字

    int res;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd==-1)
        errx(1, "Socket creation failed with code 0x%x", sockfd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(IP);

    res = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(res==-1){
		close(sockfd);
        errx(1, "Socket binding failed with code 0x%x", res);
	}
    
    res = listen(sockfd, 5);
    if(res==-1){
		close(sockfd);
        errx(1, "Socket listening failed with code 0x%x", res);
	}

    
    printf("Server linstening on port %d\n", port);

    // 接受连接并处理
    while (1) {
        addr_len = sizeof(client_addr);
        new_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
		http_recieve(new_sock, ctx);
    	close(new_sock);
    }
}

// 传感器写入数据
void listenPort3(void *arg) {
	int port = MSG_IN_PORT;
    struct tee_ctx *ctx = (struct tee_ctx*) arg;
    int sockfd, new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;

    // 创建套接字

    int res;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd==-1)
        errx(1, "Socket creation failed with code 0x%x", sockfd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(IP);

    res = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(res==-1){
		close(sockfd);
        errx(1, "Socket binding failed with code 0x%x", res);
	}
    res = listen(sockfd, 5);
    if(res==-1){
		close(sockfd);
        errx(1, "Socket listening failed with code 0x%x", res);
	}
    
    printf("Server linstening on port %d\n", port);

    // 接受连接并处理
    while (1) {
        addr_len = sizeof(client_addr);
        new_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        msg_in(new_sock, ctx);
    	close(new_sock);
    }
}


int main(int argc, char **argv)
{
	
	struct tee_ctx ctx;
    pthread_t thread1, thread2, thread3;

	prepare_tee_session(&ctx);
	database_init(&ctx);
	
	// 创建线程来监听端口1
    if (pthread_create(&thread1, NULL, listenPort1, &ctx) != 0) {
        errx(1, "Create thread1 failed with code 0x%x", -1);
    }

    // 创建线程来监听端口2
    if (pthread_create(&thread2, NULL, listenPort2, &ctx) != 0) {
        errx(1, "Create thread12 failed with code 0x%x", -1);
    }

	if (pthread_create(&thread3, NULL, listenPort3, &ctx) != 0) {
        errx(1, "Create thread3 failed with code 0x%x", -1);
    }

    // 等待线程结束
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

	terminate_tee_seesion(&ctx);

	return 0;
}
