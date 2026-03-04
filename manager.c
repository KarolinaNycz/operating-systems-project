#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdatomic.h>
#include <sched.h>

#include "common.h"

volatile sig_atomic_t quit_flag = 0;
volatile sig_atomic_t evac_flag = 0;
volatile sig_atomic_t soft_evac = 0;

static int g_shmid = -1;
static int g_msqid = -1;
static int g_semid = -1;
static pid_t cashier_pids[MAX_CASHIERS];
static pid_t tech_pids[MAX_SECTORS][GATES_PER_SECTOR];
static pid_t fan_pids[MAX_FANS];

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
    logp("\n[MANAGER] Zamykanie systemu...\n");

    pid_t pid;
    int count = 0;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        count++;
    }
    
    if (count > 0)
    {
        logp("[MANAGER] Zebrano %d procesow zombie\n", count);
    }

    if (d) shmdt(d);

    if (g_msqid != -1) msgctl(g_msqid, IPC_RMID, NULL);
    if (g_shmid != -1) shmctl(g_shmid, IPC_RMID, NULL);
    if (g_semid != -1) semctl(g_semid, 0, IPC_RMID);

    logp("[MANAGER] Cleanup zakonczony.\n");
}

void cleanup_handler(int sig)
{
    (void)sig;
    quit_flag = 1;
    evac_flag = 1;
}

void end_match_handler(int sig)
{
    (void)sig;
    quit_flag = 1;
    evac_flag = 1;
    soft_evac = 1;
}

void evacuation_handler(int s)
{
    (void)s;
    if (evac_flag) return;

    evac_flag = 1;
    quit_flag = 1;

    write(STDOUT_FILENO, "[MANAGER] Ewakuacja\n", 20);
}

void block_handler(int sig, siginfo_t *info, void *ctx)
{
    (void)sig;
    (void)ctx;
    int sector = info->si_value.sival_int;

    if (d)
    {
        sem_lock(g_semid, 2);
        if (sector >= 0 && sector < MAX_SECTORS)
        {
            d->sector_blocked[sector] = 1;
            logp("[MANAGER] Blokada sektora %d\n", sector);
        }
        else
        {
            for (int i = 0; i < MAX_SECTORS; i++) d->sector_blocked[i] = 1;
            logp("[MANAGER] Blokada wszystkich sektorow\n");
        }
        sem_unlock(g_semid, 2);
    }
}

void unblock_handler(int sig, siginfo_t *info, void *ctx)
{
    (void)sig;
    (void)ctx;
    int sector = info->si_value.sival_int;

    if (d)
    {
        sem_lock(g_semid, 2);
        if (sector >= 0 && sector < MAX_SECTORS)
        {
            d->sector_blocked[sector] = 0;
            logp("[MANAGER] Odblokowanie sektora %d\n", sector);
        }
        else
        {
            for (int i = 0; i < MAX_SECTORS; i++) d->sector_blocked[i] = 0;
            logp("[MANAGER] Odblokowanie wszystkich sektorow\n");
        }
        sem_unlock(g_semid, 2);
    }
}

void alarm_handler(int sig)
{
    (void)sig;
    evac_flag = 1;
}


