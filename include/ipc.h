#ifndef IPC_H
#define IPC_H

#include "types.h"

#define PIPE_BUF_SIZE  512
#define MAX_PIPES      8

#define MQ_MSG_SIZE    64
#define MQ_MAX_SLOTS   8
#define MAX_MQUEUES    8

#define SHM_NAME_MAX   16
#define MAX_SHM        8

typedef struct {
    int      in_use;
    int      closed;
    uint8_t  buf[PIPE_BUF_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} pipe_t;

typedef struct {
    int      in_use;
    int      closed;
    uint8_t  data[MQ_MAX_SLOTS][MQ_MSG_SIZE];
    uint8_t  msg_len[MQ_MAX_SLOTS];
    uint8_t  head;
    uint8_t  tail;
    uint8_t  count;
} mqueue_t;

typedef struct {
    int      in_use;
    char     name[SHM_NAME_MAX];
    void*    addr;
    uint32_t size;
    uint32_t num_pages;
} shm_region_t;

void  ipc_init(void);

int   pipe_create(void);
int   pipe_read(int id, void* buf, uint32_t max_len);
int   pipe_write(int id, const void* data, uint32_t len);
void  pipe_close(int id);
void  pipe_shutdown_write(int id);  /* Close write end only — readers see EOF but pipe remains usable */

int   mq_create(void);
int   mq_send(int id, const void* data, uint32_t len);
int   mq_recv(int id, void* buf, uint32_t max_len);
void  mq_close(int id);

void* shm_create(const char* name, uint32_t size);
void* shm_open(const char* name);
int   shm_close(const char* name);

void  ipc_wake(void* obj, uint8_t wait_type);

#endif // IPC_H
