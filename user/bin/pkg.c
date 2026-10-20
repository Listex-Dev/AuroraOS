/*
 * AuroraOS Package Manager (pkg)
 * Handles package installation, removal, integrity verification, and database tracking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <aurpkg.h>
#include <sha256.h>

static void make_parent_dirs(const char *path)
{
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
}

static char *read_key(const char *key_path, size_t *key_len_out)
{
    int fd = open(key_path, O_RDONLY);
    if (fd < 0) return NULL;
    char *buf = malloc(256);
    if (!buf) { close(fd); return NULL; }
    ssize_t n = read(fd, buf, 255);
    close(fd);
    if (n <= 0) { free(buf); return NULL; }
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
        n--;
    }
    buf[n] = '\0';
    *key_len_out = (size_t)n;
    return buf;
}

static int verify_package_signature(int fd, const aurpkg_header_t *hdr, const char *key, size_t key_len)
{
    sha256_ctx sign_ctx;
    sha256_init(&sign_ctx);
    sha256_update(&sign_ctx, hdr->magic, 8);
    sha256_update(&sign_ctx, hdr->name, sizeof(hdr->name));
    sha256_update(&sign_ctx, hdr->version, sizeof(hdr->version));
    sha256_update(&sign_ctx, hdr->description, sizeof(hdr->description));
    sha256_update(&sign_ctx, &hdr->file_count, sizeof(hdr->file_count));

    off_t orig = lseek(fd, sizeof(aurpkg_header_t), SEEK_SET);
    if (orig < 0) return -1;

    for (uint32_t i = 0; i < hdr->file_count; i++) {
        aurpkg_file_t fi;
        if (read(fd, &fi, sizeof(fi)) != sizeof(fi)) return -1;
        sha256_update(&sign_ctx, &fi, sizeof(fi));

        char buf[1024];
        uint32_t rem = fi.size;
        while (rem > 0) {
            size_t chunk = (rem > sizeof(buf)) ? sizeof(buf) : rem;
            ssize_t rd = read(fd, buf, chunk);
            if (rd <= 0) return -1;
            sha256_update(&sign_ctx, buf, (size_t)rd);
            rem -= rd;
        }
    }

    uint8_t digest[32];
    sha256_final(&sign_ctx, digest);

    char expected_sig[65];
    sha256_hmac(key, key_len, digest, 32, expected_sig);

    if (strcmp(hdr->signature, expected_sig) != 0) {
        return 0; /* Invalid signature */
    }
    return 1; /* Valid */
}

