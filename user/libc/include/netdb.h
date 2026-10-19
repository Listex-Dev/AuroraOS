#ifndef _NETDB_H
#define _NETDB_H

#include <sys/socket.h>

struct hostent {
    char  *h_name;       /* Official name of host */
    char **h_aliases;    /* Alias list */
    int    h_addrtype;   /* Host address type (AF_INET) */
    int    h_length;     /* Length of address in bytes */
    char **h_addr_list;  /* List of addresses from name server */
};

#define h_addr h_addr_list[0] /* For backward compatibility */

#define HOST_NOT_FOUND 1
#define TRY_AGAIN      2
#define NO_RECOVERY    3
#define NO_DATA        4

extern int h_errno;

struct hostent *gethostbyname(const char *name);
struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type);

#endif /* _NETDB_H */
