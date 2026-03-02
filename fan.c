#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include <signal.h>
#include <sched.h>
#include <limits.h>
#include <pthread.h>

volatile sig_atomic_t evac_flag = 0;
volatile sig_atomic_t leave_flag = 0;

void evacuate(int sig)
{
    (void)sig;
    evac_flag = 1;
    leave_flag = 1;
}

void leave(int sig)
{
    (void)sig;
    leave_flag = 1;
}

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int sector;
    int entered;
    int should_leave;
    int parent_id;
    int child_id;
} family_sync_t;

typedef struct {
    family_sync_t *sync;
    volatile sig_atomic_t *p_evac;
    volatile sig_atomic_t *p_leave;
} child_args_t;

static void *child_thread_func(void *arg)
{
    child_args_t  *a    = (child_args_t *)arg;
    family_sync_t *sync = a->sync;

    logp("[CHILD %d] Czeka przy kasie razem z opiekunem %d\n", sync->child_id, sync->parent_id);

    pthread_mutex_lock(&sync->mutex);
    while (!sync->entered && !sync->should_leave && !*a->p_evac)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000000LL;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&sync->cond, &sync->mutex, &ts);
    }
    int my_sector = sync->sector;
    int do_leave  = sync->should_leave || *a->p_evac;
    pthread_mutex_unlock(&sync->mutex);

    if (do_leave)
    {
        logp("[CHILD %d] Opuszcza stadion razem z opiekunem %d (bez wejscia)\n", sync->child_id, sync->parent_id);
        return NULL;
    }

    logp("[CHILD %d] Wchodzi na sektor %d razem z opiekunem %d\n", sync->child_id, my_sector, sync->parent_id);
    logp("[CHILD %d] Ogląda mecz na sektorze %d\n", sync->child_id, my_sector);

    pthread_mutex_lock(&sync->mutex);
    while (!sync->should_leave && !*a->p_evac)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000000LL;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&sync->cond, &sync->mutex, &ts);
    }
    pthread_mutex_unlock(&sync->mutex);

    logp("[CHILD %d] Opuszcza stadion razem z opiekunem %d\n", sync->child_id, sync->parent_id);
    return NULL;
}

#define SIGNAL_CHILD(sync_ptr, field, val)          \
    do {                                            \
        pthread_mutex_lock(&(sync_ptr)->mutex);     \
        (sync_ptr)->field = (val);                  \
        pthread_cond_broadcast(&(sync_ptr)->cond);  \
        pthread_mutex_unlock(&(sync_ptr)->mutex);   \
    } while (0)


static shared_data_t *d = NULL;
static int g_semid = -1;

void aggression(int sig)
{
    (void)sig;
}