int main(void)
{
    FILE *fp = fopen("raport.txt", "w");
    if (fp) fclose(fp);

    setvbuf(stdout, NULL, _IONBF, 0);

    setpgid(0, 0);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    struct sigaction sa_usr1;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_sigaction = block_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    struct sigaction sa_usr2;
    memset(&sa_usr2, 0, sizeof(sa_usr2));
    sa_usr2.sa_sigaction = unblock_handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR2, &sa_usr2, NULL);

    signal(SIGINT, cleanup_handler);  //CTRL + C
    signal(SIGTERM, end_match_handler); //SYGNAL 3
    signal(SIGQUIT, cleanup_handler); //CTRL + \//

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = evacuation_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_EVACUATE, &sa, NULL);

    signal(SIGALRM, alarm_handler);

    g_shmid = create_shared_memory(MAX_FANS);
    g_msqid = create_message_queue();
    g_semid = create_semaphore();

    d = shmat(g_shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("shmat manager");

    memset(d->sector_reported, 0, sizeof(d->sector_reported));
    memset(d->sector_blocked, 0, sizeof(d->sector_blocked));
    memset(cashier_pids, 0, sizeof(cashier_pids));
    memset(fan_pids, 0, sizeof(fan_pids));
    d->cashiers_closing = 0;

    srand(time(NULL));

    int fans_created = 0;
    time_t last_fan_creation = time(NULL);

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
    d->active_cashiers = MIN_CASHIERS;

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

    /*//Utworz fanów na raz
    for (int i = 0; i < MAX_FANS; i++)
    {
        pid_t p = fork();
        if (p == 0) { execl("./fan", "fan", NULL); fatal_error("execl fan"); }
        fan_pids[i] = p;
    }
    logp("[MANAGER] Utworzono wszystkich %d fanow\n", MAX_FANS);*/
    //fans_created = MAX_FANS;

    msg_t msg;

    int match_started_logged = 0;
    int drained = 0;
    int all_sold_logged = 0;
    
    while (!evac_flag)
    {

        time_t current_time = time(NULL);
        
        // Sprawdź czy mecz się rozpoczął
        if (!match_started_logged && current_time >= d->match_start_time)
        {
            logp("[MANAGER] *** MECZ SIE ROZPOCZAL ***\n");
            match_started_logged = 1;
        }
        
        // Sprawdź czy mecz się zakończył - EWAKUACJA
        if (current_time >= d->match_end_time)
        {
            logp("[MANAGER] *** MECZ SIE ZAKONCZYL - ZAMYKAM STADION ***\n");
            soft_evac = 1;
            evac_flag = 1;
            break;
        }

        // Uruchom fanów
        if (fans_created < MAX_FANS)
        {
            time_t current = time(NULL);

            if (current - last_fan_creation >= 1)
            {
                int to_create = 5000 + rand() % 10;
                
                for (int i = 0; i < to_create && fans_created < MAX_FANS; i++)
                {
                    pid_t p = fork();
                    if (p == 0)
                    {
                        execl("./fan", "fan", NULL);
                        fatal_error("execl fan");
                    }
                    fan_pids[fans_created] = p;
                    fans_created++;
                }
                
                last_fan_creation = current;
                logp("[MANAGER] %d/%d fanow na hali\n", fans_created, MAX_FANS);
            }
        }

        if (sem_lock(g_semid, 3) != 0) 
        {
            if (evac_flag) break;
            continue;
        }

        int all_sold = 0;
    
        if (d->total_tickets_sold >= d->total_capacity)
        {
            all_sold = 1;

            if (!all_sold_logged)
            {
                logp("[MANAGER] Wszystkie bilety sprzedane (%d/%d) - zamykam kasy\n", d->total_tickets_sold, d->total_capacity);
                all_sold_logged = 1;
            }

            if (d->active_cashiers > 0) 
            {
                for (int i = 0; i < MAX_CASHIERS; i++) if (cashier_pids[i] > 0) kill(cashier_pids[i], SIGTERM);
                d->active_cashiers = 0;
            }
                
            sem_unlock(g_semid, 3);

            if (!drained)
            {
                // Odsyla odmowy wszystkim czekajacym fanom
                msg_t drain, rej;
                while (msgrcv(g_msqid, &drain, sizeof(drain) - sizeof(long), MSG_BUY_TICKET, IPC_NOWAIT) >= 0)
                {
                    rej.mtype = MSG_TICKET_OK + drain.pid;
                    rej.pid = drain.pid;
                    rej.tickets = 0;
                    msgsnd(g_msqid, &rej, sizeof(rej) - sizeof(long), IPC_NOWAIT);
                }
                while (msgrcv(g_msqid, &drain, sizeof(drain) - sizeof(long), MSG_BUY_TICKET_VIP, IPC_NOWAIT) >= 0)
                {
                    rej.mtype = MSG_TICKET_OK + drain.pid;
                    rej.pid = drain.pid;
                    rej.tickets = 0;
                    msgsnd(g_msqid, &rej, sizeof(rej) - sizeof(long), IPC_NOWAIT);
                }
                drained = 1;
            }
    
            sched_yield();
            continue;
        }
        
        int threshold = K / 10;
        int queue = d->ticket_queue;
        int N = d->active_cashiers;
        
        if (!all_sold)
        {
            // ZAWSZE min. 2 kasy gdy są bilety do sprzedania
            if (N < 2)
            {
                // Awaryjne otwarcie kas do minimum 2
                while (d->active_cashiers < 2)
                {
                    pid_t p = fork();
                    if (p == 0)
                    {
                        execl("./cashier", "cashier", NULL);
                        fatal_error("execl cashier");
                    }
                    cashier_pids[d->active_cashiers] = p;
                    d->active_cashiers++;
                }
                logp("[MANAGER] Otwarto kasy do minimum 2 (było: %d)\n", N);
                N = d->active_cashiers;  // Aktualizuj N
            }

            // Zamykanie kas
            if (N > 2 && queue < threshold * (N - 1))
            {
                pid_t p = cashier_pids[N - 1];
    
                kill(p, SIGTERM);
    
                d->active_cashiers--;
    
                logp("[MANAGER] Zamykam kase (kolejka=%d, kasy=%d->%d)\n", queue, N, d->active_cashiers);
            }

            // Otwieranie kas
            if (N < MAX_CASHIERS && queue > threshold * N)
            {
                pid_t p = fork();
    
                if (p == 0)
                {
                    execl("./cashier", "cashier", NULL);
                    fatal_error("execl cashier");
                }
    
                cashier_pids[N] = p;
                d->active_cashiers++;
    
                logp("[MANAGER] Otwieram kase (kolejka=%d, kasy=%d->%d)\n", queue, N-1, d->active_cashiers);
            }
        }

        sem_unlock(g_semid, 3);

        if (evac_flag) break;

        sched_yield();
    }

    if (soft_evac)
    {
        if (sem_lock(g_semid, 2) == 0)
        {
            d->evacuation = 1;
            sem_unlock(g_semid, 2);
        }

        logp("[MANAGER] Koniec meczu - wysylam sygnal do grupy procesow\n");

        // WYCZYSC KOLEJKE BILETOW
        msg_t dump;

        while (msgrcv(g_msqid, &dump, sizeof(dump)-sizeof(long), MSG_BUY_TICKET, IPC_NOWAIT) >= 0);

        while (msgrcv(g_msqid, &dump, sizeof(dump)-sizeof(long), MSG_BUY_TICKET_VIP, IPC_NOWAIT) >= 0);

        // Wyślij ewakuację do techów
        logp("[MANAGER] Wysylam SIG_EVACUATE do techow...\n");
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

        //Wyślij ewakuację do fanów
        logp("[MANAGER] Wysylam SIG_EVACUATE do fanow...\n");
        for (int i = 0; i < MAX_FANS; i++)
        {
            if (fan_pids[i] > 0)
            {
                kill(fan_pids[i], SIG_EVACUATE);
            }
        }


        //Wyślij ewakuację do kasjerów
        logp("[MANAGER] Wysylam SIG_EVACUATE do kasjerow...\n");
        for (int i = 0; i < MAX_CASHIERS; i++)
        {
            if (cashier_pids[i] > 0)
            {
                kill(cashier_pids[i], SIG_EVACUATE);
                cashier_pids[i] = 0;
            }
        }
        d->active_cashiers = 0;

        logp("[MANAGER] Czekam na potwierdzenia od %d sektorow...\n", MAX_SECTORS);

        int empty = 0;
        int iterations = 0;
        int max_iterations = 10000000;

        while (empty < MAX_SECTORS && iterations < max_iterations)
        {
            ssize_t r = msgrcv(g_msqid, &msg, sizeof(msg) - sizeof(long),  MSG_SECTOR_EMPTY, IPC_NOWAIT);
        
            if (r >= 0 && msg.mtype == MSG_SECTOR_EMPTY)
            {
                iterations = 0;   // RESET ZAWSZE

                if (!d->sector_reported[msg.sector])
                {
                    d->sector_reported[msg.sector] = 1;
                    empty++;

                    logp("[MANAGER] Sektor %d oprozniony (%d/%d)\n", msg.sector, empty, MAX_SECTORS);
                }
            }

            else if (r == -1)
            {
                if (errno == ENOMSG)
                {
                    sched_yield();
                    iterations++;

                    if (iterations % 1000 == 0) sched_yield();

                    continue;
                }

                if (errno == EINTR)
                {
                    continue;
                }
                if (errno == EIDRM)
                {
                    logp("[MANAGER] Kolejka wiadomosci usunieta\n");
                    break;
                }
            }
        }

        if (empty == MAX_SECTORS) logp("[MANAGER] Otrzymano wszystkie potwierdzenia\n");

        //Wyślij SIGTERM do wszystkich techow
        for (int s = 0; s < MAX_SECTORS; s++)
        {
            for (int g = 0; g < GATES_PER_SECTOR; g++)
            {
                if (tech_pids[s][g] > 0) kill(tech_pids[s][g], SIGTERM);
            }
        }

        //Wyślij SIGTERM do wszystkich kasjerow
        logp("[MANAGER] Wysylam SIGTERM do kasjerow...\n");
        for (int i = 0; i < MAX_CASHIERS; i++)
        {
            if (cashier_pids[i] > 0)
            {
                kill(cashier_pids[i], SIGTERM);
            }
        }

        signal(SIGTERM, SIG_IGN);
        signal(SIGCHLD, SIG_DFL);
        kill(-getpgrp(), SIGTERM);
        while (wait(NULL) > 0);
    }
    else
    {
        d->evacuation = 1;
        signal(SIGTERM, SIG_IGN);
        signal(SIGCHLD, SIG_DFL);
        kill(-getpgrp(), SIGTERM);
        while (wait(NULL) > 0);
    }
    cleanup();
    return 0;
}