#include "common.h"

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
    signal(SIG_EVACUATE, evacuate);

    int vip = (rand() % 1000) < 3;
    int team = rand() % 2; //0=A, 1=B
    int patience = 5;
    int first_time = 1;

    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("fan shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("fan msgget");

    int semid = semget(ftok(".", IPC_KEY + 2), 1, 0);
    if (semid < 0) fatal_error("fan semget");

    d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("fan shmat");
    
    int my_id;

    sem_lock(semid);
    my_id = ++d->fan_counter;
    sem_unlock(semid);


    msg_t req, res;

    while (!d->evacuation && !evac_flag)
    {
        if (first_time && !evac_flag)
        {
            printf("[FAN %d] Kibic drużyny %c podchodzi do kasy\n",
                my_id,
                team == 0 ? 'A' : 'B');
            fflush(stdout);
            first_time = 0;
        }

        if (!vip && patience-- <= 0)
        {
            raise(SIGUSR1);
            break;
        }

        req.mtype = MSG_BUY_TICKET;
        req.pid = my_id;
        req.vip = vip;
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
            if (errno == EINTR && evac_flag) break;

            continue;
        }

        if (d->evacuation) break;


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

            if (msgsnd(msqid, &gate_req, sizeof(gate_req) - sizeof(long), 0) == -1)
            {
                if (errno == EINTR || errno == EIDRM) break;

                continue;
            }

            long my_type = MSG_GATE_RESPONSE + my_id;
            ssize_t gr = msgrcv( msqid, &gate_res, sizeof(gate_res) - sizeof(long), my_type, 0);

            if (gr == -1)
            {
                if (errno == EINTR && evac_flag) break;

                continue;
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

            sem_lock(semid);

            for (int i = 0; i < GATES_PER_SECTOR; i++)
            {
                if (d->gate_team[res.sector][i] == team && d->gate_count[res.sector][i] > 0)
                {
                    d->gate_count[res.sector][i]--;

                    if (d->gate_count[res.sector][i] == 0) d->gate_team[res.sector][i] = -1;

                    break;
                }
            }

            sem_unlock(semid);

            sleep(1);

            break;
        }


        sleep(1);
    }
    
    shmdt(d);
    return 0;
}
