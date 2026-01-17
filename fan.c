#include "common.h"

int main(void)
{
    int msqid = msgget(ftok(".", IPC_KEY + 1), 0);
    if (msqid < 0) fatal_error("fan msgget");

    msg_t msg;
    msg.mtype = 1;
    msg.pid = getpid();

    if (msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) fatal_error("fan msgsnd");

    return 0;
}
