#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdatomic.h>

#include "common.h"

volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t evac_flag = 0;

static int g_shmid = -1;
static int g_msqid = -1;
static int g_semid = -1;

static shared_data_t *d = NULL;

void cleanup(void)
{
    printf("\n[MANAGER] Zamykanie systemu...\n");
    fflush(stdout);

    kill(0, SIGTERM);

    pid_t pid;

    while (1)
    {
        pid = waitpid(-1, NULL, 0);

        if (pid > 0) continue;

        if (pid == -1 && errno == EINTR) continue;

        break;
    }


    if (d) shmdt(d);

    if (g_msqid != -1) msgctl(g_msqid, IPC_RMID, NULL);

    if (g_shmid != -1) shmctl(g_shmid, IPC_RMID, NULL);

    if (g_semid != -1) semctl(g_semid, 0, IPC_RMID);

    printf("[MANAGER] Cleanup zakonczony.\n");
}


void cleanup_handler(int sig)
{
    (void) sig;
    quit_flag = 1;
}

void evacuation_handler(int s)
{
    (void)s;

    evac_flag = 1;
    quit_flag = 1;

    write(STDOUT_FILENO, "[MANAGER] Ewakuacja\n", 20);
}

int main(void)
{
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    signal(SIGQUIT, cleanup_handler);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = evacuation_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIG_EVACUATE, &sa, NULL);


    g_shmid = create_shared_memory();
    g_msqid = create_message_queue();
    g_semid = create_semaphore();


    d = shmat(g_shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("shmat manager");

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
    while (1)
    {
        ssize_t r = msgrcv( g_msqid, &msg, sizeof(msg) - sizeof(long), 0, 0);

        if (r >= 0) break;

        if (errno == EINTR && evac_flag) break;

        if (errno == EINTR) continue;

        fatal_error("manager msgrcv");
    }

    sem_lock(g_semid);
    d->evacuation = 1;
    sem_unlock(g_semid);

    kill(0, SIG_EVACUATE);

    while (!quit_flag)
    {
        pause();
    }

    cleanup();
    return 0;

}
