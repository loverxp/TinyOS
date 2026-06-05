#include "../include/ipc.h"
#include "../include/scheduler.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/stdio.h"
#include "../include/io.h"

static pipe_t   pipes[MAX_PIPES];
static mqueue_t mqueues[MAX_MQUEUES];
static shm_region_t shm_regions[MAX_SHM];

#define IPC_WAIT_READ   1
#define IPC_WAIT_WRITE  2

static void ipc_block(void* obj, uint8_t wait_type) {
    task_t* cur = scheduler_get_current();
    disable_interrupts();
    cur->ipc_wait_obj = obj;
    cur->ipc_wait_type = wait_type;
    cur->state = TASK_BLOCKED;
    need_reschedule = 1;
    enable_interrupts();
    while (cur->state == TASK_BLOCKED) {
        halt();
    }
    cur->ipc_wait_obj = NULL;
    cur->ipc_wait_type = 0;
}

void ipc_wake(void* obj, uint8_t wait_type) {
    scheduler_wake_ipc(obj, wait_type);
}

void ipc_init(void) {
    memset(pipes, 0, sizeof(pipes));
    memset(mqueues, 0, sizeof(mqueues));
    memset(shm_regions, 0, sizeof(shm_regions));
    printf("[IPC] Initialized (pipes=%d, mqueues=%d, shm=%d)\n",
           MAX_PIPES, MAX_MQUEUES, MAX_SHM);
}

/* ---- Pipe ---- */

int pipe_create(void) {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].in_use) {
            memset(&pipes[i], 0, sizeof(pipe_t));
            pipes[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

int pipe_read(int id, void* buf, uint32_t max_len) {
    if (id < 0 || id >= MAX_PIPES || !pipes[id].in_use) return -1;
    pipe_t* p = &pipes[id];

    while (p->count == 0 && !p->closed) {
        ipc_block(p, IPC_WAIT_READ);
    }
    if (p->count == 0 && p->closed) return 0;

    uint32_t n = (max_len < p->count) ? max_len : p->count;
    uint8_t* dst = (uint8_t*)buf;
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = p->buf[p->head];
        p->head = (p->head + 1) % PIPE_BUF_SIZE;
    }
    p->count -= n;

    if (p->count < PIPE_BUF_SIZE) {
        ipc_wake(p, IPC_WAIT_WRITE);
    }
    return (int)n;
}

int pipe_write(int id, const void* data, uint32_t len) {
    if (id < 0 || id >= MAX_PIPES || !pipes[id].in_use) return -1;
    pipe_t* p = &pipes[id];
    if (p->closed) return -1;

    const uint8_t* src = (const uint8_t*)data;
    uint32_t written = 0;
    while (written < len) {
        while (p->count >= PIPE_BUF_SIZE && !p->closed) {
            ipc_block(p, IPC_WAIT_WRITE);
        }
        if (p->closed) return (written > 0) ? (int)written : -1;

        p->buf[p->tail] = src[written++];
        p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
        p->count++;
        ipc_wake(p, IPC_WAIT_READ);
    }
    return (int)written;
}

void pipe_close(int id) {
    if (id < 0 || id >= MAX_PIPES || !pipes[id].in_use) return;
    pipes[id].closed = 1;
    ipc_wake(&pipes[id], IPC_WAIT_READ);
    ipc_wake(&pipes[id], IPC_WAIT_WRITE);
    pipes[id].in_use = 0;
}

/* ---- Message Queue ---- */

int mq_create(void) {
    for (int i = 0; i < MAX_MQUEUES; i++) {
        if (!mqueues[i].in_use) {
            memset(&mqueues[i], 0, sizeof(mqueue_t));
            mqueues[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

int mq_send(int id, const void* data, uint32_t len) {
    if (id < 0 || id >= MAX_MQUEUES || !mqueues[id].in_use) return -1;
    if (len > MQ_MSG_SIZE) return -1;
    mqueue_t* mq = &mqueues[id];
    if (mq->closed) return -1;

    while (mq->count >= MQ_MAX_SLOTS && !mq->closed) {
        ipc_block(mq, IPC_WAIT_WRITE);
    }
    if (mq->closed) return -1;

    memcpy(mq->data[mq->tail], data, len);
    mq->msg_len[mq->tail] = (uint8_t)len;
    mq->tail = (mq->tail + 1) % MQ_MAX_SLOTS;
    mq->count++;

    ipc_wake(mq, IPC_WAIT_READ);
    return 0;
}

int mq_recv(int id, void* buf, uint32_t max_len) {
    if (id < 0 || id >= MAX_MQUEUES || !mqueues[id].in_use) return -1;
    mqueue_t* mq = &mqueues[id];

    while (mq->count == 0 && !mq->closed) {
        ipc_block(mq, IPC_WAIT_READ);
    }
    if (mq->count == 0 && mq->closed) return -1;

    uint8_t len = mq->msg_len[mq->head];
    uint8_t copy_len = (max_len < len) ? (uint8_t)max_len : len;
    memcpy(buf, mq->data[mq->head], copy_len);
    mq->head = (mq->head + 1) % MQ_MAX_SLOTS;
    mq->count--;

    if (mq->count < MQ_MAX_SLOTS) {
        ipc_wake(mq, IPC_WAIT_WRITE);
    }
    return (int)copy_len;
}

void mq_close(int id) {
    if (id < 0 || id >= MAX_MQUEUES || !mqueues[id].in_use) return;
    mqueues[id].closed = 1;
    ipc_wake(&mqueues[id], IPC_WAIT_READ);
    ipc_wake(&mqueues[id], IPC_WAIT_WRITE);
    mqueues[id].in_use = 0;
}

/* ---- Shared Memory ---- */

void* shm_create(const char* name, uint32_t size) {
    if (!name || size == 0) return NULL;

    for (int i = 0; i < MAX_SHM; i++) {
        if (shm_regions[i].in_use && strcmp(shm_regions[i].name, name) == 0) {
            return shm_regions[i].addr;
        }
    }

    for (int i = 0; i < MAX_SHM; i++) {
        if (!shm_regions[i].in_use) {
            uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
            if (pages > 1) return NULL;

            void* addr = pmm_alloc_page();
            if (!addr) return NULL;
            memset(addr, 0, PAGE_SIZE);

            strncpy(shm_regions[i].name, name, SHM_NAME_MAX);
            shm_regions[i].addr = addr;
            shm_regions[i].size = size;
            shm_regions[i].num_pages = pages;
            shm_regions[i].in_use = 1;
            return addr;
        }
    }
    return NULL;
}

void* shm_open(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < MAX_SHM; i++) {
        if (shm_regions[i].in_use && strcmp(shm_regions[i].name, name) == 0) {
            return shm_regions[i].addr;
        }
    }
    return NULL;
}

int shm_close(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < MAX_SHM; i++) {
        if (shm_regions[i].in_use && strcmp(shm_regions[i].name, name) == 0) {
            for (uint32_t p = 0; p < shm_regions[i].num_pages; p++) {
                pmm_free_page((void*)((uint32_t)shm_regions[i].addr + p * PAGE_SIZE));
            }
            memset(&shm_regions[i], 0, sizeof(shm_region_t));
            return 0;
        }
    }
    return -1;
}
