#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <signal.h>
#include <sched.h>

volatile sig_atomic_t evac_flag = 0;

void evacuate(int sig)
{
    (void)sig;
    evac_flag = 1;
}

static shared_data_t *d = NULL;

void aggression(int sig)
{
    (void)sig;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    srand(getpid());

    signal(SIGUSR1, aggression);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = evacuate;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIG_EVACUATE, &sa, NULL);

    int has_flare = (rand() % 1000) < FLARE_PROB;
    int team = rand() % 2; //0=A, 1=B
    int patience = 5;
    int age;
    int guardian = 0;
    int first_time = 1;
    int queued = 0;
    int in_gate_queue = 0;

    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("fan shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("fan msgget");

    int semid = semget(ftok(".", IPC_KEY + 2), 3, 0);
    if (semid < 0) fatal_error("fan semget");

    d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("fan shmat");
    
    int my_id;

    if (sem_lock(semid, 2) != 0)
    {
        shmdt(d);
        return 0;
    }
    // 10% dzieci
    if ((rand() % 100) < 10)
    {
        age = 10 + rand() % 5; // 10-14
    }
    else
    {
        age = 18 + rand() % 40; // dorośli
    }

    my_id = ++d->fan_counter;
    d->gate_wait[my_id] = 0;

    if (age < 15)
    {
        guardian = d->last_adult_id;   // ostatni dorosły

        if (guardian == 0) guardian = 0;
    }
    else
    {
        // zapamiętujemy dorosłego
        d->last_adult_id = my_id;
    }

    if (age < 15)
    {
    printf("[FAN %d] Dziecko (%d lat), opiekun %d\n", my_id, age, guardian);
    fflush(stdout);
    }       

    if (sem_unlock(semid, 2) != 0)
    {
        shmdt(d);
        return 0;
    }

    int vip = 0;

    sem_lock(semid, 2);

    double limit = 0.003 * d->fan_counter;

    if (d->vip_count < limit && (rand() % 1000) < 3)
    {
        vip = 1;
        d->vip_count++;
    }

    sem_unlock(semid, 2);

    if (vip)
    {
        printf("[FAN %d] VIP (vip=%d / all=%d)\n", my_id, d->vip_count, d->fan_counter);
        fflush(stdout);
    }
    msg_t req, res;

    while (!evac_flag)
    {
        if (first_time && !evac_flag)
        {
            if (vip)
            {
                printf("[VIP %d] Podchodzi do kasy bez kolejki\n", my_id);
            }
            else
            {
                printf("[FAN %d] Kibic drużyny %c podchodzi do kasy\n", my_id, team == 0 ? 'A' : 'B');
            }

            fflush(stdout);
            first_time = 0;
        }
        
        if (!queued)
        {
            sem_lock(semid, 2);

            if (vip)
                d->vip_queue++;
            else
                d->ticket_queue++;

            sem_unlock(semid, 2);

            queued = 1;
        }

        if (!vip && patience-- <= 0)
        {
            raise(SIGUSR1);
            break;
        }

        if (vip)
        {
            req.mtype = MSG_BUY_TICKET_VIP;
        }
        else
        {
            req.mtype = MSG_BUY_TICKET;
        }

        req.pid = my_id;
        req.vip = vip;

        if (rand() % 100 < 10)
        {
            req.want_tickets = 2;
        }
        else 
        {
            req.want_tickets = 1;
        }

        req.has_flare = has_flare;
        req.team = team;
        req.age = age;
        req.guardian = guardian;

        if (vip)
        {
            req.sector = VIP_SECTOR;   // sektor VIP
        }
        else if (team == 0) // A
        {
            req.sector = rand() % 4;   // sektory 0-3
        }
        else // B
        {
            req.sector = 4 + rand() % 4; // sektory 4-7
        }

        printf("[FAN %d] Chce kupic bilet na sektor %d\n",
            my_id, req.sector);
        fflush(stdout);

        if (msgsnd(msqid, &req, sizeof(req) - sizeof(long), 0) == -1)
        {
            if (errno == EINTR || errno == EIDRM) break;

            continue;
        }

        ssize_t r = msgrcv( msqid, &res, sizeof(res) - sizeof(long), MSG_TICKET_OK + my_id, 0);

        if (r == -1)
        {
            if (errno == EINTR || errno == EIDRM || evac_flag) break;

            fatal_error("fan msgrcv ticket");
        }

        if (d->evacuation) break;

        if (res.tickets && vip)
        {
            printf("[VIP %d] Kupil bilet na sektor VIP\n", my_id);
            fflush(stdout);
        }

        if (queued)
        {
            sem_lock(semid, 2);

            if (vip && d->vip_queue > 0)
                d->vip_queue--;
            else if (!vip && d->ticket_queue > 0)
                d->ticket_queue--;

            sem_unlock(semid, 2);

            queued = 0;
        }

        if (res.tickets && vip)
        {
            printf("[VIP %d] Wchodzi bez kontroli do sektora VIP\n", my_id);
            fflush(stdout);
            break;
        }

        printf("[FAN %d] Kupil %d bilet(y)\n", my_id, res.tickets);
        fflush(stdout);

        if (res.tickets && !vip)
        {
            if (res.tickets == 2)
            {
                printf("[FAN %d] Wraz z osoba towarzyszaca ma bilety na sektor %d, szuka bramki...\n", my_id, res.sector);
            }
            else
            {
                printf("[FAN %d] Ma bilet na sektor %d, szuka bramki...\n", my_id, res.sector);
            }
            fflush(stdout);

            msg_t gate_req;

            int gate = rand() % GATES_PER_SECTOR;

            gate_req.mtype = MSG_GATE_BASE + res.sector * GATES_PER_SECTOR + gate;
            gate_req.pid = my_id;
            gate_req.team = team;
            gate_req.sector = res.sector;
            gate_req.has_flare = has_flare;
            gate_req.tickets = res.tickets;

            if (msgsnd(msqid, &gate_req, sizeof(gate_req) - sizeof(long), 0) == -1)
            {
                if (errno != EINTR && errno != EIDRM) perror("fan gate msgsnd");
            }

            if (!in_gate_queue)
            {
                sem_lock(semid, 1);
                queue_push(&d->gate_queue[res.sector], my_id);
                sem_unlock(semid, 1);

                in_gate_queue = 1;
            }

            msg_t gate_res;


            sched_yield();


            long my_type = MSG_GATE_RESPONSE + my_id;

            while (!evac_flag)
            {
                ssize_t gr = msgrcv(msqid, &gate_res, sizeof(gate_res) - sizeof(long), my_type, IPC_NOWAIT);

                if (gr >= 0) break;   // dostał odpowiedź

                if (errno == ENOMSG)
                {
                    sched_yield();   // czekaj spokojnie
                    continue;
                }

                if (errno == EINTR || errno == EIDRM) break;

                fatal_error("fan msgrcv gate");
            }

            if (d->evacuation) break;

            if (d->priority[my_id])
            {
                printf("[FAN %d] Zdenerwowany – dostalem priorytet\n", my_id);
                fflush(stdout);
            }
            
            if (d->evacuation) break;

            printf("[FAN %d] Wchodzi na hale\n", my_id);
            fflush(stdout);

            in_gate_queue = 0;

            break;
        }

    }
    
    shmdt(d);
    return 0;
}