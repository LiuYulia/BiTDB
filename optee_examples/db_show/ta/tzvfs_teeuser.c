#include <tee_internal_api.h>
#include "./include/tzvfs_teeuser.h"

int tzvfs_errno;

static TEE_Time start, end, mid;
extern int write_time, read_time, lseek_time, strcspn_time;

off_t offset_g = 0;
int whence_g = SEEK_CUR;

// #include <tee_api.h>
// #define TA_AES_MODE_ENCODE    1
// #define TA_AES_MODE_DECODE    0
// #define AES128_KEY_BIT_SIZE   128
// #define AES128_KEY_BYTE_SIZE    (AES128_KEY_BIT_SIZE / 8)
// #define TA_AES_ALGO_ECB     0
// #define TA_AES_ALGO_CBC     1
// #define TA_AES_ALGO_CTR     2
// #define AES_BLOCK_SIZE    16

// #define AES256_KEY_BIT_SIZE   256
// #define AES256_KEY_BYTE_SIZE    (AES256_KEY_BIT_SIZE / 8)

// TEE_OperationHandle l_OperationHandle; 
// uint32_t algo = TEE_ALG_AES_CBC_NOPAD;
// uint32_t mode = TEE_MODE_ENCRYPT;
// uint32_t key_size = AES256_KEY_BYTE_SIZE;
// TEE_OperationHandle op_handle;
// TEE_ObjectHandle key_handle;

int dd, de, iv_time, read_time, write_time;

