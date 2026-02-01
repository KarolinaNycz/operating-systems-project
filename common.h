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
#define MAX_SECTORS 8
#define MIN_CASHIERS 2
#define MAX_CASHIERS 10
#define MAX_FANS 500 //ile fanow
#define GATES_PER_SECTOR 2
#define MAX_GATE_CAPACITY 3

#define MSG_BUY_TICKET 1
#define MSG_TICKET_OK  2
#define MSG_SECTOR_EMPTY 3

#define MSG_GATE_REQUEST  10
#define MSG_GATE_RESPONSE 11
#define MSG_GATE_LEAVE    12
#define MSG_GATE_REJECT 13
#define FLARE_PROB 5   // 0.5% ze ma race (5 / 1000)

typedef struct 
{
    int fan_counter;
    int total_capacity;
    int sector_capacity[MAX_SECTORS];
    int gate_count[MAX_SECTORS][GATES_PER_SECTOR];
    int gate_team[MAX_SECTORS][GATES_PER_SECTOR];
    int sector_taken[MAX_SECTORS];
    int entry_blocked[MAX_SECTORS];
    int evacuation;
    int gate_wait[MAX_FANS];
    int priority[MAX_FANS];
    int gate_queue[MAX_SECTORS];
    int active_cashiers;
    int ticket_queue;
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


#endif
