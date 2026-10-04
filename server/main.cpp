#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"
#define BACKLOG 10
#define MAXDATASIZE 100 // max number of bytes we can get at once

void sigchld_handler(int s)
{
    // reap child processes
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    // look for dead processes and reap them without blocking
    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

bool sendall(int s, char *buf, int len) {
    int total = 0;
    int bytesleft = len;
    while (total < len) {
        bytesleft = len - total;
        std::cout << "Send " << bytesleft << std::endl;
        int n = send(s, buf + total, bytesleft, 0);
        if (n == -1) return false;
        total += bytesleft;
    }
    return total == len;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main() {
    std::cout << "Starting up the server" << std::endl;
    // socked file descriptor
    int sockfd, newfd, numbytes;
    struct addrinfo hints, *servinfo, *dummy;
    struct sockaddr_storage their_addr;
    int yes = 1;
    struct sigaction sa;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    // clear out hints data
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_INET;        // use ipv4
    hints.ai_socktype=SOCK_STREAM;  // use tcp
    hints.ai_flags=AI_PASSIVE;      // use my ip
    // get addr info
    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        std::cerr << "getaddrinfo error" << std::endl;
        return 1;
    }
    // find IP we can bind to
    for (dummy=servinfo; dummy != NULL; dummy=dummy->ai_next) {
        // create socket
        if ((sockfd = (socket(dummy->ai_family, dummy->ai_socktype,dummy->ai_protocol))) == -1) {
            std::cerr << "socket could not be created, moving to next" << std::endl;
            continue;
        }
        // make socket reusable
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            std::cerr << "setsockopt error, moving to next" << std::endl;
            continue;
        }
        // bind to socket
        if (bind(sockfd, dummy->ai_addr, dummy->ai_addrlen) == -1) {
            std::cerr << "bind error, moving to next" << std::endl;
            continue;
        }
        break;
    }
    if (dummy == NULL) {
        std::cerr << "getaddrinfo error" << std::endl;
        return 1;
    }
    // free linked list of ip addr
    freeaddrinfo(servinfo);
    // listen on socket with BACKLOG amount of requests in the queue at any one time
    if (listen(sockfd, BACKLOG) == -1) {
        std::cerr << "listen error" << std::endl;
        return 1;
    }

    // when sigchild occurs, call sigchld_handler
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        std::cerr << "sigaction error" << std::endl;
        return 1;
    }

    // while accept loop
    while (1) {
        std::cout << "Waiting for a new connection..." << std::endl;
        sin_size = sizeof their_addr;
        // accept new connection
        if ((newfd = accept(sockfd,(struct sockaddr *)&their_addr,&sin_size)) == -1) {
            std::cerr << "accept error" << std::endl;
            continue;
        }
        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        printf("server: got connection from %s\n", s);
        // child process
        if (!fork()) {
            close(sockfd); // child doesnt need the listener
            // send data to connection
            char *buf = "Hello, World";
            int len = strlen(buf);
            if (!sendall(newfd, buf, len)) {
                std::cerr << "send error" << std::endl;
            }
            // receive message
            char buf2[MAXDATASIZE];
            if ((numbytes = recv(newfd, buf2, MAXDATASIZE - 1, 0)) == -1) {
                std::cerr << "recv error" << std::endl;
                return 1;
            }
            buf2[numbytes] = '\0';
            printf("client: received '%s'\n", buf2);
            close(newfd);
            exit(0);
        }
        close(newfd); // parent doesnt need this
    }
    return 0;
}