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

#define SIG_STOP_WORK SIGUSR1

#define IPC_KEY 11
#define MAX_SECTORS 8
#define MIN_CASHIERS 2

typedef struct 
{
    int total_capacity;
    int sector_capacity[MAX_SECTORS];
} shared_data_t;

typedef struct
{
    long mtype;
    pid_t pid;
} msg_t;

void fatal_error(const char *msg);

int create_shared_memory(void);
int create_message_queue(void);
int create_semaphore(void);

void remove_shared_memory(int shmid);
void remove_semaphore(int semid);

#endif
