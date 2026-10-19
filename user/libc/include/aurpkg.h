#ifndef _AURPKG_H
#define _AURPKG_H

#include <stdint.h>

#define AURPKG_MAGIC "AURPKG10"
#define AURPKG_MAX_FILES 64
#define AURPKG_KEY_PATH "/etc/pkg/trusted/aurora.key"
#define AURPKG_DB_PATH  "/var/lib/pkg/db"

typedef struct {
    char magic[8];           /* "AURPKG10" */
    char name[64];           /* e.g. "sample" */
    char version[32];        /* e.g. "1.0.0" */
    char description[128];   /* Package description */
    char signature[65];      /* 64-hex HMAC-SHA256 signature + '\0' */
    uint32_t file_count;     /* Number of files */
    uint32_t total_size;     /* Total byte size of package */
} __attribute__((packed)) aurpkg_header_t;

typedef struct {
    char path[128];          /* Destination path, e.g. "/bin/sample" */
    uint32_t size;           /* Size of payload */
    uint32_t mode;           /* File permissions, e.g. 0755 */
    char sha256[65];         /* SHA256 of file payload + '\0' */
} __attribute__((packed)) aurpkg_file_t;

#endif /* _AURPKG_H */
