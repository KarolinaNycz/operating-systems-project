#include "common.h"

static void stop_handler(int sig)
{
    (void)sig;
    _exit(0);
}

int main(void)
{
    signal(SIG_STOP_WORK, stop_handler);

    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("cashier shmget");

    shared_data_t *d = shmat(shmid, NULL, 0);
    (void)d;

    sleep(1);
    return 0;
}
