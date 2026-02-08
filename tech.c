#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <signal.h>
#include <sys/types.h>
#include <sched.h>

volatile sig_atomic_t evac_flag = 0;

static shared_data_t *d = NULL;
int last_block_state = 0;

void evacuate(int sig)
{
    (void)sig;
    evac_flag = 1;
}



int main(int argc, char **argv) 
{
    if (argc < 3)
    {
        fprintf(stderr, "Tech: brak numeru sektora\n");
        exit(1);
    }

    int my_sector = atoi(argv[1]);
    int my_gate   = atoi(argv[2]);
    
    setvbuf(stdout, NULL, _IONBF, 0);

    srand(getpid());
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = evacuate;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_EVACUATE, &sa, NULL);

    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("tech shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("tech msgget");

    int semid = semget(ftok(".", IPC_KEY + 2), 3 + MAX_SECTORS, 0);
    if (semid < 0) fatal_error("tech semget");

    d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("tech shmat");
    
    msg_t req, res;
    int sent_confirmation = 0;
    int semr;

    while (!sent_confirmation)
    {

        int check_evac = 0;

        if (sem_lock(semid, 2) == 0)
        {
            check_evac = d->evacuation;
            sem_unlock(semid, 2);
        }
        

        if (evac_flag || check_evac)
        {
            logp("[TECH %d/%d] TRYB EWAKUACJI- czekam na oproznienie sektora\n",  my_sector, my_gate);
            fflush(stdout);

            int left;
            int wait_iterations = 0;
            int max_wait = 100000;
            do
            {
                semr = sem_lock(semid, 3 + my_sector);

                if (semr == -2) continue;

                if (semr == -1) goto cleanup;

                left = d->sector_taken[my_sector];
                sem_unlock(semid, 3 + my_sector);

                if (left > 0)
                {
                    sched_yield();
                    wait_iterations++;

                    if (wait_iterations > max_wait)
                    {
                        logp("[TECH %d/%d] TIMEOUT - forsowne oproznienie\n", my_sector, my_gate);
                        fflush(stdout);
                        
                        if (sem_lock(semid, 3 + my_sector) == 0) 
                        {
                            d->sector_taken[my_sector] = 0;
                            sem_unlock(semid, 3 + my_sector);
                        }
                        break;
                    }
                }

            } while (left > 0); 

            logp("[TECH %d/%d] Sektor oprozniony\n", my_sector, my_gate);
            fflush(stdout);

            if (my_gate == 0)
            {
                msg_t info;
                info.mtype = MSG_SECTOR_EMPTY;
                info.sector = my_sector;
                info.pid = getpid();

                logp("[TECH %d/%d] Wysylam potwierdzenie do managera...\n", my_sector, my_gate);
                fflush(stdout);

                int retry = 0;
                int max_retry = 100000;
                int sent = 0;

                while (retry < max_retry)
                {
                    if (msgsnd(msqid, &info, sizeof(info) - sizeof(long), IPC_NOWAIT) == 0)
                    {
                        logp("[TECH %d/%d] Potwierdzenie wyslane!\n", my_sector, my_gate);
                        fflush(stdout);
                        sent = 1;
                        break;
                    }
                    
                    if (errno == EIDRM)
                    {
                        logp("[TECH %d/%d] Kolejka usunieta\n", my_sector, my_gate);
                        fflush(stdout);
                        break;
                    }
                    
                    if (errno == EAGAIN)
                    {
                        sched_yield();
                        retry++;
                        continue;
                    }
                    
                    perror("tech msgsnd empty");
                    break;
                }
                
                if (!sent && errno != EIDRM)
                {
                    logp("[TECH %d/%d] UWAGA: Nie udalo sie wyslac potwierdzenia\n", my_sector, my_gate);
                    fflush(stdout);
                }
                }
                else
                {
                    logp("[TECH %d/%d] Sektor oprozniony (bramka pomocnicza)\n", my_sector, my_gate);
                    fflush(stdout);
                }

                sent_confirmation = 1;
                break;
            }

        long my_type = MSG_GATE_BASE + my_sector * GATES_PER_SECTOR + my_gate;

        ssize_t r = msgrcv(msqid, &req, sizeof(req) - sizeof(long), my_type, IPC_NOWAIT);

        if (r <= 0)
        {
            if (errno == ENOMSG)
            {
                sched_yield();
                continue;
            }
            if (errno == EIDRM) 
            {
                continue;
            }
            continue;
        }

        if (check_evac || evac_flag)
        {
            continue;
        }

        int gate = -1;
        int need = (req.tickets == 2) ? 2 : 1;

        while (!check_evac && !evac_flag)
        {
            if (sem_lock(semid, 2) == 0)
            {
                check_evac = d->evacuation;
                sem_unlock(semid, 2);
            }
    
            if (check_evac || evac_flag)
            {
                break;
            }

            int blocked;

            semr = sem_lock(semid, 3 + req.sector);

            if (semr == -2) continue;

            if (semr == -1) goto cleanup;

            blocked = d->sector_blocked[req.sector];
            sem_unlock(semid, 3 + req.sector);

            if (blocked && last_block_state == 0)
            {
                logp("[TECH %d/%d] Sektor ZABLOKOWANY\n", my_sector, my_gate);
                fflush(stdout);
            }

            if (!blocked && last_block_state == 1)
            {
                logp("[TECH %d/%d] Sektor ODBLOKOWANY\n", my_sector, my_gate);
                fflush(stdout);
            }

            last_block_state = blocked;

            if (blocked)
            {
                sched_yield();
                continue;
            }

            semr = sem_lock(semid, 3 + req.sector);

            if (semr == -2) continue;

            if (semr == -1) goto cleanup;

            if (check_evac || evac_flag)
            {
                sem_unlock(semid, 3 + req.sector);
                break;
            }

            int i = my_gate;
            int count = d->gate_count[req.sector][i];
            int gteam = d->gate_team[req.sector][i];

            // Priorytet
            if (count > 0 && gteam != req.team && count < MAX_GATE_CAPACITY && !d->priority[req.pid])
            {
                sem_lock(semid, 2);

                d->gate_wait[req.pid]++;

                sem_unlock(semid, 2);

                if (d->gate_wait[req.pid] >= 5)
                {
                    d->priority[req.pid] = 1;
                    logp("[TECH %d/%d] Fan %d dostaje priorytet\n", my_sector, my_gate, req.pid);
                    fflush(stdout);
                }
            }

            if (d->priority[req.pid] && count + need <= MAX_GATE_CAPACITY)
            {
                d->gate_count[req.sector][i] += need;
                d->gate_team[req.sector][i] = req.team;
                gate = i;
                sem_unlock(semid, 3 + req.sector);
                break;
            }

            if (count + need <= MAX_GATE_CAPACITY)
            {
                if (count == 0 || gteam == req.team)
                {
                    d->gate_team[req.sector][i] = req.team;
                    d->gate_count[req.sector][i] += need;
                    gate = i;
                    sem_unlock(semid, 3 + req.sector);
                    break;
                }
            }

            sem_unlock(semid, 3 + req.sector);

            if (gate != -1 || evac_flag || check_evac)
            {
                break;
            }

            sched_yield();
        }
        
        if (gate == -1) 
        {
            continue;
        }

        if (check_evac || evac_flag)
        {
            continue;
        }

        logp("[TECH %d/%d] Obsluguje fana %d\n", my_sector, my_gate, req.pid);
        fflush(stdout);

        semr = sem_lock(semid, 3 + req.sector);

        if (semr == -2) continue;

        if (semr == -1) goto cleanup;


        int f = queue_front(&d->gate_queue[req.sector]);
        if (f == req.pid)
        {
            queue_pop(&d->gate_queue[req.sector]);
        }

        d->gate_wait[req.pid] = 0;
        d->priority[req.pid] = 0;

        sem_unlock(semid, 3 + req.sector);

        if (req.tickets == 2)
        {
            logp("[Bramka %d/%d] Sprawdzam fana %d + towarzysz (sektor %d, team %c)\n",  my_sector, gate, req.pid, req.sector, req.team == 0 ? 'A' : 'B');
        }
        else
        {
            logp("[Bramka %d/%d] Sprawdzam fana %d (sektor %d, team %c)\n",  my_sector, gate, req.pid, req.sector, req.team == 0 ? 'A' : 'B');
        }
        fflush(stdout);

        // Sprawdzenie rac
        if (req.has_flare)
        {
            logp("[Bramka %d/%d] ALARM! Fan %d ma race - WYPROWADZENIE\n", my_sector, gate, req.pid);
            fflush(stdout);

            kill(req.pid, SIGKILL);

            if (sem_lock(semid, 3 + req.sector) != -1)
            {
                if (d->gate_count[req.sector][gate] >= need)
                {
                    d->gate_count[req.sector][gate] -= need;
                    if (d->gate_count[req.sector][gate] == 0) 
                        d->gate_team[req.sector][gate] = -1;
                }

                if (d->sector_taken[req.sector] >= need)
                {
                    d->sector_taken[req.sector] -= need;
                }

                sem_unlock(semid, 3 + req.sector);
            }
            continue;
        }

        if (req.tickets == 2)
        {
            logp("[Bramka %d/%d] Fan %d + osoba towarzyszaca bezpieczni\n", my_sector, gate, req.pid);
        }
        else
        {
            logp("[Bramka %d/%d] Fan %d bezpieczny\n", my_sector, gate, req.pid);
        }
        fflush(stdout);

        if (sem_lock(semid, 3 + req.sector) != -1)
        {
            if (d->gate_count[req.sector][gate] >= need)
            {
                d->gate_count[req.sector][gate] -= need;
                if (d->gate_count[req.sector][gate] == 0) 
                    d->gate_team[req.sector][gate] = -1;
            }

            sem_unlock(semid, 3 + req.sector);
        }

        res.mtype  = MSG_GATE_RESPONSE + req.pid;
        res.pid    = req.pid;
        res.tickets = gate;

        while (msgsnd(msqid, &res, sizeof(res) - sizeof(long), 0) == -1)
        {
            if (errno == EINTR) continue;

            if (errno == EIDRM) break;

            perror("tech send response");
            break;
        }

    }

    cleanup:
    shmdt(d);
    return 0;
}