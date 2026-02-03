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

int main(int argc, char **argv) 
{
    if (argc < 3)
    {
        fprintf(stderr, "Tech: brak numeru sektora\n");
        exit(1);
    }

    int my_sector = atoi(argv[1]);
    int my_gate   = atoi(argv[2]);
    
    setvbuf(stdout, NULL, _IONBF, 0);

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
        long my_type = MSG_GATE_BASE + my_sector * GATES_PER_SECTOR + my_gate;

        ssize_t r = msgrcv(msqid, &req, sizeof(req) - sizeof(long), my_type, 0);

        if (r <= 0) continue;

        //printf("[TECH %d/%d] Obsługuję fana %d\n", my_sector, my_gate, req.pid);
        //fflush(stdout);

        if (d->evacuation) break;

        sem_lock(semid, 3 + req.sector);

        sem_unlock(semid, 3 + req.sector);

        int gate = -1;

        while (!d->evacuation && !evac_flag)
        {
            if (sem_lock(semid, 3 + req.sector) == -1) continue;

            int i = my_gate;
            
            int count = d->gate_count[req.sector][i];
            int gteam = d->gate_team[req.sector][i];

            // Zablokowany przez inny team → liczymy do priorytetu
            if (count > 0 && gteam != req.team && count < MAX_GATE_CAPACITY && !d->priority[req.pid])
            {
                d->gate_wait[req.pid]++;

                if (d->gate_wait[req.pid] >= 5)
                {
                    d->priority[req.pid] = 1;

                    printf("[TECH] Fan %d dostaje priorytet na bramce bezpieczenstwa\n", req.pid);
                    fflush(stdout);
                }
            }


            /* PRIORYTET: wchodzi zawsze gdy jest miejsce */
            if (d->priority[req.pid] && count < MAX_GATE_CAPACITY)
            {
                d->gate_count[req.sector][i]++;
                d->gate_team[req.sector][i] = req.team;
                gate = i;
                sem_unlock(semid, 3 + req.sector);
                break;
            }

            if (count == 0)
            {
                d->gate_team[req.sector][i] = req.team;
                d->gate_count[req.sector][i] = 1;
                gate = i;
                sem_unlock(semid, 3 + req.sector);
                break;
            }

            if (gteam == req.team && count < MAX_GATE_CAPACITY)
            {
                d->gate_count[req.sector][i]++;
                gate = i;
                sem_unlock(semid, 3 + req.sector);
                break;
            }

            printf("[TECH %d/%d] obsluguję fana %d\n", my_sector, my_gate, req.pid);
            fflush(stdout);

            if (gate != -1 || evac_flag)
            {
                sem_unlock(semid, 3 + req.sector);
                break;
            }    
        }
        
        if (gate == -1) continue;

        sem_lock(semid, 3 + req.sector);

        int f = queue_front(&d->gate_queue[req.sector]);

        if (f == req.pid)
        {
            queue_pop(&d->gate_queue[req.sector]);
        }

        d->gate_wait[req.pid] = 0;
        d->priority[req.pid] = 0;

        sem_unlock(semid, 3 + req.sector);

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

            if (sem_lock(semid, 3 + req.sector) == -1) break;


            // zwalnianie miejsca w bramce
            int i = my_gate;

            if (d->gate_team[req.sector][i] == req.team && d->gate_count[req.sector][i] > 0)
            {
                d->gate_count[req.sector][i]--;

                if (d->gate_count[req.sector][i] == 0) d->gate_team[req.sector][i] = -1;
            }

            sem_unlock(semid, 3 + req.sector);

            continue;
        }

        printf("[Bramka %d] Fan %d bezpieczny\n", gate, req.pid);
        fflush(stdout);

        /* ZWALNIAMY MIEJSCE NA BRAMCE */
        if (sem_lock(semid, 3 + req.sector) == -1) break;

        if (d->gate_count[req.sector][gate] > 0)
        {
            d->gate_count[req.sector][gate]--;

            if (d->gate_count[req.sector][gate] == 0) d->gate_team[req.sector][gate] = -1;
        }

        sem_unlock(semid, 3 + req.sector);

        res.mtype  = MSG_GATE_RESPONSE + req.pid;
        res.pid    = req.pid;
        res.tickets = gate;

        if (msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0) == -1)
        {
            if (errno != EINTR && errno != EIDRM) perror("tech send response");
        }

        sched_yield();


    }

    shmdt(d);

    return 0;
}
