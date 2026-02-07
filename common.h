#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <signal.h>

#define SIG_EVACUATE SIGUSR2

#define IPC_KEY 11
#define MAX_SECTORS 9   // 0-7 normalne, 8 = VIP
#define VIP_SECTOR 8

#define MIN_CASHIERS 2
#define MAX_CASHIERS 10
#define MAX_FANS 500 //ile fanow
#define GATES_PER_SECTOR 2
#define MAX_GATE_CAPACITY 3

#define MSG_BUY_TICKET 1
#define MSG_BUY_TICKET_VIP 5
#define MSG_TICKET_OK  2
#define MSG_SECTOR_EMPTY 3

#define MSG_GATE_REQUEST  10
#define MSG_GATE_RESPONSE 11
#define MSG_GATE_LEAVE    12
#define MSG_GATE_REJECT 13
#define MSG_GATE_BASE 3000
#define FLARE_PROB 5  // 0.5% ze ma race (5 / 1000)

#define MAX_QUEUE 1000

typedef struct 
{
    int buf[MAX_QUEUE];
    int head;
    int tail;
    int size;
} gate_queue_t;

typedef struct 
{
    int fan_counter;
    int vip_count;
    int total_capacity;
    int sector_capacity[MAX_SECTORS];
    int gate_count[MAX_SECTORS][GATES_PER_SECTOR];
    int gate_team[MAX_SECTORS][GATES_PER_SECTOR];
    int sector_taken[MAX_SECTORS];
    int entry_blocked[MAX_SECTORS];
    int evacuation;
    int gate_wait[MAX_FANS];
    int priority[MAX_FANS];
    int active_cashiers;
    int ticket_queue;
    int vip_queue;

    int last_adult_id;
    gate_queue_t gate_queue[MAX_SECTORS];
} shared_data_t;

typedef struct
{
    long mtype;
    pid_t pid;
    int vip;
    int has_flare; 
    int team;
    int sector;
    int tickets;
    int age;        // wiek fana
    int guardian;   // pid opiekuna (0 = brak)
    int want_tickets;   // ile chce kupić (1 lub 2)
} msg_t;

union semun 
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void fatal_error(const char *msg);

int create_shared_memory(void);
int create_message_queue(void);
int create_semaphore(void);

void remove_shared_memory(int shmid);
void remove_semaphore(int semid);

static inline int sem_lock(int semid, int num)
{
    struct sembuf sb = {num, -1, SEM_UNDO};

    while (1)
    {
        if (semop(semid, &sb, 1) == 0)
            return 0;

        if (errno == EINTR)
            return -1;

        perror("semop lock");
        exit(1);
    }
}

static inline int sem_unlock(int semid, int num)
{
    struct sembuf sb = {num, 1, SEM_UNDO};

    while (1)
    {
        if (semop(semid, &sb, 1) == 0)
            return 0;

        if (errno == EINTR)
            return -1;

        perror("semop unlock");
        exit(1);
    }
}

static inline void queue_push(gate_queue_t *q, int pid)
{
    if (q->size >= MAX_QUEUE) return;

    q->buf[q->tail] = pid;
    q->tail = (q->tail + 1) % MAX_QUEUE;
    q->size++;
}

static inline int queue_pop(gate_queue_t *q)
{
    if (q->size == 0) return -1;

    int pid = q->buf[q->head];
    q->head = (q->head + 1) % MAX_QUEUE;
    q->size--;

    return pid;
}

static inline int queue_front(gate_queue_t *q)
{
    if (q->size == 0) return -1;

    return q->buf[q->head];
}



#endif