// 打开文件. 调用成功时返回一个文件描述符fd, 调用失败时返回-1, 并修改errno
int tzvfs_open(const char *filename, int flags, mode_t mode){
  int ret = -1;
  // aes_init();
  // TEE_AllocateOperation(&l_OperationHandle, TEE_ALG_SHA256, TEE_MODE_DIGEST, 0);
  // TEE_GetREETime(&start);
  ret = utee_tzvfs_open(&tzvfs_errno, filename, flags, mode);
  // TEE_GetREETime(&end);
  // DMSG("has been called, filename=%s, flags=%d, mode=%d, ret=%d, tzvfs_errno=%d", filename, flags, mode, ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// 若文件顺利关闭则返回0, 发生错误时返回-1
int tzvfs_close(int fd){
  int ret = -1;
  
  // TEE_GetREETime(&start);
  ret = utee_tzvfs_close(&tzvfs_errno, fd);
  // TEE_GetREETime(&end);
  // DMSG("has been called, fd=%d, ret=%d, tzvfs_errno=%d", fd, ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// 获取当前工作目录, 成功则返回当前工作目录; 如失败返回NULL, 错误代码存于errno
char *tzvfs_getcwd(char *buf, size_t size){
  char* ret = NULL;
  
  // TEE_GetREETime(&start);
  ret = utee_tzvfs_getcwd(&tzvfs_errno, buf, size);
  // TEE_GetREETime(&end);
  // DMSG("has been called, buf=%x, size=%d, ret=%d, tzvfs_errno=%d", buf, size, ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// 获取一些文件相关的信息, 成功执行时，返回0。失败返回-1，errno
// lstat函数是不穿透（不追踪）函数，对软链接文件进行操作时，操作的是软链接文件本身
int tzvfs_lstat( const char* path, struct tzvfs_stat *buf ) {
  int ret = -1;

  // TEE_GetREETime(&start);
  ret = utee_tzvfs_lstat(&tzvfs_errno, path, buf);
  // TEE_GetREETime(&end);
  // DMSG("has been called, path=%s, buf=%x, ret=%d, sizeof(struct tzvfs_flock)=%d, tzvfs_errno=%d", path, buf, ret, sizeof(struct tzvfs_flock), tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// 获取一些文件相关的信息, 成功执行时，返回0。失败返回-1，errno
// stat函数是穿透（追踪）函数，即对软链接文件进行操作时，操作的是链接到的那一个文件，不是软链接文件本身
int tzvfs_stat(const char *path, struct tzvfs_stat *buf){
  int ret = -1;

  // TEE_GetREETime(&start);
  ret = utee_tzvfs_stat(&tzvfs_errno, path, buf);
  // TEE_GetREETime(&end);
  // DMSG("has been called, path=%s, buf=%x, ret=%d, tzvfs_errno=%d", path, buf, ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// fstat函数与stat函数的功能一样，只是第一个形参是文件描述符
int tzvfs_fstat(int fd, struct tzvfs_stat *buf){
  int ret = -1;

  // TEE_GetREETime(&start);
  ret = utee_tzvfs_fstat(&tzvfs_errno, fd, buf);
  // TEE_GetREETime(&end);
  // DMSG("has been called, fd=%d, buf=%x, ret=%d, tzvfs_errno=%d", fd, buf, ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// 通过fcntl可以改变已打开的文件性质, F_SETLK 设置文件锁定的状态
// fcntl的返回值与命令有关。如果出错，所有命令都返回-1，如果成功则返回某个其他值。
int tzvfs_fcntl(int fd, int cmd, ... /* arg */ ){
  int ret = -1;
  
  // Read one argument
  va_list valist;
  va_start(valist, cmd);
  struct tzvfs_flock* arg = va_arg(valist, struct tzvfs_flock*);
  va_end(valist);
  
  // TEE_GetREETime(&start);
  ret = utee_tzvfs_fcntl(&tzvfs_errno, fd, cmd, arg);
  // TEE_GetREETime(&end);
  // DMSG("has been called, fd=%d, cmd=%d, arg=%x, ret=%d, tzvfs_errno=%d", fd, cmd, arg, ret, sizeof(struct tzvfs_flock), tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// read会把参数fd所指的文件传送count个字节到buf指针所指的内存中
// 返回值为实际读取到的字节数, 如果返回0, 表示已到达文件尾或是无可读取的数据
// 当有错误发生时则返回-1, 错误代码存入errno 中
ssize_t tzvfs_read(int fd, void *buf, size_t count){
  ssize_t ret = -1;

  // read_time++;
  ret = utee_tzvfs_read(&tzvfs_errno, fd, buf, count,  offset_g, whence_g);
  // MSG("read count: %d offset: %d fd: %d", count, offset_g, fd);
  // unsigned char temp[4096] = "";
  // for(int i=0;i<count; i++){
  //   snprintf(temp, 4096, "%s %x", temp, ((unsigned char*)buf)[i]);
  // }
  // MSG("len: %d data: %s\n", count, temp);
  offset_g = 0;
  whence_g = SEEK_CUR;
  // TEE_GetREETime(&start);
  // aes(buf, count);
  // TEE_GetREETime(&mid);
  // sha256(buf, count);
  // TEE_GetREETime(&end
  // dd += 1000*(mid.seconds-start.seconds) + (mid.millis-start.millis);
  // iv_time += 1000*(end.seconds-mid.seconds) + (end.millis-mid.millis);
  // DMSG("has been called, fd=%d, buf=%x, count=%d, ret=%d, tzvfs_errno=%d", fd, buf, count, ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  // read_time += 1000*(end.seconds-start.seconds) + (end.millis-start.millis);
  return ret;
}

pid_t tzvfs_getpid(void){
  pid_t ret = -1;

  // TEE_GetREETime(&start);
  ret = utee_tzvfs_getpid(&tzvfs_errno);
  // TEE_GetREETime(&end);
  // DMSG("has been called, ret=%d, tzvfs_errno=%d", ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

// geteuid()用来取得执行目前进程有效的用户识别码
// 返回有效的用户识别码
uid_t tzvfs_geteuid(void){
  uid_t ret = -1;

  // TEE_GetREETime(&start);
  ret = utee_tzvfs_geteuid(&tzvfs_errno);
  // TEE_GetREETime(&end);
  // DMSG("has been called, ret=%d, tzvfs_errno=%d", ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  return ret;
}

off_t tzvfs_lseek(int fd, off_t offset, int whence){
  off_t ret = offset;

  offset_g = offset;
  whence_g = whence;

  // TEE_GetREETime(&start);
  // ret = utee_tzvfs_lseek(&tzvfs_errno, fd, offset, whence);
  // TEE_GetREETime(&end);
  // DMSG("has been called, fd=%d, offset=%d, whence=%d, ret=%d, tzvfs_errno=%d", fd, offset, whence, ret, tzvfs_errno);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  // lseek_time += 1000*(end.seconds-start.seconds) + (end.millis-start.millis);
  return ret;
}

// write函数把buf中nbyte写入文件描述符handle所指的文档
// 成功时返回写的字节数，错误时返回-1
ssize_t tzvfs_write(int fd, const void *buf, size_t count){
  ssize_t ret = -1;
  // write_time++;
  // MSG("has been called");

  ret = utee_tzvfs_write(&tzvfs_errno, fd, buf, count,  offset_g, whence_g);
  // MSG("11111   %d", count);
  offset_g = 0;
  whence_g = SEEK_CUR;
  // TEE_GetREETime(&start);
  // aes(buf, count);
  // TEE_GetREETime(&mid);
  // sha256(buf, count);
  // TEE_GetREETime(&end);
  // de += 1000*(mid.seconds-start.seconds) + (mid.millis-start.millis);
  // iv_time += 1000*(end.seconds-mid.seconds) + (end.millis-mid.millis);
  // DMSG("time: %d ms", 1000*(end.seconds-start.seconds) + (end.millis-start.millis));
  // write_time += 1000*(end.seconds-start.seconds) + (end.millis-start.millis);
  
  return ret;
}

int tzvfs_unlink(const char *pathname){
  int ret = -1;
  
  // DMSG("has been called");
  ret = utee_tzvfs_unlink(&tzvfs_errno, pathname);

  return ret;
}

int tzvfs_access(const char *pathname, int mode){
  int ret = -1;

  // DMSG("has been called");
  ret = utee_tzvfs_access(&tzvfs_errno, pathname, mode);

  return ret;
}

void *tzvfs_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off){
  void* ret = (void *)-1;

  // DMSG("has been called");
  ret =  utee_tzvfs_mmap(&tzvfs_errno, addr, len, prot, flags, fildes, off);
  
  return ret;
}

void *tzvfs_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ... /* void *new_address */){
  void* ret = (void *)-1;

  // DMSG("has been called");
  ret =  utee_tzvfs_mremap(&tzvfs_errno, old_address, old_size, new_size, flags);

  return ret;
}

int tzvfs_munmap(void *addr, size_t length){
  int ret = -1;

  // DMSG("has been called");
  ret = utee_tzvfs_munmap(&tzvfs_errno, addr, length);

  return ret;
}

size_t tzvfs_strcspn(const char *str1, const char *str2){
  // size_t ret = 0;

  // // DMSG("has been called");
  // ret = utee_tzvfs_strcspn(&tzvfs_errno, str1, str2);
  
  // return ret;
  // TEE_GetREETime(&start);
  char *p1 = (char *)str1;
  char *p2;
  while (*p1 != '\0') {
      p2 = (char *)str2;
      while (*p2 != '\0') {
          if (*p2 == *p1) return p1-str1;
          ++p2;
      }
      ++p1;
  }
  // TEE_GetREETime(&end);
  // strcspn_time += 1000*(end.seconds-start.seconds) + (end.millis-start.millis);
  return  p1 - str1;
}

int tzvfs_utimes(const char *filename, const struct tzvfs_timeval times[2]){
  int ret = -1;

  // DMSG("has been called");
  ret = utee_tzvfs_utimes(&tzvfs_errno, filename, times);
  
  return ret;
}

int tzvfs_fsync(int fd){
  int ret = -1;

  // DMSG("has been called");
  ret = utee_tzvfs_fsync(&tzvfs_errno, fd);

  return ret;
}

char* tzvfs_getenv(const char *name){
  char* ret = NULL;

  // DMSG("has been called");
  ret = utee_tzvfs_getenv(&tzvfs_errno, name);
  
  return ret;
}

time_t tzvfs_time(time_t *t){
  time_t ret = -1;

  // DMSG("has been called");
  ret =  utee_tzvfs_time(&tzvfs_errno, t);

  return ret;
}

unsigned int tzvfs_sleep(unsigned int seconds){
  unsigned int ret;

  // DMSG("has been called");
  ret = utee_tzvfs_sleep(&tzvfs_errno, seconds);

  return ret;
}

int tzvfs_gettimeofday(struct tzvfs_timeval *tv, struct tzvfs_timezone *tz){
  int ret = -1;

  // DMSG("has been called");
  ret = utee_tzvfs_gettimeofday(&tzvfs_errno, tv, tz);

  return ret;
}

int tzvfs_fchown(int fd, uid_t owner, gid_t group){
  int ret = -1;

  // DMSG("has been called");
  ret = utee_tzvfs_fchown(&tzvfs_errno, fd, owner, group);
  
  return ret;
}

int tzvfs_ftruncate(int fd, off_t length){
  // DMSG("%s: haven't been realized!\n", __func__);
  return 0;
}

int tzvfs_fchmod(int fd, mode_t mode){
  // DMSG("%s: haven't been realized!\n", __func__);
  return 0;
}

void *tzvfs_dlopen(const char *filename, int flag){
  // DMSG("%s: haven't been realized!\n", __func__);
  return NULL;
}

char *tzvfs_dlerror(void){
  // DMSG("%s: haven't been realized!\n", __func__);
  return NULL;
}

void *tzvfs_dlsym(void *handle, const char *symbol){
  // DMSG("%s: haven't been realized!\n", __func__);
  return NULL;
}

int tzvfs_dlclose(void *handle){
  // DMSG("%s: haven't been realized!\n", __func__);
  return 0;
}

int tzvfs_mkdir(const char *pathname, mode_t mode) {
  // DMSG("%s: haven't been realized!\n", __func__);
  return 0;
}

int tzvfs_rmdir(const char *pathname){
  // DMSG("%s: haven't been realized!\n", __func__);
  return 0;
}

ssize_t tzvfs_readlink(const char *path, char *buf, size_t bufsiz){
  // DMSG("%s: haven't been realized!\n", __func__);
  return 0;
}

long int tzvfs_sysconf(int name){
  long int ret = -1;
  // DMSG("%s: haven't been realized!\n", __func__);
  return ret;
}

struct tzvfs_tm *tzvfs_localtime(const time_t *timep){
  struct tzvfs_tm * ret = NULL;
  // DMSG("%s: haven't been realized!\n", __func__);
  return ret;
}



// char key[] = {0x2FU, 0x58U, 0x7FU, 0xF0U, 0x43U, 0x83U, 0x95U, 0x3CU,
//                       0x1DU, 0x44U, 0x05U, 0x2BU, 0x61U, 0x49U, 0x17U, 0xF8U};
// char iv[] = {0x1DU, 0x44U, 0x05U, 0x2BU, 0x61U, 0x49U, 0x17U, 0xF8U,
//                      0x58U, 0xE0U, 0x90U, 0x43U, 0x84U, 0xA1U, 0xC1U, 0x75U};           
// // struct aes_cipher *sess;


// TEE_Attribute attr = {0};

// int flag = 0;


// int aes_init(){
//   flag = 1;

//   // memset(key, 0xa5, sizeof(key));
//   // memset(iv, 0, sizeof(iv));
//   // sess = (struct aes_cipher*)malloc(sizeof(struct aes_cipher));
//   TEE_Result res;


//   // if (op_handle != TEE_HANDLE_NULL)
//   //       TEE_FreeOperation(op_handle);
//   res = TEE_AllocateOperation(&op_handle,
//             algo,
//             mode,
//             key_size * 8);

//   if(res!=TEE_SUCCESS){
//     return -1;
//   }
//   // if (key_handle != TEE_HANDLE_NULL)
//   //       TEE_FreeTransientObject(key_handle);
//   res = TEE_AllocateTransientObject(TEE_TYPE_AES,
//                       key_size * 8,
//                       &key_handle);
//   if (res != TEE_SUCCESS) {
//     EMSG("TEE_AllocateTransientObject failed, %x", res);
//     return -1;
//   }

//   TEE_InitRefAttribute(&attr, TEE_ATTR_SECRET_VALUE, key, key_size);
//   res = TEE_PopulateTransientObject(key_handle, &attr, 1);
//   if (res != TEE_SUCCESS) {
//     EMSG("TEE_PopulateTransientObject failed, %x", res);
//     return -1;
//   }
//   // TEE_ResetOperation(op_handle);
//   res = TEE_SetOperationKey(op_handle, key_handle);
//   if (res != TEE_SUCCESS) {
//     EMSG("TEE_SetOperationKey failed %x", res);
//     return -1;
//   }
//   // DMSG("111111111111");
//   // TEE_CipherInit(op_handle, iv, key_size);
// }

// char out[8192];

// void aes(char *in, uint32_t insz){
//   int res = -1;
//   // if(flag==0){
//   //   res = aes_init();
//   //   if(res == -1) DMSG("111111111111");
//   // }
//   TEE_CipherInit(op_handle, iv, AES_BLOCK_SIZE);
//   int osz = insz;
//   // void *out = TEE_Malloc(4096, 0);
//   //DMSG("111111111111");
//   res = TEE_CipherUpdate(op_handle, in, insz, out, &osz);
//   if(res != TEE_SUCCESS)
//     {
//         EMSG("Do the final aes operation fail %x %d %d\n", res, osz, sizeof(out));
//         return;
//     }
//   // TEE_Free(out);
// }



// int sha_flag = 0;

// void sha256(char* input, uint32_t inLen){
//   TEE_Result ret;
//   // if(sha_flag==0){
//   //   sha_flag = 1;
//   //   ret = TEE_AllocateOperation(&l_OperationHandle, TEE_ALG_SHA256, TEE_MODE_DIGEST, 0);
//   //     if(ret != TEE_SUCCESS) 
//   //     {
//   //         EMSG("Allocate SHA operation handle fail %x\n", ret);
//   //         return;
//   //     }
//   // }
//     TEE_DigestUpdate(l_OperationHandle, input, inLen);

//   // void *output = TEE_Malloc(64, 0);
//   int pOutLen = 32;

//     //计算哈希
//     ret = TEE_DigestDoFinal(l_OperationHandle, NULL, 0, out, &pOutLen);
//     // TEE_Free(output);
//     if(ret != TEE_SUCCESS)
//     {
//         EMSG("Do the final sha operation fail %x %d %d", ret, pOutLen, sizeof(out));
//         return;
//     }

//     // TEE_FreeOperation(l_OperationHandle);
// }
