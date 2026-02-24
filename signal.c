#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Użycie: ./signal <sygnał> [sektor]\n");
        printf("  ./signal 10 x  -> zablokuj sektor x\n");
        printf("  ./signal 12 x  -> odblokuj sektor x\n");
        printf("  ./signal 3     -> koniec meczu\n");
        return 1;
    }

    int sig = atoi(argv[1]);

    pid_t pid = 0;
    FILE *f = popen("pgrep -x manager", "r");
    if (f) { fscanf(f, "%d", &pid); pclose(f); }

    if (pid <= 0) { printf("Manager nie działa!\n"); return 1; }

    if (sig == 3)
    {
        kill(pid, SIGTERM);
        printf("Wysłano koniec meczu do managera (pid=%d)\n", pid);
        return 0;
    }

    if (argc < 3)
    {
        printf("Podaj numer sektora!\n");
        return 1;
    }

    int val = atoi(argv[2]);
    union sigval sv;
    sv.sival_int = val;

    sigqueue(pid, sig, sv);
    printf("Wysłano sygnał %d z wartością %d do managera (pid=%d)\n", sig, val, pid);
    return 0;
}