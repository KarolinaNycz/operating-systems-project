#include "common.h"

int main(void)
{
    int shmid = create_shared_memory();

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

    while (wait(NULL) > 0);

    shmdt(d);
    remove_shared_memory(shmid);
    return 0;
}