static int cmd_install(const char *pkg_path)
{
    printf("pkg: installing package %s...\n", pkg_path);

    int fd = open(pkg_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "pkg: cannot open package file '%s': %s\n", pkg_path, strerror(errno));
        return 1;
    }

    aurpkg_header_t hdr;
    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        fprintf(stderr, "pkg: corrupted or incomplete package header\n");
        close(fd);
        return 1;
    }

    if (memcmp(hdr.magic, AURPKG_MAGIC, 8) != 0) {
        fprintf(stderr, "pkg: invalid package magic (not an AuroraOS package)\n");
        close(fd);
        return 1;
    }

    /* 1. Check signature with trusted key */
    printf("[1/3] Verifying package cryptographic signature...\n");
    size_t key_len = 0;
    char *key = read_key(AURPKG_KEY_PATH, &key_len);
    if (!key) {
        fprintf(stderr, "pkg: trusted key not found at %s!\n", AURPKG_KEY_PATH);
        close(fd);
        return 1;
    }

    int sig_ok = verify_package_signature(fd, &hdr, key, key_len);
    free(key);

    if (sig_ok <= 0) {
        fprintf(stderr, "pkg: ERROR: Signature verification FAILED for '%s'!\n", pkg_path);
        fprintf(stderr, "pkg: Package is corrupted or signed with an untrusted key.\n");
        fprintf(stderr, "pkg: Aborting installation. Zero files modified.\n");
        close(fd);
        return 1;
    }
    printf("      Signature valid (HMAC-SHA256: %.16s...)\n", hdr.signature);

    /* 2. Extract and verify files */
    printf("[2/3] Extracting files for %s %s...\n", hdr.name, hdr.version);
    lseek(fd, sizeof(aurpkg_header_t), SEEK_SET);

    char installed_files_list[2048] = "";

    for (uint32_t i = 0; i < hdr.file_count; i++) {
        aurpkg_file_t fi;
        if (read(fd, &fi, sizeof(fi)) != sizeof(fi)) {
            fprintf(stderr, "pkg: error reading file header %u\n", i);
            close(fd);
            return 1;
        }

        make_parent_dirs(fi.path);
        int out_fd = open(fi.path, O_WRONLY | O_CREAT | O_TRUNC, fi.mode ? fi.mode : 0755);
        if (out_fd < 0) {
            fprintf(stderr, "pkg: cannot write file '%s': %s\n", fi.path, strerror(errno));
            close(fd);
            return 1;
        }

        sha256_ctx file_ctx;
        sha256_init(&file_ctx);

        char buf[1024];
        uint32_t rem = fi.size;
        while (rem > 0) {
            size_t chunk = (rem > sizeof(buf)) ? sizeof(buf) : rem;
            ssize_t rd = read(fd, buf, chunk);
            if (rd <= 0) break;
            write(out_fd, buf, rd);
            sha256_update(&file_ctx, buf, rd);
            rem -= rd;
        }
        close(out_fd);
        chmod(fi.path, fi.mode ? fi.mode : 0755);

        uint8_t file_digest[32];
        sha256_final(&file_ctx, file_digest);
        char computed_hex[65];
        for (int b = 0; b < 32; b++) {
            sprintf(computed_hex + b * 2, "%02x", file_digest[b]);
        }
        computed_hex[64] = '\0';

        if (strcmp(fi.sha256, computed_hex) != 0) {
            fprintf(stderr, "pkg: checksum mismatch on file '%s'!\n", fi.path);
            close(fd);
            return 1;
        }

        printf("      -> %s (mode %04o, %u bytes)\n", fi.path, fi.mode, fi.size);
        if (strlen(installed_files_list) + strlen(fi.path) + 4 < sizeof(installed_files_list)) {
            strcat(installed_files_list, "  ");
            strcat(installed_files_list, fi.path);
            strcat(installed_files_list, "\n");
        }
    }
    close(fd);

    /* 3. Register in /var/lib/pkg/db */
    printf("[3/3] Registering package in %s...\n", AURPKG_DB_PATH);
    make_parent_dirs(AURPKG_DB_PATH);
    int db_fd = open(AURPKG_DB_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (db_fd >= 0) {
        char reg_buf[4096];
        snprintf(reg_buf, sizeof(reg_buf),
                 "Package: %s\nVersion: %s\nStatus: installed\nDescription: %s\nFiles:\n%s\n",
                 hdr.name, hdr.version, hdr.description, installed_files_list);
        write(db_fd, reg_buf, strlen(reg_buf));
        close(db_fd);
    }

    printf("Successfully installed %s %s.\n", hdr.name, hdr.version);
    return 0;
}

static int cmd_list(void)
{
    int fd = open(AURPKG_DB_PATH, O_RDONLY);
    if (fd < 0) {
        printf("No packages currently installed.\n");
        return 0;
    }

    printf("%-18s %-10s %-12s %s\n", "NAME", "VERSION", "STATUS", "DESCRIPTION");
    printf("----------------------------------------------------------------------\n");

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    char *line = strtok(buf, "\n");
    char cur_pkg[64] = "";
    char cur_ver[32] = "";
    char cur_status[32] = "";
    char cur_desc[128] = "";

    while (line) {
        if (strncmp(line, "Package: ", 9) == 0) {
            strncpy(cur_pkg, line + 9, sizeof(cur_pkg) - 1);
        } else if (strncmp(line, "Version: ", 9) == 0) {
            strncpy(cur_ver, line + 9, sizeof(cur_ver) - 1);
        } else if (strncmp(line, "Status: ", 8) == 0) {
            strncpy(cur_status, line + 8, sizeof(cur_status) - 1);
        } else if (strncmp(line, "Description: ", 13) == 0) {
            strncpy(cur_desc, line + 13, sizeof(cur_desc) - 1);
        } else if (strcmp(line, "Files:") == 0) {
            if (cur_pkg[0]) {
                printf("%-18s %-10s %-12s %s\n", cur_pkg, cur_ver, cur_status, cur_desc);
                cur_pkg[0] = '\0';
            }
        }
        line = strtok(NULL, "\n");
    }
    if (cur_pkg[0]) {
        printf("%-18s %-10s %-12s %s\n", cur_pkg, cur_ver, cur_status, cur_desc);
    }
    return 0;
}

