/*
 * AuroraOS /bin/sync - Synchronize cached writes to persistent storage
 */

#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    sync();
    return 0;
}