void leave_sector(int my_sector , int my_id, int count)
{
    (void)my_id;

    if (my_sector != -1)
    {
        if (sem_lock(g_semid, 4 + my_sector) == 0)
        {
            int dec = (d->sector_taken[my_sector] >= count) ? count : d->sector_taken[my_sector];
            d->sector_taken[my_sector] -= dec;

            if (sem_lock(g_semid, 3) == 0)
            {
                if (d->total_taken >= dec) d->total_taken -= dec;
                sem_unlock(g_semid, 3);
            }
            sem_unlock(g_semid, 4 + my_sector);
        }
    }
}


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGTERM, leave);

    srand(getpid());

    signal(SIGUSR1, aggression);
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = evacuate;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIG_EVACUATE, &sa, NULL);

    int has_flare = (rand() % 1000) < FLARE_PROB;
    int team = rand() % 2;
    int patience = 5;
    int age = 18 + rand() % 40;
    int has_child = (rand() % 100) < 10;
    int first_time = 1;
    int queued = 0;
    int in_gate_queue = 0;
    int my_sector = -1;
    int already_left = 0;

    pthread_t child_tid    = 0;
    family_sync_t sync;
    child_args_t cargs;
    int child_started = 0;

    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("fan shmget");

    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("fan msgget");

    int semid = semget(ftok(".", IPC_KEY + 2), 4 + MAX_SECTORS, 0);
    if (semid < 0) fatal_error("fan semget");

    g_semid = semid;

    d = shmat(shmid, NULL, 0);
    if (d == (void *)-1) fatal_error("fan shmat");
    
    int my_id;

    if (sem_lock(semid, 3) != 0)
    {
        shmdt(d);
        return 0;
    }

    my_id = ++d->fan_counter;
    d->gate_wait[my_id] = 0;

    int child_id = 0;
    if (has_child)
    {
        child_id = ++d->fan_counter;
        logp("[FAN %d] Przychodzi z dzieckiem (id=%d)\n", my_id, child_id);
    }

    if (sem_unlock(semid, 3) != 0)
    {
        shmdt(d);
        return 0;
    }
    
    int vip = 0;

    if (sem_lock(semid, 3) == 0)
    {
        double limit = 0.003 * d->fan_counter;
        if (d->vip_count < limit && (rand() % 1000) < 3)
        {
            vip = 1;
            d->vip_count++;
        }
        sem_unlock(semid, 3);
    }

    if (vip)
    {
        logp("[FAN %d] VIP (vip=%d / all=%d)\n", my_id, d->vip_count, d->fan_counter);
    }

    if (has_child && !vip)
    {
        pthread_mutex_init(&sync.mutex, NULL);
        pthread_cond_init(&sync.cond, NULL);
        sync.sector       = -1;
        sync.entered      = 0;
        sync.should_leave = 0;
        sync.parent_id    = my_id;
        sync.child_id     = child_id;

        cargs.sync    = &sync;
        cargs.p_evac  = &evac_flag;
        cargs.p_leave = &leave_flag;

        pthread_create(&child_tid, NULL, child_thread_func, &cargs);
        child_started = 1;
    }

    int want_tickets = (has_child && !vip) ? 2 : 1;
    msg_t req, res;

    while (!evac_flag && !leave_flag)
    {
        if (leave_flag)
        {
            logp("[FAN %d] Opuszczam stadion\n", my_id);

            break;
        }

        if (first_time && !evac_flag)
        {
            if (vip)
            {
                logp("[VIP %d] Podchodzi do kasy bez kolejki\n", my_id);
            }
            else
            {
                logp("[FAN %d] Kibic druzyny %c podchodzi do kasy\n", my_id, team == 0 ? 'A' : 'B');
            }
            first_time = 0;
        }
        
        if (!queued)
        {
            if (sem_lock(semid, 3) == 0)
            {
                if (vip) d->vip_queue++;
                else d->ticket_queue++;
                sem_unlock(semid, 3);
            }
            queued = 1;
        }

        if (!vip && patience-- <= 0)
        {
            raise(SIGUSR1);
            break;
        }

        if (vip)
        {
            req.mtype = MSG_BUY_TICKET_VIP;
        }
        else
        {
            req.mtype = MSG_BUY_TICKET;
        }

        req.pid = my_id;
        req.vip = vip;

        req.want_tickets = want_tickets;

        req.has_flare = has_flare;
        req.team = team;
        req.age = age;
        req.guardian = 0;

        if (vip)
        {
            req.sector = VIP_SECTOR;
        }
        else
        {
            req.sector = rand() % (MAX_SECTORS - 1);
        }

        if (sem_lock(semid, 2) == 0)
        {
            if (d->total_tickets_sold >= d->total_capacity)
            {
                sem_unlock(semid, 2);
                logp("[FAN %d] Ide do domu (brak biletow)\n", my_id);
                break;
            }
            sem_unlock(semid, 2);
        }

        logp("[FAN %d] Chce kupic bilet na sektor %d\n", my_id, req.sector);

        int send_retry = 0;
        int max_send_retry = 100000;
        
        while (send_retry < max_send_retry)
        {
            if (evac_flag || d->evacuation)
            {
                leave_sector(my_sector, my_id, want_tickets);
                goto exit_loop;
            }

            if (msgsnd(msqid, &req, sizeof(req) - sizeof(long), IPC_NOWAIT) == 0)
            {
                break;
            }
            
            if (errno == EINTR || errno == EIDRM || evac_flag || d->evacuation) 
            {
                goto exit_loop;
            }
            
            if (errno == EAGAIN)
            {
                sched_yield();
                send_retry++;
                continue;
            }
            
            break;
        }
        
        if (send_retry >= max_send_retry)
        {
            logp("[FAN %d] Nie udalo sie wyslac requestu\n", my_id);
            continue;
        }

        ssize_t r;
        int recv_retry = 0;
        int max_recv_retry = INT_MAX;
        
        while (recv_retry < max_recv_retry)
        {
            if (evac_flag || d->evacuation)
            {
                leave_sector(my_sector, my_id, want_tickets);
                goto exit_loop;
            }
            
            r = msgrcv(msqid, &res, sizeof(res) - sizeof(long), MSG_TICKET_OK + my_id, IPC_NOWAIT);
            
            if (r >= 0)
            {
                break;
            }
            
            if (errno == ENOMSG)
            {
                sched_yield();
                recv_retry++;
                continue;
            }
            
            if (errno == EINTR || errno == EIDRM)
            {
                goto exit_loop;
            }
            
            fatal_error("fan msgrcv ticket");
        }

        if (queued)
        {
            if (sem_lock(semid, 3) == 0)
            {
                if (vip && d->vip_queue > 0) d->vip_queue--;
                else if (!vip && d->ticket_queue > 0) d->ticket_queue--;
                sem_unlock(semid, 3);
            }
            queued = 0;
        }

        my_sector = res.sector;

        if (res.tickets > 0 && res.tickets < want_tickets && child_started)
        {
            logp("[FAN %d] Dostal %d zamiast %d biletow - dziecko wraca do domu\n", my_id, res.tickets, want_tickets);
            want_tickets = res.tickets;
            SIGNAL_CHILD(&sync, should_leave, 1);
            pthread_join(child_tid, NULL);
            pthread_mutex_destroy(&sync.mutex);
            pthread_cond_destroy(&sync.cond);
            child_started = 0;
        }

        if (res.tickets && vip)
        {
            logp("[VIP %d] Kupil bilet na sektor VIP\n", my_id);
            
            logp("[VIP %d] Wchodzi bez kontroli do sektora VIP\n", my_id);

            if (sem_lock(semid, 4 + my_sector) == 0)
            {
                d->sector_taken[my_sector]++;
                if (sem_lock(semid, 3) == 0)
                {
                    d->total_taken++;
                    sem_unlock(semid, 3);
                }
                sem_unlock(semid, 4 + my_sector);
            }

            logp("[VIP %d] Ogląda mecz na sektorze VIP\n", my_id);

            int check_evac = 0;
            while (!evac_flag && !check_evac && !leave_flag)
            {
                if (sem_lock(semid, 2) == 0)
                {
                    check_evac = d->evacuation;
                    sem_unlock(semid, 2);
                }
                sched_yield();
            }
            break;
        }

        logp("[FAN %d] Kupil %d bilet(y)\n", my_id, res.tickets);

        if (res.tickets == 0)
        {
            if (want_tickets == 2)
            {
                logp("[FAN %d] Brak miejsca dla 2 osob, probuje kupic 1 bilet (dziecko zostaje)\n", my_id);
                want_tickets = 1;
                patience = 5;
                if (child_started)
                {
                    SIGNAL_CHILD(&sync, should_leave, 1);
                    pthread_join(child_tid, NULL);
                    pthread_mutex_destroy(&sync.mutex);
                    pthread_cond_destroy(&sync.cond);
                    child_started = 0;
                }
                continue;
            }

            if (!evac_flag && !d->evacuation)
            {
                logp("[FAN %d] Ide do domu (brak biletow)\n", my_id);
            }
            break;
        }

        if (res.tickets && !vip)
        {
            if (has_child && res.tickets == 2)
            {
                logp("[FAN %d] Wraz z dzieckiem %d ma bilety na sektor %d, szuka bramki...\n", my_id, child_id, res.sector);
            }
            else if (res.tickets == 2)
            {
                logp("[FAN %d] Wraz z osoba towarzyszaca ma bilety na sektor %d, szuka bramki...\n", my_id, res.sector);
            }
            else
            {
                logp("[FAN %d] Ma bilet na sektor %d, szuka bramki...\n", my_id, res.sector);
            }

            msg_t gate_req;
            int gate = rand() % GATES_PER_SECTOR;

            gate_req.mtype = MSG_GATE_BASE + res.sector * GATES_PER_SECTOR + gate;
            gate_req.pid = my_id;
            gate_req.team = team;
            gate_req.sector = res.sector;
            gate_req.has_flare = has_flare;
            gate_req.tickets = res.tickets;

            if (msgsnd(msqid, &gate_req, sizeof(gate_req) - sizeof(long), IPC_NOWAIT) == -1)
            {
                if (errno != EINTR && errno != EIDRM && errno != EAGAIN) perror("fan gate msgsnd");
            }

            if (!in_gate_queue)
            {
                if (sem_lock(semid, 1) == 0)
                {
                    queue_push(&d->gate_queue[res.sector], my_id);
                    sem_unlock(semid, 1);
                }
                in_gate_queue = 1;
            }

            msg_t gate_res;
            long my_type = MSG_GATE_RESPONSE + my_id;

            ssize_t gr;
            int gate_retry = 0;
            int max_gate_retry = INT_MAX;
            
            while (gate_retry < max_gate_retry)
            {
                if (evac_flag || d->evacuation)
                {
                    leave_sector(my_sector, my_id, want_tickets);
                    goto exit_loop;
                }

                if (gate_retry % 1000000 == 0 && gate_retry > 0)
                {
                    logp("[FAN %d] UWAGA: Czekam juz %d iteracji na bramke %d/%d\n", my_id, gate_retry, res.sector, gate);
                }
                
                gr = msgrcv(msqid, &gate_res, sizeof(gate_res) - sizeof(long), my_type, IPC_NOWAIT);
                
                if (gr >= 0)
                {
                    break;
                }
                
                if (errno == ENOMSG)
                {
                    sched_yield();
                    gate_retry++;
                    continue;
                }
                
                if (errno == EINTR || errno == EIDRM)
                {
                    goto exit_loop;
                }
                
                fatal_error("fan msgrcv gate");
            }

            if (gate_res.tickets < 0)
            {
                logp("[FAN %d] Ide do domu (wyprowadzony - race)\n", my_id);
                break;
            }

            if (d->evacuation)
            {
                leave_sector(my_sector, my_id, want_tickets);
                break;
            }

            logp("[FAN %d] Wchodzi na hale\n", my_id);

            in_gate_queue = 0;

            if (child_started)
            {
                pthread_mutex_lock(&sync.mutex);
                sync.sector  = my_sector;
                sync.entered = 1;
                pthread_cond_broadcast(&sync.cond);
                pthread_mutex_unlock(&sync.mutex);
                logp("[FAN %d] Sygnalizuje dziecku %d wejscie na sektor %d\n", my_id, child_id, my_sector);
            }

            logp("[FAN %d] Ogląda mecz na sektorze %d\n", my_id, my_sector);

            int check_evac = 0;
            while (!evac_flag && !check_evac && !leave_flag)
            {
                if (sem_lock(semid, 2) == 0)
                {
                    check_evac = d->evacuation;
                    sem_unlock(semid, 2);
                }
                sched_yield();
            }

            if (!already_left) 
            {
                leave_sector(my_sector, my_id, want_tickets);
                already_left = 1;
            }

            if (child_started) SIGNAL_CHILD(&sync, should_leave, 1);
            
            break;
        }
    }
    
    exit_loop:
        if ((evac_flag || d->evacuation) && !already_left)
        {
            leave_sector(my_sector, my_id, want_tickets);
            already_left = 1;
        }
    
    if (my_sector != -1 && !already_left) 
    {
        leave_sector(my_sector, my_id, want_tickets);
        already_left = 1;
    }

    if (child_started)
    {
        SIGNAL_CHILD(&sync, should_leave, 1);
        pthread_join(child_tid, NULL);
        pthread_mutex_destroy(&sync.mutex);
        pthread_cond_destroy(&sync.cond);
    }
    shmdt(d);
    return 0;
}