#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <signal.h>
#include <sys/types.h>
#include <sched.h>

volatile sig_atomic_t evac_flag = 0;

static shared_data_t *d = NULL;

void evacuate(int sig)
{
    (void)sig;
    evac_flag = 1;
}

int main(void) 
{
    srand(getpid());
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = evacuate;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIG_EVACUATE, &sa, NULL);

    
    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("tech shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("tech msgget");

    int semid = semget(ftok(".", IPC_KEY + 2), 3, 0);
    if (semid < 0) fatal_error("tech semget");

    d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("tech shmat");
    msg_t req, res;

    while (!d->evacuation && !evac_flag)
    {
        ssize_t r = msgrcv( msqid, &req, sizeof(req) - sizeof(long), MSG_GATE_REQUEST, 0);

        if (r == -1)
        {
            if (errno == ENOMSG) continue;

            if (errno == EINTR || errno == EIDRM || evac_flag) break;

            fatal_error("tech msgrcv");
        }


        if (d->evacuation) break;

        int gate = -1;

        while (!d->evacuation && !evac_flag)
        {
            if (sem_lock(semid, 1) == -1) break;


            for (int i = 0; i < GATES_PER_SECTOR; i++)
            {
                int count = d->gate_count[req.sector][i];
                int gteam = d->gate_team[req.sector][i];

                // PRIORYTET - wchodzi zawsze gdy jest miejsce
                if (d->priority[req.pid] && (count < MAX_GATE_CAPACITY))
                {
                    d->gate_count[req.sector][i]++;
                    d->gate_team[req.sector][i] = req.team;

                    d->priority[req.pid] = 0;

                    gate = i;
                    break;
                }


                if (count == 0)
                {
                    d->gate_team[req.sector][i] = req.team;
                    d->gate_count[req.sector][i] = 1;
                    gate = i;
                    break;
                }

                if (gteam == req.team && count < MAX_GATE_CAPACITY)
                {
                    d->gate_count[req.sector][i]++;
                    gate = i;
                    break;
                }


            
            }
            


            if (gate == -1)
            {
                d->gate_queue[req.sector]++;
            }
            else
            {
                if (d->gate_queue[req.sector] > 0) d->gate_queue[req.sector]--;
            }

            if (d->gate_queue[req.sector] > 1 && !d->priority[req.pid])
            {
                d->priority[req.pid] = 1;
                printf("[TECH] Fan %d dostaje priorytet (kolejka=%d)\n", req.pid, d->gate_queue[req.sector]);
            }

            if (sem_unlock(semid, 1) == -1)
            {
                evac_flag = 1;
                break;
            }



            if (gate != -1 || evac_flag) break;

        }
        
        if (gate == -1) continue;

        printf("[Bramka %d] Sprawdzam fana %d (sektor %d, team %c)\n", 
            gate, req.pid, req.sector,
            req.team == 0 ? 'A' : 'B');
        fflush(stdout);

        // Sprawdzenie rac
        if (req.has_flare)
        {
            printf("[Bramka %d] ALARM! Fan %d ma race – WYPROWADZENIE Z BUDYNKU\n", gate, req.pid);
            fflush(stdout);

            kill(req.pid, SIGKILL);

            if (sem_lock(semid, 1) == -1) break;


            // zwalniamy miejsce w bramce
            for (int i = 0; i < GATES_PER_SECTOR; i++)
            {
                if (d->gate_team[req.sector][i] == req.team && d->gate_count[req.sector][i] > 0)
                {
                    d->gate_count[req.sector][i]--;

                    if (d->gate_count[req.sector][i] == 0)
                        d->gate_team[req.sector][i] = -1;

                    break;
                }
            }

            if (sem_unlock(semid, 1) == -1)
            {
                evac_flag = 1;
                break;
            }

            continue;
        }

        printf("[Bramka %d] Fan %d bezpieczny\n",
                gate, req.pid);
        fflush(stdout);

        res.mtype = MSG_GATE_RESPONSE + req.pid;
        res.pid = req.pid;
        res.tickets = gate;

        if (msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0) == -1)
        {
            if (errno == EINTR || errno == EIDRM) break;
        }

        sched_yield();


    }

    shmdt(d);

    return 0;
}
