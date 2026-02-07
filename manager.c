#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdatomic.h>
#include <sched.h>

#include "common.h"

volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t evac_flag = 0;

static int g_shmid = -1;
static int g_msqid = -1;
static int g_semid = -1;
static pid_t cashier_pids[10];

static shared_data_t *d = NULL;

void cleanup(void)
{
    printf("\n[MANAGER] Zamykanie systemu...\n");
    fflush(stdout);

    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0);

    if (d) shmdt(d);

    if (g_msqid != -1) msgctl(g_msqid, IPC_RMID, NULL);

    if (g_shmid != -1) shmctl(g_shmid, IPC_RMID, NULL);

    if (g_semid != -1) semctl(g_semid, 0, IPC_RMID);

    printf("[MANAGER] Cleanup zakonczony.\n");
}


void cleanup_handler(int sig)
{
    (void)sig;

    quit_flag = 1;
    evac_flag = 1;
}


void evacuation_handler(int s)
{
    (void)s;

    if (evac_flag) return;

    evac_flag = 1;
    quit_flag = 1;

    write(STDOUT_FILENO, "[MANAGER] Ewakuacja\n", 20);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    signal(SIGQUIT, cleanup_handler);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = evacuation_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIG_EVACUATE, &sa, NULL);


    g_shmid = create_shared_memory();
    g_msqid = create_message_queue();
    g_semid = create_semaphore();


    d = shmat(g_shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("shmat manager");

    for (int i = 0; i < MIN_CASHIERS; i++)
    {
        pid_t p = fork();

        if (p == 0)
        {
            execl("./cashier", "cashier", NULL);
            fatal_error("execl cashier");
        }

        cashier_pids[i] = p;
    }


    for (int i = 0; i < MAX_FANS; i++)
    {
        if (!fork())
        {
            execl("./fan", "fan", NULL);
            fatal_error("execl fan");
        }
    }


    for (int s = 0; s < MAX_SECTORS; s++)
    {
        for (int g = 0; g < GATES_PER_SECTOR; g++)
        {
            if (!fork())
            {
                char buf1[8], buf2[8];

                sprintf(buf1, "%d", s);
                sprintf(buf2, "%d", g);

                execl("./tech", "tech", buf1, buf2, NULL);
                fatal_error("execl tech");
            }
        }
    }



    msg_t msg;

    while (!evac_flag)
    {
        if (sem_lock(g_semid, 2) == -1) break;

        int total = 0;

        for (int i = 0; i < MAX_SECTORS; i++) total += d->sector_taken[i];

        if (total >= d->total_capacity)
        {
            printf("[MANAGER] Stadion pelny – ewakuacja\n"); 
            evac_flag = 1;
            break;
        }

        int K = d->ticket_queue;
        int N = d->active_cashiers;

        if (N < 2) N = 2;

        // zamykanie
        if (N > 2 && K < (K / 10.0) * (N - 1))
        {
            pid_t p = cashier_pids[N - 1];

            kill(p, SIGTERM);

            d->active_cashiers--;

            printf("[MANAGER] Zamykam kase (%d aktywnych)\n", d->active_cashiers);
        }

        // otwieranie
        if (N < 10 && K > 20)
        {
            pid_t p = fork();

            if (p == 0)
            {
                execl("./cashier", "cashier", NULL);
                fatal_error("execl cashier");
            }

            cashier_pids[N] = p;
            d->active_cashiers++;

            printf("[MANAGER] Otwieram kase (%d aktywnych)\n", d->active_cashiers);
        }

        if (sem_unlock(g_semid, 2) == -1) break;

        ssize_t r = msgrcv( g_msqid, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);

        if (r >= 0 && msg.mtype == MSG_SECTOR_EMPTY) break;

        if (r == -1)
        {
            if (errno == ENOMSG || errno == EINTR || errno == EIDRM)
            {
            
            }

            else
            {
                fatal_error("manager msgrcv");
            }
        }

        sched_yield();
    }

    if (sem_lock(g_semid, 2) == 0)
    {
        d->evacuation = 1;
        sem_unlock(g_semid, 2);
    }



    printf("[MANAGER] Ewakuacja\n");
    fflush(stdout);

    kill(-getpgrp(), SIG_EVACUATE);
    cleanup();
    return 0;

}