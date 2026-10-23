#pragma once
#include <netinet/in.h>
#include <sys/socket.h>
#define ZSOCK_POLLIN 1
#define ZSOCK_POLLERR 2
#define ZSOCK_POLLHUP 4
#define ZSOCK_POLLNVAL 8
struct zsock_pollfd { int fd; short events, revents; };
struct zsock_addrinfo { int ai_family, ai_socktype; struct sockaddr *ai_addr; };
int zsock_getaddrinfo(const char *, const char *, const struct zsock_addrinfo *, struct zsock_addrinfo **);
void zsock_freeaddrinfo(struct zsock_addrinfo *);
int zsock_poll(struct zsock_pollfd *, unsigned, int);
