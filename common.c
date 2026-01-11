#include "common.h"

void fatal_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int create_shared_memory(void)
{
    key_t key = ftok(".", IPC_KEY);
    if (key == -1) fatal_error("ftok");

    int shmid = shmget(key, sizeof(shared_data_t), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid == -1) fatal_error("shmget");

    shared_data_t *d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("shmat");

    memset(d, 0, sizeof(*d));
    d->total_capacity = 800;

    for (int i = 0; i < MAX_SECTORS; i++)
        d->sector_capacity[i] = d->total_capacity / MAX_SECTORS;

    shmdt(d);
    return shmid;
}

int create_message_queue(void)
{
    key_t key = ftok(".", IPC_KEY + 1);
    if (key == -1) fatal_error("ftok msg");

    int msqid = msgget(key, IPC_CREAT | 0666);
    if (msqid == -1) fatal_error("msgget");

    return msqid;
}

void remove_shared_memory(int shmid)
{
    shmctl(shmid, IPC_RMID, NULL);
}

