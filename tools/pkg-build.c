/*
 * AuroraOS Package Builder Host Tool
 * Creates signed .aurpkg packages and corrupted test packages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../user/libc/include/aurpkg.h"
#include "../user/libc/include/sha256.h"

/* Embed SHA-256 implementation for host build */
#include "../user/libc/src/sha256.c"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s pack <out.aurpkg> <name> <version> <description> <key_file> <dest:src:mode> ...\n", prog);
    fprintf(stderr, "  %s corrupt <in.aurpkg> <out.aurpkg>\n", prog);
    exit(1);
}

static char *read_file(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open file %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sz ? sz : 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (sz > 0 && fread(buf, 1, sz, f) != (size_t)sz) {
        fprintf(stderr, "Error: failed to read %s\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size_out = (size_t)sz;
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        usage(argv[0]);
    }

    if (strcmp(argv[1], "corrupt") == 0) {
        if (argc != 4) usage(argv[0]);
        size_t sz = 0;
        char *buf = read_file(argv[2], &sz);
        if (!buf || sz < sizeof(aurpkg_header_t) + 10) {
            fprintf(stderr, "Error: invalid input package to corrupt\n");
            return 1;
        }

        /* Corrupt a byte in the payload area past the header */
        size_t corrupt_pos = sizeof(aurpkg_header_t) + sizeof(aurpkg_file_t) + 2;
        if (corrupt_pos >= sz) corrupt_pos = sz - 1;
        buf[corrupt_pos] ^= 0xFF;

        FILE *out = fopen(argv[3], "wb");
        if (!out) {
            fprintf(stderr, "Error: cannot open output %s\n", argv[3]);
            free(buf);
            return 1;
        }
        fwrite(buf, 1, sz, out);
        fclose(out);
        free(buf);
        printf("[pkg-build] Corrupted package written to %s (tampered byte at offset %zu)\n", argv[3], corrupt_pos);
        return 0;
    }

    if (strcmp(argv[1], "pack") == 0) {
        if (argc < 7) usage(argv[0]);

        const char *out_path = argv[2];
        const char *pkg_name = argv[3];
        const char *pkg_ver = argv[4];
        const char *pkg_desc = argv[5];
        const char *key_file = argv[6];

        size_t key_len = 0;
        char *key_data = read_file(key_file, &key_len);
        if (!key_data) {
            fprintf(stderr, "Error: cannot read key file %s\n", key_file);
            return 1;
        }
        /* Trim newline if present */
        while (key_len > 0 && (key_data[key_len - 1] == '\n' || key_data[key_len - 1] == '\r')) {
            key_len--;
        }

        int file_count = argc - 7;
        if (file_count <= 0 || file_count > AURPKG_MAX_FILES) {
            fprintf(stderr, "Error: invalid number of files: %d\n", file_count);
            free(key_data);
            return 1;
        }

        aurpkg_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        memcpy(hdr.magic, AURPKG_MAGIC, 8);
        strncpy(hdr.name, pkg_name, sizeof(hdr.name) - 1);
        strncpy(hdr.version, pkg_ver, sizeof(hdr.version) - 1);
        strncpy(hdr.description, pkg_desc, sizeof(hdr.description) - 1);
        hdr.file_count = (uint32_t)file_count;

        aurpkg_file_t *files = calloc(file_count, sizeof(aurpkg_file_t));
        char **file_payloads = calloc(file_count, sizeof(char *));

        uint32_t total_payload_bytes = 0;

        for (int i = 0; i < file_count; i++) {
            char *arg = strdup(argv[7 + i]);
            char *dest = strtok(arg, ":");
            char *src = strtok(NULL, ":");
            char *mode_str = strtok(NULL, ":");

            if (!dest || !src || !mode_str) {
                fprintf(stderr, "Error: invalid file spec '%s' (expected dest:src:mode)\n", argv[7 + i]);
                free(arg);
                return 1;
            }

            size_t src_size = 0;
            char *payload = read_file(src, &src_size);
            if (!payload) {
                fprintf(stderr, "Error: could not read src file '%s'\n", src);
                free(arg);
                return 1;
            }

            strncpy(files[i].path, dest, sizeof(files[i].path) - 1);
            files[i].size = (uint32_t)src_size;
            files[i].mode = (uint32_t)strtoul(mode_str, NULL, 8);
            sha256_hash_bytes(payload, src_size, files[i].sha256);

            file_payloads[i] = payload;
            total_payload_bytes += (uint32_t)src_size;
            free(arg);
        }

        hdr.total_size = sizeof(aurpkg_header_t) +
                         file_count * sizeof(aurpkg_file_t) +
                         total_payload_bytes;

        /* Calculate HMAC-SHA256 signature over metadata + files + payloads */
        sha256_ctx sign_ctx;
        sha256_init(&sign_ctx);
        sha256_update(&sign_ctx, hdr.magic, 8);
        sha256_update(&sign_ctx, hdr.name, sizeof(hdr.name));
        sha256_update(&sign_ctx, hdr.version, sizeof(hdr.version));
        sha256_update(&sign_ctx, hdr.description, sizeof(hdr.description));
        sha256_update(&sign_ctx, &hdr.file_count, sizeof(hdr.file_count));

        for (int i = 0; i < file_count; i++) {
            sha256_update(&sign_ctx, &files[i], sizeof(aurpkg_file_t));
            sha256_update(&sign_ctx, file_payloads[i], files[i].size);
        }

        uint8_t digest[32];
        sha256_final(&sign_ctx, digest);

        /* HMAC with secret key */
        sha256_hmac(key_data, key_len, digest, 32, hdr.signature);

        FILE *out = fopen(out_path, "wb");
        if (!out) {
            fprintf(stderr, "Error: failed to open %s for writing\n", out_path);
            return 1;
        }

        fwrite(&hdr, 1, sizeof(hdr), out);
        for (int i = 0; i < file_count; i++) {
            fwrite(&files[i], 1, sizeof(aurpkg_file_t), out);
            if (files[i].size > 0) {
                fwrite(file_payloads[i], 1, files[i].size, out);
            }
            free(file_payloads[i]);
        }
        fclose(out);

        free(files);
        free(file_payloads);
        free(key_data);

        printf("[pkg-build] Package %s %s successfully created:\n", hdr.name, hdr.version);
        printf("  Output:    %s\n", out_path);
        printf("  Files:     %u\n", hdr.file_count);
        printf("  Total sz:  %u bytes\n", hdr.total_size);
        printf("  Signature: %s\n", hdr.signature);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
