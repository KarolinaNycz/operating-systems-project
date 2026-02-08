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
    setvbuf(stdout, NULL, _IONBF, 0);

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
    if (shmid < 0) 
    {
        perror("cashier shmget");
        return 1;
    }

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) 
    {
        perror("cashier msgget");
        return 1;
    }

    int semid = semget(ftok(".", IPC_KEY + 2), 3 + MAX_SECTORS, 0);
    if (semid < 0) 
    {
        perror("cashier semget");
        return 1;
    }

    shared_data_t *d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) 
    {
        perror("cashier shmat");
        return 1;
    }

    printf("[CASHIER] Uruchomiony\n");
    fflush(stdout);

    msg_t req, res;

    while (!d->evacuation && !evac_flag && !quit_flag)
    {
        // Priorytet dla VIP
        ssize_t r = msgrcv(msqid, &req, sizeof(req) - sizeof(long), MSG_BUY_TICKET_VIP, IPC_NOWAIT);

        if (r == -1)
        {
            r = msgrcv(msqid, &req, sizeof(req) - sizeof(long), MSG_BUY_TICKET, IPC_NOWAIT);
        }

        if (r == -1)
        {
            if (errno == EINTR || errno == EIDRM) 
            {
                break;
            }
            if (errno == ENOMSG) 
            {
                sched_yield();
                continue;
            }
            
            perror("cashier msgrcv");
            break;
        }

        if (d->evacuation) break;
        
        printf("[CASHIER] Fan %d (Team %c) chce sektor %d\n", req.pid, req.team == 0 ? 'A' : 'B', req.sector);
        fflush(stdout);

        // Sprawdzanie dziecka
        if (req.age < 15)
        {
            if (req.guardian == 0)
            {
                printf("[CASHIER] Dziecko %d bez opiekuna - odmowa\n", req.pid);
                fflush(stdout);

                res.mtype = MSG_TICKET_OK + req.pid;
                res.tickets = 0;
                
                if (msgsnd(msqid, &res, sizeof(res) - sizeof(long), IPC_NOWAIT) == -1)
                {
                    if (errno != EINTR && errno != EIDRM && errno != EAGAIN)
                    {
                        perror("cashier msgsnd");
                    }
                }
                continue;
            }

            printf("[CASHIER] Dziecko %d z opiekunem %d\n", req.pid, req.guardian);
            fflush(stdout);
        }

        if (req.vip)
        {
            printf("[CASHIER] Obsluguje VIP-a %d...\n", req.pid);
            fflush(stdout);
        }

        int sector = req.sector;

        res.mtype = MSG_TICKET_OK + req.pid;
        res.pid = req.pid;
        res.team = req.team;
        res.sector = sector;
        
        // Używamy semafora SEKTOROWEGO
        if (sem_lock(semid, 3 + sector) == -1) 
        {
            break;
        }

        int free = d->sector_capacity[sector] - d->sector_taken[sector];

        if (free >= req.want_tickets)
        {
            d->sector_taken[sector] += req.want_tickets;
            res.tickets = req.want_tickets;
        }
        else if (free > 0)
        {
            d->sector_taken[sector] += free;
            res.tickets = free;
        }
        else
        {
            res.tickets = 0;
        }

        sem_unlock(semid, 3 + sector);

        if (res.tickets)
        {
            printf("[CASHIER] Sprzedano %d bilet(y) fanowi %d na sektor %d (zajete: %d/%d)\n", 
                   res.tickets, req.pid, sector, d->sector_taken[sector], d->sector_capacity[sector]);
        }
        else
        {
            printf("[CASHIER] Brak miejsc w sektorze %d dla fana %d\n", sector, req.pid);
        }
        fflush(stdout);

        if (msgsnd(msqid, &res, sizeof(res) - sizeof(long), IPC_NOWAIT) == -1)
        {
            if (errno == EINTR || errno == EIDRM) 
            {
                break;
            }
            if (errno != EAGAIN)
            {
                perror("cashier msgsnd");
            }
        }

        sched_yield();
    }

    // ✅ FIX: Kasjer informuje o zamknięciu
    if (quit_flag)
    {
        if (sem_lock(semid, 2) == 0)
        {
            if (d->active_cashiers > 0)
            {
                d->active_cashiers--;
            }
            d->cashiers_closing = 0;
            printf("[CASHIER] Zamykam sie (aktywnych: %d)\n", d->active_cashiers);
            fflush(stdout);
            sem_unlock(semid, 2);
        }
    }
    else if (evac_flag || d->evacuation)
    {
        dprintf(STDOUT_FILENO, "[CASHIER] Koncze prace (ewakuacja)\n");

    }

    shmdt(d);
    return 0;
}