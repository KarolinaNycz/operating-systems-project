#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <unistd.h>
#include <signal.h>

static int g_shmid = -1;
static int g_msqid = -1;
static int g_semid = -1;

static shared_data_t *d = NULL;

void cleanup(int sig)
{
    (void)sig;

    write(1, "\n[MANAGER] Sprzatanie...\n", 24);

    if (d) shmdt(d);
    if (g_msqid != -1)
        msgctl(g_msqid, IPC_RMID, NULL);

    if (g_shmid != -1)
        shmctl(g_shmid, IPC_RMID, NULL);

    if (g_semid != -1)
        semctl(g_semid, 0, IPC_RMID);
    
    _exit(0);
}

void evacuation_handler(int s)
{
    (void)s;

    if (d) d->evacuation = 1;

    write(STDOUT_FILENO,
          "[MANAGER] Ewakuacja\n", 20);
}

int main(void)
{
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGQUIT, cleanup);

    signal(SIG_EVACUATE, evacuation_handler);

    g_shmid = create_shared_memory();
    g_msqid = create_message_queue();
    g_semid = create_semaphore();


    d = shmat(g_shmid, NULL, 0);
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

    for (int i = 0; i < MAX_FANS; i++)
    {
        if (!fork())
        {
            execl("./fan", "fan", NULL);
            fatal_error("execl fan");
        }
    }


    if (!fork())
    {
        execl("./tech", "tech", NULL);
        fatal_error("execl tech");
    }

    msg_t msg;
    if (msgrcv(g_msqid, &msg, sizeof(msg) - sizeof(long), 0, 0) == -1) fatal_error("manager msgrcv");
    
    sleep(2);
    kill(0, SIG_EVACUATE);


    while (wait(NULL) > 0);

    cleanup(0);
    return 0;
}
