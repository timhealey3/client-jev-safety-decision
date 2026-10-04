//
// Created by Tim Healey on 10/2/26.
//
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <random>

#define PORT "3490" // the port client will be connecting to
#define IP "127.0.0.1"  // the ip address
#define MAXDATASIZE 100 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

bool sendall(int s, std::string buf, int len) {
    int total = 0;
    int bytesleft = len;
    while (total < len) {
        bytesleft = len - total;
        std::cout << "Send bytes left: " << bytesleft << std::endl;
        std::cout << "Send message: " << buf << std::endl;
        int n = send(s, buf.c_str() + total, bytesleft, 0);
        if (n == -1) return false;
        total += bytesleft;
    }
    return total == len;
}

std::string getEnv() {
    std::vector<std::string> env = {
        "It started to lightly rain while driving on the highway halfway to destination",
        "There are traffic cones and a reduced speed zone",
        "you are approaching a intersection",
        "the sun is setting",
        "normal driving conditions",
        "the car in front is swerving",
        "there are first responder sirens",
        "the car has detected LIDAR is broken",
        "the car has 5 miles of fuel left"
    };
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distrib(0, env.size() - 1);
    return env[distrib(gen)];
}

int main() {
    // socket file descriptor
    int sockfd, numbytes, rv;
    struct addrinfo hints, *servinfo, *dummy;
    char s[INET6_ADDRSTRLEN];
    char buf[MAXDATASIZE];
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
        if ((sockfd = socket(dummy->ai_family,
                     dummy->ai_socktype,
                     dummy->ai_protocol)) == -1) {
            std::cerr << "socket error" << std::endl;
            continue;
        }
        inet_ntop(dummy->ai_family,get_in_addr((struct sockaddr *) dummy->ai_addr), s, sizeof s);
        printf("client: attempting connection to %s", s);
        std::cout << ":" << PORT << std::endl;
        // connect
        if (connect(sockfd, dummy->ai_addr, dummy->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            continue;
        }
        break;
    }
    if (dummy == NULL) {
        std::cerr << "client: failed to connect" << std::endl;
        return 1;
    }
    inet_ntop(dummy->ai_family, get_in_addr((struct sockaddr *) dummy->ai_addr), s, sizeof s);
    printf("client: connected to %s\n", s);
    freeaddrinfo(servinfo);
    // send message
    std::string sendBuf = getEnv();
    if (!sendall(sockfd, sendBuf, sendBuf.size())) {
        std::cerr << "send error" << std::endl;
    }
    // receive server response
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
        std::cerr << "recv error" << std::endl;
        return 1;
    }
    buf[numbytes] = '\0';
    printf("client: received '%s'\n", buf);
    // cleanup
    close(sockfd);
    return 0;
}
