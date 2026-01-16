#include "common.h"
#include <pthread.h>

static pthread_mutex_t control_mutex = PTHREAD_MUTEX_INITIALIZER;

static void stop_handler(int sig)
{
    (void)sig;
    _exit(0);
}

static void* control(void *arg)
{
    (void)arg;

    pthread_mutex_lock(&control_mutex);
    sleep(1);
    pthread_mutex_unlock(&control_mutex);

    pthread_exit(NULL);
}

int main(void) 
{
    signal(SIG_STOP_WORK, stop_handler);
    
    int shmid = shmget(ftok(".", IPC_KEY), sizeof(shared_data_t), 0);
    if (shmid < 0) fatal_error("tech shmget");

    shared_data_t *d = shmat(shmid, NULL, 0);
    (void)d;

    pthread_t t1, t2;

    pthread_create(&t1, NULL, control, NULL);
    pthread_create(&t2, NULL, control, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    sleep(1);
    return 0;
}
