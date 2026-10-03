//
// Created by Tim Healey on 10/2/26.
//
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT "3490" // the port client will be connecting to
#define IP "127.0.0.1"
#include <arpa/inet.h>

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main() {
    // socket file descriptor
    int sockfd;
    int rv;
    struct addrinfo hints, *servinfo, *dummy;
    char s[INET6_ADDRSTRLEN];
    // clear out hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_INET;    // ipv4
    hints.ai_socktype=SOCK_STREAM;  // tcp
    // get address info
    if (getaddrinfo(IP, PORT, &hints, &servinfo) != 0) {
        std::cerr << "getaddrinfo error" << std::endl;
        return 1;
    }
    // make socket connection by testing each element in servinfo ll
    for (dummy = servinfo; dummy != NULL; dummy = dummy->ai_next) {
        // make socket
        if (sockfd = (socket(dummy->ai_family, dummy->ai_socktype, dummy->ai_protocol)) == -1) {
            std::cerr << "socket error" << std::endl;
            continue;
        }
        inet_ntop(dummy->ai_family,get_in_addr((struct sockaddr *)dummy->ai_addr),s, sizeof s);
        printf("client: attempting connection to %s\n", s);
        // connect
        if (connect(sockfd, dummy->ai_addr, dummy->ai_addrlen) == -1) {
            std::cerr << "connect error" << std::endl;
            return 1;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if (dummy == NULL) {
        std::cerr << "client: failed to connect" << std::endl;
        return 1;
    }



    // receive data
    return 0;
}