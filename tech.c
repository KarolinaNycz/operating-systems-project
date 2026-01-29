#include "common.h"

static shared_data_t *d = NULL;

void evacuate(int s)
{
    (void)s;
    d->evacuation = 1;
}

int main(void) 
{
    signal(SIG_EVACUATE, evacuate);
    
    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("tech shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("tech msgget");

    int semid = semget(ftok(".", IPC_KEY + 2), 1, 0);
    if (semid < 0) fatal_error("tech semget");

    d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("tech shmat");
    msg_t req, res;

    while (!d->evacuation)
    {
        if (msgrcv(msqid, &req, sizeof(req)-sizeof(long), MSG_GATE_REQUEST, 0) == -1) continue;

        int gate = -1;

        while (!d->evacuation)
        {
            sem_lock(semid);

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

            sem_unlock(semid);

            if (gate != -1) break;

        }
        
        if (gate == -1) continue;

        printf("[Bramka %d] Sprawdzam fana %d (sektor %d, team %c)\n", 
            gate, req.pid, req.sector,
            req.team == 0 ? 'A' : 'B');
        fflush(stdout);

        printf("[Bramka %d] Fan %d bezpieczny\n",
                gate, req.pid);
        fflush(stdout);

        res.mtype = MSG_GATE_RESPONSE + req.pid;
        res.pid = req.pid;
        res.tickets = gate;

        msgsnd(msqid, &res, sizeof(res)-sizeof(long), 0);

    }

    shmdt(d);

    return 0;
}
