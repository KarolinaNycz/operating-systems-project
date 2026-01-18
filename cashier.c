#include "common.h"


int main(void)
{
    srand(getpid());

    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("cashier shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("cashier msgget");

    shared_data_t *d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("cashier shmat");

    msg_t req, res;

    while (!d->evacuation)
    {
        if (msgrcv(msqid, &req, sizeof(req) - sizeof(long), MSG_BUY_TICKET, 0) == -1) continue;
        
        int sector = rand() % MAX_SECTORS;

        res.mtype = MSG_TICKET_OK;
        res.pid = req.pid;
        res.sector = sector;

        if (d->sector_taken[sector] < d->sector_capacity[sector])
        {
            d->sector_taken[sector]++;
            res.tickets = 1;
        }
        else
        {
            res.tickets = 0;
        }

        msgsnd(msqid, &res, sizeof(res) - sizeof(long), 0);
    }

    shmdt(d);
    return 0;
}
