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

    int semid = semget(ftok(".", IPC_KEY + 2), 4 + MAX_SECTORS, 0);
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

    logp("[CASHIER] Uruchomiony\n");

    msg_t req, res;

    while (!quit_flag)
    {
        if (d->evacuation || evac_flag)
        {
            logp("[CASHIER] STOP sprzedazy – ewakuacja\n");
            break;
        }

        //Sprawdz czy wszystkie bilety zostaly sprzedane
        if (sem_lock(semid, 3) == 0)
        {
            int sold = d->total_tickets_sold;
            int capacity = d->total_capacity;
            sem_unlock(semid, 3);
        
            if (sold >= capacity)
            {
                logp("[CASHIER] Wszystkie bilety sprzedane (%d/%d) - zamykam kase\n", sold, capacity);
                break;
            }
        }

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

        if (d->evacuation || evac_flag)
        {
            logp("[CASHIER] Przerywam sprzedaz – ewakuacja\n");

            res.mtype = MSG_TICKET_OK +req.pid;
            res.tickets = 0;
            msgsnd(msqid, &res, sizeof(res) - sizeof(long), IPC_NOWAIT);

            break;
        }

        logp("[CASHIER] Fan %d (Team %c) chce sektor %d\n", req.pid, req.team == 0 ? 'A' : 'B', req.sector);

        // Sprawdzanie dziecka
        if (req.age < 15)
        {
            if (req.guardian == 0)
            {
                logp("[CASHIER] Dziecko %d bez opiekuna - odmowa\n", req.pid);

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

            logp("[CASHIER] Dziecko %d z opiekunem %d\n", req.pid, req.guardian);
        }

        if (req.vip)
        {
            logp("[CASHIER] Obsluguje VIP-a %d...\n", req.pid);
        }

        int sector = req.sector;

        if (!req.vip && sector == VIP_SECTOR)
        {
            logp("[CASHIER] Fan %d (non-VIP) nie może kupić na sektor VIP %d - szukam alternatywy\n", req.pid, VIP_SECTOR);
            sector = -1;
        }

        res.mtype = MSG_TICKET_OK + req.pid;
        res.pid = req.pid;
        res.team = req.team;
        res.sector = sector;
        res.tickets = 0;
        
        if (sector != -1 && sem_lock(semid, 4 + sector) == 0)
        {
            if (d->evacuation || evac_flag)
            {
                sem_unlock(semid, 4 + sector);
                break;
            }

            int free = d->sector_capacity[sector] - d->sector_tickets_sold[sector];

            if (free >= req.want_tickets)
            {
                //Jest miejsce - sprzedaj
                d->sector_tickets_sold[sector] += req.want_tickets;

                if (sem_lock(semid, 3) == 0)
                {
                    d->total_tickets_sold += req.want_tickets;
                    sem_unlock(semid, 3);
                }

                res.tickets = req.want_tickets;
                res.sector = sector;
                
                int current_sold = d->sector_tickets_sold[sector];
                int current_capacity = d->sector_capacity[sector];
                
                sem_unlock(semid, 4 + sector);
                
                logp("[CASHIER] Sprzedano %d bilet(y) fanowi %d na sektor %d (zajete: %d/%d)\n", res.tickets, req.pid, sector, current_sold, current_capacity);
            }
            else
            {
                //Brak miejsca - szukaj alternatywnego
                sem_unlock(semid, 4 + sector);
                
                int found = 0;
                
                for (int s = 0; s < MAX_SECTORS; s++)
                {
                    if (s == sector) continue;

                    if (!req.vip && s == VIP_SECTOR) continue;

                    if (req.team == 0 && s >= 4) continue;
                    if (req.team == 1 && s < 4) continue;
                    
                    //Zablokuj semafor nowego sektora
                    if (sem_lock(semid, 4 + s) != 0) continue;
                    
                    if (d->evacuation || evac_flag)
                    {
                        sem_unlock(semid, 4 + s);
                        break;
                    }
                    
                    int f = d->sector_capacity[s] - d->sector_tickets_sold[s];
                    
                    if (f >= req.want_tickets)
                    {
                        //Znaleziono - sprzedaj
                        d->sector_tickets_sold[s] += req.want_tickets;

                        if (sem_lock(semid, 3) == 0)
                        {
                            d->total_tickets_sold += req.want_tickets;
                            sem_unlock(semid, 3);
                        }

                        res.tickets = req.want_tickets;
                        res.sector = s;
                        
                        int current_taken = d->sector_taken[s];
                        int current_capacity = d->sector_capacity[s];
                        
                        sem_unlock(semid, 4 + s);
                        
                        logp("[CASHIER] Brak miejsca w sektorze %d dla fana %d – sprzedano bilet na sektor %d (zajete: %d/%d)\n", sector, req.pid, s, current_taken, current_capacity);
                        
                        found = 1;
                        break;
                    }
                    
                    sem_unlock(semid, 4 + s);
                }
                
                if (!found)
                {
                    res.tickets = 0;
                    logp("[CASHIER] Brak miejsc dla fana %d (wszystkie sektory pelne)\n", req.pid);
                }
            }
        }
        else
        {
            logp("[CASHIER] Nie udalo sie zablokowac semafora sektora %d\n", sector);
            res.tickets = 0;
        }

        if (d->evacuation || evac_flag)
        {
            res.tickets = 0;
            msgsnd(msqid, &res, sizeof(res)-sizeof(long), IPC_NOWAIT);
            break;
        }

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

        sched_yield();;

    }

    //Kasjer informuje o zamknięciu
    if (!evac_flag && !d->evacuation)
    {
        if (sem_lock(semid, 3) == 0)
        {
            if (d->active_cashiers > 0)
            {
                d->active_cashiers--;
            }
            d->cashiers_closing = 0;
            logp("[CASHIER] Zamykam sie (aktywnych: %d)\n", d->active_cashiers);
            sem_unlock(semid, 3);
        }
    }
    else
    {
        dprintf(STDOUT_FILENO, "[CASHIER] Koncze prace (ewakuacja)\n");

    }

    shmdt(d);
    return 0;
}