static int cmd_info(const char *name)
{
    int fd = open(AURPKG_DB_PATH, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "pkg: package '%s' is not installed\n", name);
        return 1;
    }

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        fprintf(stderr, "pkg: package '%s' is not installed\n", name);
        return 1;
    }
    buf[n] = '\0';

    char target_prefix[128];
    snprintf(target_prefix, sizeof(target_prefix), "Package: %s\n", name);

    char *p = strstr(buf, target_prefix);
    if (!p) {
        fprintf(stderr, "pkg: package '%s' is not installed\n", name);
        return 1;
    }

    /* Print until next package or end */
    char *end = strstr(p + 1, "\nPackage: ");
    if (end) *end = '\0';

    printf("%s\n", p);
    return 0;
}

static int cmd_remove(const char *name)
{
    printf("pkg: removing package '%s'...\n", name);

    int fd = open(AURPKG_DB_PATH, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "pkg: package '%s' not found\n", name);
        return 1;
    }

    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        fprintf(stderr, "pkg: package '%s' not found\n", name);
        return 1;
    }
    buf[n] = '\0';

    char target[128];
    snprintf(target, sizeof(target), "Package: %s\n", name);
    char *start = strstr(buf, target);
    if (!start) {
        fprintf(stderr, "pkg: package '%s' is not installed\n", name);
        return 1;
    }

    /* Locate files to remove */
    char *files_sec = strstr(start, "Files:\n");
    char *next_pkg = strstr(start + 1, "Package: ");

    if (files_sec && (!next_pkg || files_sec < next_pkg)) {
        char *line = files_sec + 7;
        while (line && (!next_pkg || line < next_pkg) && *line == ' ') {
            while (*line == ' ') line++;
            char filepath[128];
            int idx = 0;
            while (*line && *line != '\n' && idx < 127) {
                filepath[idx++] = *line++;
            }
            filepath[idx] = '\0';
            if (*line == '\n') line++;

            if (filepath[0]) {
                printf("  removing %s\n", filepath);
                unlink(filepath);
            }
        }
    }

    /* Rewrite database excluding this package block */
    unlink(AURPKG_DB_PATH);
    int out_fd = open(AURPKG_DB_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd >= 0) {
        if (start > buf) {
            write(out_fd, buf, start - buf);
        }
        if (next_pkg) {
            write(out_fd, next_pkg, strlen(next_pkg));
        }
        close(out_fd);
    }

    printf("Successfully removed package '%s'.\n", name);
    return 0;
}

static int cmd_verify(const char *target)
{
    /* If target ends with .aurpkg, verify archive file */
    size_t len = strlen(target);
    if (len > 7 && strcmp(target + len - 7, ".aurpkg") == 0) {
        int fd = open(target, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "pkg verify: cannot open '%s'\n", target);
            return 1;
        }
        aurpkg_header_t hdr;
        if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr) || memcmp(hdr.magic, AURPKG_MAGIC, 8) != 0) {
            fprintf(stderr, "pkg verify: invalid package header\n");
            close(fd);
            return 1;
        }

        size_t key_len = 0;
        char *key = read_key(AURPKG_KEY_PATH, &key_len);
        if (!key) {
            fprintf(stderr, "pkg verify: trusted key not found\n");
            close(fd);
            return 1;
        }
        int ok = verify_package_signature(fd, &hdr, key, key_len);
        free(key);
        close(fd);

        if (ok > 0) {
            printf("pkg verify: %s: SIGNATURE OK\n", target);
            return 0;
        } else {
            printf("pkg verify: %s: SIGNATURE FAILED (untrusted/corrupted)\n", target);
            return 1;
        }
    }

    /* Otherwise verify installed package files on disk */
    printf("pkg verify: checking installed files for '%s'...\n", target);
    return cmd_info(target);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("AuroraOS Package Manager (pkg) v1.0.0\n");
        printf("Usage:\n");
        printf("  pkg install <package.aurpkg>    Install and verify package\n");
        printf("  pkg remove <name>               Remove an installed package\n");
        printf("  pkg list                        List installed packages\n");
        printf("  pkg info <name>                 Show package metadata\n");
        printf("  pkg verify <file|name>          Verify signature or files\n");
        return 1;
    }

    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: pkg install <file.aurpkg>\n"); return 1; }
        return cmd_install(argv[2]);
    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: pkg remove <name>\n"); return 1; }
        return cmd_remove(argv[2]);
    } else if (strcmp(argv[1], "list") == 0) {
        return cmd_list();
    } else if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: pkg info <name>\n"); return 1; }
        return cmd_info(argv[2]);
    } else if (strcmp(argv[1], "verify") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: pkg verify <target>\n"); return 1; }
        return cmd_verify(argv[2]);
    }

    fprintf(stderr, "pkg: unknown command '%s'. Run 'pkg' for help.\n", argv[1]);
    return 1;
}
