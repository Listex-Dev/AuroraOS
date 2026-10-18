/*
 * AuroraOS /bin/sleep - Delay for a specified amount of time
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sleep seconds\n");
        return 1;
    }
    int secs = atoi(argv[1]);
    if (secs > 0) {
        sleep(secs);
    }
    return 0;
}
