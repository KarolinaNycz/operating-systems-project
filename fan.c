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
    srand(getpid());

    signal(SIGUSR1, aggression);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = evacuate;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIG_EVACUATE, &sa, NULL);

    int vip = (rand() % 1000) < 3;
    int has_flare = (rand() % 1000) < FLARE_PROB;
    int team = rand() % 2; //0=A, 1=B
    int patience = 5;
    int first_time = 1;
    int queued = 0;

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

    my_id = ++d->fan_counter;

    if (sem_unlock(semid, 2) != 0)
    {
        shmdt(d);
        return 0;
    }



    msg_t req, res;

    while (!evac_flag)
    {
        if (first_time && !evac_flag)
        {
            printf("[FAN %d] Kibic drużyny %c podchodzi do kasy\n",
                my_id,
                team == 0 ? 'A' : 'B');
            fflush(stdout);
            first_time = 0;
        }
        
        if (!queued)
        {
            if (sem_lock(semid, 2) == -1) break;

            d->ticket_queue++;
            if (sem_unlock(semid, 2) == -1) break;


            queued = 1;
        }



        if (!vip && patience-- <= 0)
        {
            raise(SIGUSR1);
            break;
        }

        req.mtype = MSG_BUY_TICKET;
        req.pid = my_id;
        req.vip = vip;
        req.has_flare = has_flare;
        req.team = team;

        if (team == 0) //A
            req.sector = rand() % (MAX_SECTORS / 2);
        else //B
            req.sector = (MAX_SECTORS / 2) + rand() % (MAX_SECTORS / 2);

        printf("[FAN %d] Chce kupic bilet na sektor %d\n",
            my_id, req.sector);
        fflush(stdout);

        if (msgsnd(msqid, &req, sizeof(req) - sizeof(long), 0) == -1)
        {
            if (errno == EINTR || errno == EIDRM) break;

            continue;
        }

        ssize_t r = msgrcv( msqid, &res, sizeof(res) - sizeof(long), MSG_TICKET_OK, 0);

        if (r == -1)
        {
            if (errno == EINTR || errno == EIDRM || evac_flag) break;

            fatal_error("fan msgrcv ticket");
        }



        if (d->evacuation) break;

        if (queued)
        {
            if (sem_lock(semid, 2) == -1) break;

            if (d->ticket_queue > 0) d->ticket_queue--;
            if (sem_unlock(semid, 2) == -1) break;


            queued = 0;
        }

        if (res.tickets)
        {

            printf("[FAN %d] Ma bilet na sektor %d, szuka bramki...\n",
                my_id, res.sector);
            fflush(stdout);
            
            msg_t gate_req, gate_res;

            gate_req.mtype = MSG_GATE_REQUEST;
            gate_req.pid = my_id;
            gate_req.team = team;
            gate_req.sector = res.sector;
            gate_req.has_flare = has_flare; 

            if (msgsnd(msqid, &gate_req, sizeof(gate_req) - sizeof(long), 0) == -1)
            {
                if (errno == EINTR || errno == EIDRM) break;

                continue;
            }

            sched_yield();


            long my_type = MSG_GATE_RESPONSE + my_id;
            ssize_t gr = msgrcv( msqid, &gate_res, sizeof(gate_res) - sizeof(long), my_type, 0);

            if (gr == -1)
            {
                if (errno == EINTR || errno == EIDRM || evac_flag) break;

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

            if (sem_lock(semid, 1) == -1) break;


            for (int i = 0; i < GATES_PER_SECTOR; i++)
            {
                if (d->gate_team[res.sector][i] == team && d->gate_count[res.sector][i] > 0)
                {
                    d->gate_count[res.sector][i]--;

                    if (d->gate_count[res.sector][i] == 0) d->gate_team[res.sector][i] = -1;

                    break;
                }
            }

            if (sem_unlock(semid, 1) == -1) break;


            break;
        }

    }
    
    shmdt(d);
    return 0;
}
