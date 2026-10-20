/*
 * rmmod - Remove a module from the AuroraOS kernel
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: rmmod <module_name>\n");
        return 1;
    }

    const char *name = argv[1];
    int ret = delete_module(name, 0);
    if (ret != 0) {
        perror("rmmod: delete_module");
        return 1;
    }

    return 0;
}
