#include "csapp.h"

int main(int argc, char **argv)
{
    struct addrinfo *p, *listp;
    struct addrinfo hints;
    char buf[MAXLINE];
    int rc, flags;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(0);
    }

    /* Get a list of addrinfo records */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4만을 허용
    hints.ai_sockty
    

    exit(0);
}
