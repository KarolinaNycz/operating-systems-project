#include "common.h"
#include <stdarg.h>
#include <math.h>

#define LOG_FILE "raport.txt"

void fatal_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int create_shared_memory(int total_fans)
{
    key_t key = ftok(".", IPC_KEY);
    if (key == -1) fatal_error("ftok");

    int shmid = shmget(key, sizeof(shared_data_t), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid == -1) fatal_error("shmget");

    shared_data_t *d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("shmat");

    memset(d, 0, sizeof(*d));

    int vip_capacity = (int)ceil(0.003 * total_fans);

    for (int i = 0; i < MAX_SECTORS; i++)
    {
        if (i == VIP_SECTOR)
        {
            d->sector_capacity[i] = vip_capacity;
        }
        else
        {
            d->sector_capacity[i] = SECTOR_CAPACITY;
        }
        
        d->sector_tickets_sold[i] = 0;
        d->sector_taken[i] = 0;
    }

    d->total_capacity = (MAX_SECTORS - 1) * SECTOR_CAPACITY + vip_capacity;
    
    logp("[SYSTEM] Capacity VIP (sektor %d): %d miejsc (0.3%% z %d fanow)\n", VIP_SECTOR, vip_capacity, total_fans);
    logp("[SYSTEM] Total capacity: %d miejsc\n", d->total_capacity);

    d->active_cashiers = MIN_CASHIERS;
    d->ticket_queue = 0;
    d->total_taken = 0;
    d->total_tickets_sold = 0;
    d->vip_count = 0;
    d->vip_queue = 0;
    d->last_adult_id = 0;
    d->cashiers_closing = 0;

    time_t now = time(NULL);
    d->match_start_time = now + MATCH_START_DELAY;
    d->match_end_time = d->match_start_time + MATCH_DURATION;

    for (int s = 0; s < MAX_SECTORS; s++)
    {
        for (int g = 0; g < GATES_PER_SECTOR; g++)
        {
            d->gate_team[s][g] = -1;
        }
    }

    for (int i = 0; i < MAX_SECTORS; i++)
    {
        d->gate_queue[i].head = 0;
        d->gate_queue[i].tail = 0;
        d->gate_queue[i].size = 0;
    }

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

int create_semaphore(void)
{
    key_t key = ftok(".", IPC_KEY + 2);
    if (key == -1) fatal_error("ftok sem");

    int semid = semget(key, 4 + MAX_SECTORS, IPC_CREAT | 0666);

    if (semid == -1) fatal_error("semget");

    union semun arg;

    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    arg.val = 1;
    semctl(semid, 1, SETVAL, arg);

    arg.val = 1;
    semctl(semid, 2, SETVAL, arg); //evacuation + sector_blocked

    arg.val = 1;
    semctl(semid, 3, SETVAL, arg);

    for (int i = 0; i < MAX_SECTORS; i++)
    {
        arg.val = 1;
        semctl(semid, 4 + i, SETVAL, arg);
    }

    return semid;
}

void remove_shared_memory(int shmid)
{
    shmctl(shmid, IPC_RMID, NULL);
}

void remove_semaphore(int semid)
{
    semctl(semid, 0, IPC_RMID);
}

void logp(const char *format, ...)
{
    int semid = semget(ftok(".", IPC_KEY + 2), 0, 0);

    if (semid != -1) sem_lock(semid, 0);

    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    fflush(stdout);

    FILE *fp = fopen(LOG_FILE, "a");

    if (fp)
    {
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);

        fflush(fp);
        fclose(fp);
    }


    if (semid != -1) sem_unlock(semid, 0);
}