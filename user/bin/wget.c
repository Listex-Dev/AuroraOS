#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void parse_url(const char *url, char *host, size_t host_len,
                      uint16_t *port, char *path, size_t path_len)
{
    *port = 80;
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    const char *slash = strchr(p, '/');
    if (slash) {
        strncpy(path, slash, path_len - 1);
        path[path_len - 1] = '\0';
    } else {
        strncpy(path, "/", path_len - 1);
        path[path_len - 1] = '\0';
        slash = p + strlen(p);
    }

    size_t host_part_len = (size_t)(slash - p);
    char host_part[128];
    if (host_part_len >= sizeof(host_part)) {
        host_part_len = sizeof(host_part) - 1;
    }
    memcpy(host_part, p, host_part_len);
    host_part[host_part_len] = '\0';

    char *colon = strchr(host_part, ':');
    if (colon) {
        *colon = '\0';
        *port = (uint16_t)atoi(colon + 1);
    }

    strncpy(host, host_part, host_len - 1);
    host[host_len - 1] = '\0';
}

static const char *get_default_filename(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    if (last_slash && *(last_slash + 1) != '\0') {
        return last_slash + 1;
    }
    return "index.html";
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: wget [-O output_file] <URL>\n");
        return 1;
    }

    const char *out_file = NULL;
    const char *url = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-O") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (argv[i][0] != '-') {
            url = argv[i];
        }
    }

    if (!url) {
        fprintf(stderr, "wget: missing URL\n");
        return 1;
    }

    char host[128];
    char path[256];
    uint16_t port = 80;
    parse_url(url, host, sizeof(host), &port, path, sizeof(path));

    if (!out_file) {
        out_file = get_default_filename(path);
    }

    printf("--%s--  %s\n", "2026-10-19 12:00:00", url);
    printf("Resolving %s (%s)... ", host, host);

    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list[0]) {
        printf("failed: Name or service not known.\n");
        fprintf(stderr, "wget: unable to resolve host address '%s'\n", host);
        return 1;
    }

    struct in_addr ip_in;
    memcpy(&ip_in.s_addr, he->h_addr_list[0], 4);
    char *ip_str = inet_ntoa(ip_in);
    printf("%s\n", ip_str);

    printf("Connecting to %s (%s)|%s|:%u... ", host, host, ip_str, port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("failed: Cannot create socket.\n");
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr = ip_in;

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        printf("failed: Connection refused.\n");
        close(sock);
        return 1;
    }
    printf("connected.\n");

    /* Format HTTP/1.0 GET request */
    char req[512];
    int req_len = snprintf(req, sizeof(req),
                           "GET %s HTTP/1.0\r\n"
                           "Host: %s\r\n"
                           "User-Agent: Wget/1.20 (AuroraOS)\r\n"
                           "Accept: */*\r\n"
                           "Connection: close\r\n\r\n",
                           path, host);

    printf("HTTP request sent, awaiting response... ");
    if (write(sock, req, req_len) != req_len) {
        printf("failed: Cannot send request.\n");
        close(sock);
        return 1;
    }

    /* Read initial response header */
    char buf[2048];
    int n = read(sock, buf, sizeof(buf) - 1);
    if (n <= 0) {
        printf("failed: Empty response.\n");
        close(sock);
        return 1;
    }
    buf[n] = '\0';

    /* Parse status code */
    int status_code = 0;
    char *status_line = strstr(buf, "HTTP/1.");
    if (status_line) {
        char *space = strchr(status_line, ' ');
        if (space) {
            status_code = atoi(space + 1);
        }
    }

    if (status_code != 200 && (status_code < 200 || status_code > 299)) {
        printf("%d (Error)\n", status_code);
        fprintf(stderr, "wget: server returned error: %d\n", status_code);
        close(sock);
        return 1;
    }
    printf("200 OK\n");

    /* Find header boundary: \r\n\r\n or \n\n */
    char *body = strstr(buf, "\r\n\r\n");
    int body_offset = 0;
    if (body) {
        body_offset = (int)(body + 4 - buf);
    } else {
        body = strstr(buf, "\n\n");
        if (body) {
            body_offset = (int)(body + 2 - buf);
        } else {
            body_offset = n;
        }
    }

    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        fprintf(stderr, "wget: cannot open output file '%s'\n", out_file);
        close(sock);
        return 1;
    }

    printf("Saving to: '%s'\n\n", out_file);

    unsigned long long total_bytes = 0;

    /* Write body bytes from first buffer */
    int initial_body_len = n - body_offset;
    if (initial_body_len > 0) {
        write(out_fd, buf + body_offset, initial_body_len);
        total_bytes += initial_body_len;
    }

    /* Read remainder of stream until EOF */
    while ((n = read(sock, buf, sizeof(buf))) > 0) {
        write(out_fd, buf, n);
        total_bytes += n;
    }

    close(out_fd);
    close(sock);

    printf("2026-10-19 12:00:00 (10.0 MB/s) - '%s' saved [%llu/%llu]\n\n",
           out_file, total_bytes, total_bytes);
    return 0;
}
