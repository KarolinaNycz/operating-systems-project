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
#define MAX_FANS 50
#define GATES_PER_SECTOR 2
#define MAX_GATE_CAPACITY 3

#define MSG_BUY_TICKET 1
#define MSG_TICKET_OK  2
#define MSG_SECTOR_EMPTY 3

#define MSG_GATE_REQUEST  10
#define MSG_GATE_RESPONSE 11
#define MSG_GATE_LEAVE    12

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
} shared_data_t;

typedef struct
{
    long mtype;
    pid_t pid;
    int vip;
    int team;
    int sector;
    int tickets;
} msg_t;

void fatal_error(const char *msg);

int create_shared_memory(void);
int create_message_queue(void);
int create_semaphore(void);

void remove_shared_memory(int shmid);
void remove_semaphore(int semid);

static inline void sem_lock(int semid)
{
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}

static inline void sem_unlock(int semid)
{
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}

#endif
