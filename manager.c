#include "common.h"

int main(void)
{
    int shmid = create_shared_memory();
    int msqid = create_message_queue();
    int semid = create_semaphore();

    shared_data_t *d = shmat(shmid, NULL, 0);
    if (d == (void *)-1)
        fatal_error("shmat manager");

    for (int i = 0; i < MIN_CASHIERS; i++)
    {
        if (!fork())
	    {
            execl("./cashier", "cashier", NULL);
            fatal_error("execl cashier");
        }
    }

    if (!fork())
    {
        execl("./tech", "tech", NULL);
        fatal_error("execl tech");
    }

    msg_t msg;
    msgrcv(msqid, &msg, sizeof(msg) - sizeof(long), 0, 0);
    
    sleep(2);
    kill(0, SIG_STOP_WORK);

    while (wait(NULL) > 0);

    shmdt(d);
    msgctl(msqid, IPC_RMID, NULL);
    remove_semaphore(semid);
    remove_shared_memory(shmid);
    return 0;
}
