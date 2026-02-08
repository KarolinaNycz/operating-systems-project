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
static pid_t cashier_pids[MAX_CASHIERS];
static pid_t tech_pids[MAX_SECTORS][GATES_PER_SECTOR];

static shared_data_t *d = NULL;

void sigchld_handler(int sig)
{
    (void)sig;
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);
    
    errno = saved_errno;
}

void cleanup(void)
{
    printf("\n[MANAGER] Zamykanie systemu...\n");
    fflush(stdout);

    pid_t pid;
    int count = 0;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        count++;
    }
    
    if (count > 0)
    {
        printf("[MANAGER] Zebrano %d procesow zombie\n", count);
        fflush(stdout);
    }

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

void block_handler(int sig)
{
    (void)sig;

    if (d)
    {
        sem_lock(g_semid, 2);
        for (int i = 0; i < MAX_SECTORS; i++) 
            d->sector_blocked[i] = 1;
        sem_unlock(g_semid, 2);

        printf("[MANAGER] Blokada sektorow\n");
        fflush(stdout);
    }
}

void unblock_handler(int sig)
{
    (void)sig;

    if (d)
    {
        sem_lock(g_semid, 2);
        for (int i = 0; i < MAX_SECTORS; i++) 
            d->sector_blocked[i] = 0;
        sem_unlock(g_semid, 2);

        printf("[MANAGER] Odblokowanie sektorow\n");
        fflush(stdout);
    }
}

void alarm_handler(int sig)
{
    (void)sig;
    evac_flag = 1;
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    setpgid(0, 0);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    signal(SIGUSR1, block_handler);
    signal(SIGUSR2, unblock_handler);
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    signal(SIGQUIT, cleanup_handler);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = evacuation_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_EVACUATE, &sa, NULL);

    signal(SIGALRM, alarm_handler);

    g_shmid = create_shared_memory();
    g_msqid = create_message_queue();
    g_semid = create_semaphore();

    d = shmat(g_shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("shmat manager");

    memset(d->sector_reported, 0, sizeof(d->sector_reported));
    memset(d->sector_blocked, 0, sizeof(d->sector_blocked));
    memset(cashier_pids, 0, sizeof(cashier_pids));
    d->cashiers_closing = 0;

    // Uruchom kasjerów
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

    // Uruchom fanów
    for (int i = 0; i < MAX_FANS; i++)
    {
        if (!fork())
        {
            execl("./fan", "fan", NULL);
            fatal_error("execl fan");
        }
    }

    // Uruchom techów
    for (int s = 0; s < MAX_SECTORS; s++)
    {
        for (int g = 0; g < GATES_PER_SECTOR; g++)
        {
            pid_t p = fork();

            if (p == 0)
            {
                char buf1[8], buf2[8];
                sprintf(buf1, "%d", s);
                sprintf(buf2, "%d", g);
                execl("./tech", "tech", buf1, buf2, NULL);
                fatal_error("execl tech");
            }

            tech_pids[s][g] = p;
        }
    }

    // automatyczna ewakuacja po 8 sekundach
    alarm(8);

    msg_t msg;

    while (!evac_flag)
    {
        if (sem_lock(g_semid, 2) == -1) 
        {
            if (errno == EINTR) continue;
            break;
        }

        int total = 0;
        for (int i = 0; i < MAX_SECTORS; i++) 
            total += d->sector_taken[i];

        if (total >= d->total_capacity)
        {
            printf("[MANAGER] Stadion pelny - ewakuacja\n"); 
            evac_flag = 1;
            sem_unlock(g_semid, 2);
            break;
        }

        int K = d->ticket_queue;
        int N = d->active_cashiers;

        if (N < 2) N = 2;

        // zamykanie kas
        if (N > 2 && K < (K / 10.0) * (N - 1))
        {
            pid_t p = cashier_pids[N - 1];

            kill(p, SIGTERM);

            d->active_cashiers--;

            printf("[MANAGER] Zamykam kase (%d aktywnych)\n", d->active_cashiers);
        }

        // otwieranie kas
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

        sem_unlock(g_semid, 2);

    }

    if (sem_lock(g_semid, 2) == 0)
    {
        d->evacuation = 1;
        sem_unlock(g_semid, 2);
    }

    printf("[MANAGER] Ewakuacja - wysylam sygnal do grupy procesow\n");
    fflush(stdout);

    // Wyślij ewakuację do techów
    for (int s = 0; s < MAX_SECTORS; s++)
    {
        for (int g = 0; g < GATES_PER_SECTOR; g++)
        {
            if (tech_pids[s][g] > 0)
            {
                kill(tech_pids[s][g], SIG_EVACUATE);
            }
        }
    }

    printf("[MANAGER] Forsowne zamykanie fanow...\n");
    fflush(stdout);

    printf("[MANAGER] Czekam na potwierdzenia od %d sektorow...\n", MAX_SECTORS);
    fflush(stdout);

    int empty = 0;
    int iterations = 0;
    int max_iterations = 10000000;

    while (empty < MAX_SECTORS && iterations < max_iterations)
    {
        ssize_t r = msgrcv(g_msqid, &msg, sizeof(msg) - sizeof(long),  MSG_SECTOR_EMPTY, IPC_NOWAIT);
        
        if (r >= 0 && msg.mtype == MSG_SECTOR_EMPTY)
        {
            if (!d->sector_reported[msg.sector])
            {
                d->sector_reported[msg.sector] = 1;
                empty++;
                printf("[MANAGER] Sektor %d oprozniony (%d/%d)\n", 
                       msg.sector, empty, MAX_SECTORS);
                fflush(stdout);
            }
            iterations = 0;
        }
        else if (r == -1)
        {
            if (errno == ENOMSG)
            {
                sched_yield();
                iterations++;
                continue;
            }
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EIDRM)
            {
                printf("[MANAGER] Kolejka wiadomosci usunieta\n");
                fflush(stdout);
                break;
            }
        }
    }

    if (iterations >= max_iterations)
    {
        printf("[MANAGER] TIMEOUT - forsowne zamkniecie (otrzymano %d/%d)\n", empty, MAX_SECTORS);
        fflush(stdout);
    }

    printf("[MANAGER] Otrzymano wszystkie potwierdzenia\n");
    fflush(stdout);

    for (int s = 0; s < MAX_SECTORS; s++)
    {
        for (int g = 0; g < GATES_PER_SECTOR; g++)
        {
            if (tech_pids[s][g] > 0) kill(tech_pids[s][g], SIGTERM);
        }
    }

    int idx = -1;

    for (int i = MAX_CASHIERS - 1; i >= 0; i--)
    {
        if (cashier_pids[i] > 0)
        {
            idx = i;
            break;
        }
    }

    if (idx != -1)
    {
        kill(cashier_pids[idx], SIGTERM);
        cashier_pids[idx] = 0;
        d->active_cashiers--;
    }

    kill(-getpgrp(), SIGTERM);
    while (wait(NULL) > 0);

    cleanup();
    return 0;
}