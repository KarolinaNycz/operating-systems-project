#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <signal.h>
#include <sched.h>


volatile sig_atomic_t evac_flag = 0;
volatile sig_atomic_t quit_flag = 0;

void quit_handler(int sig)
{
    (void)sig;
    quit_flag = 1;
}

void evacuate(int sig)
{
    (void)sig;
    evac_flag = 1;
}

int main(void)
{
    struct sigaction sb;
    memset(&sb, 0, sizeof(sb));

    sb.sa_handler = quit_handler;
    sigemptyset(&sb.sa_mask);
    sb.sa_flags = 0;

    sigaction(SIGTERM, &sb, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = evacuate;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIG_EVACUATE, &sa, NULL);


    srand(getpid());

    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("cashier shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("cashier msgget");

    int semid = semget(ftok(".", IPC_KEY + 2), 3, 0);
    if (semid < 0) fatal_error("cashier semget");


    shared_data_t *d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("cashier shmat");

    msg_t req, res;

    while (!d->evacuation && !evac_flag && !quit_flag)
    {
        ssize_t r = msgrcv( msqid, &req, sizeof(req) - sizeof(long), MSG_BUY_TICKET, 0);
        if (r == -1)
        {
            if (errno == ENOMSG) continue;

            if (errno == EINTR || errno == EIDRM || evac_flag || quit_flag) break;

            fatal_error("cashier msgrcv");
        }


        if (d->evacuation) break;
        
        printf("[CASHIER] Fan %d (Team %c) chce sektor %d\n",
            req.pid,
            req.team == 0 ? 'A' : 'B',
            req.sector);
        fflush(stdout);

        int sector = req.sector;

        res.mtype = MSG_TICKET_OK;
        res.pid = req.pid;
        res.team = req.team;
        res.sector = sector;
        
        if (sem_lock(semid, 0) == -1) break;

        if (d->sector_taken[sector] < d->sector_capacity[sector])
        {
            d->sector_taken[sector]++;
            res.tickets = 1;
        }
        else
        {
            res.tickets = 0;
        }

        if (sem_unlock(semid, 0) == -1)
        {
            evac_flag = 1;
            break;
        }

        
        if (res.tickets)
        {
            printf("[CASHIER] Sprzedano bilet fanowi %d na sektor %d\n", req.pid, sector);
        }
        else
        {
            printf("[CASHIER] Brak miejsc w sektorze %d dla fana %d\n", sector, req.pid);
        }
        fflush(stdout);

        if (msgsnd(msqid, &res, sizeof(res) - sizeof(long), 0) == -1)
        {
            if (errno == EINTR || errno == EIDRM) break;
        }

        sched_yield();


    }

    shmdt(d);
    return 0;
}
