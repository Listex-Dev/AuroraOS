#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-m MODE] NAME TYPE [MAJOR MINOR]\n", prog);
    fprintf(stderr, "Create the special file NAME of the given TYPE.\n");
    fprintf(stderr, "  TYPE:\n");
    fprintf(stderr, "    b      create a block (buffered) special file\n");
    fprintf(stderr, "    c, u   create a character (unbuffered) special file\n");
    fprintf(stderr, "    p      create a FIFO\n");
}

int main(int argc, char **argv)
{
    mode_t perm = 0660;
    int arg_idx = 1;

    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[arg_idx], "-m") == 0) {
        if (arg_idx + 1 >= argc) {
            print_usage(argv[0]);
            return 1;
        }
        perm = (mode_t)strtoul(argv[arg_idx + 1], NULL, 8);
        arg_idx += 2;
    }

    if (arg_idx >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    const char *name = argv[arg_idx++];
    if (arg_idx >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    char type_char = argv[arg_idx++][0];
    mode_t mode = 0;
    dev_t dev = 0;

    if (type_char == 'b') {
        mode = S_IFBLK | (perm & 07777);
        if (arg_idx + 1 >= argc) {
            fprintf(stderr, "%s: special files require major and minor device numbers\n", argv[0]);
            return 1;
        }
        unsigned int ma = (unsigned int)atoi(argv[arg_idx++]);
        unsigned int mi = (unsigned int)atoi(argv[arg_idx++]);
        dev = makedev(ma, mi);
    } else if (type_char == 'c' || type_char == 'u') {
        mode = S_IFCHR | (perm & 07777);
        if (arg_idx + 1 >= argc) {
            fprintf(stderr, "%s: special files require major and minor device numbers\n", argv[0]);
            return 1;
        }
        unsigned int ma = (unsigned int)atoi(argv[arg_idx++]);
        unsigned int mi = (unsigned int)atoi(argv[arg_idx++]);
        dev = makedev(ma, mi);
    } else if (type_char == 'p') {
        mode = S_IFIFO | (perm & 07777);
    } else {
        fprintf(stderr, "%s: invalid device type '%c'\n", argv[0], type_char);
        return 1;
    }

    if (mknod(name, mode, dev) < 0) {
        perror("mknod");
        return 1;
    }

    return 0;
}
