#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#define PORT "3490"
#define BACKLOG 10
#define MAXDATASIZE 100 // max number of bytes we can get at once

size_t write_callback(
    char* contents,
    size_t size,
    size_t nmemb,
    void* userp
) {
    size_t total = size * nmemb;

    std::string* response =
        static_cast<std::string*>(userp);

    response->append(contents, total);

    return total;
}

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
    char recvBuf[MAXDATASIZE];
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cerr << "Failed to initialize curl\n";
        return 1;
    }
    // Get API key from environment
    const char* api_key = std::getenv("OPENROUTER_API_KEY");

    if (api_key == nullptr) {
        std::cerr << "OPENROUTER_API_KEY is not set\n";
        curl_easy_cleanup(curl);
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://openrouter.ai/api/alpha/decisions");
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
            // receive data to connection
            char buf[MAXDATASIZE];
            if ((numbytes = recv(newfd, buf, MAXDATASIZE - 1, 0)) == -1) {
                std::cerr << "recv error" << std::endl;
                return 1;
            }
            buf[numbytes] = '\0';
            printf("client: received '%s'\n", buf);
            std::string environment = buf;
            // JSON body
            std::string request_body = R"({
                    "model": "typesafe/jev-1.13",
                    "state": {
                        "agent_details": "L4 autonomous driving agent",
                        "environment": ")" + environment + R"("
                    },
                    "questions": {
                        "decision": {
                            "type": "score",
                            "instructions": "What should you do?",
                            "criteria": [
                                "pullover",
                                "require human driver attention",
                                "require human driver to manually drive",
                                "continue autonomously driving the vehicle"
                            ]
                        }
                    }
                })";
                // CURLOPT_POSTFIELDS implicity makes the request a post
                curl_easy_setopt(
                    curl,
                    CURLOPT_POSTFIELDS,
                    request_body.c_str()
                );

                // Headers
                struct curl_slist* headers = nullptr;
                std::string auth_header = "Authorization: Bearer " + std::string(api_key);
                headers = curl_slist_append(headers,auth_header.c_str());
                headers = curl_slist_append(headers,"Content-Type: application/json");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                // Capture response
                std::string response;

                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

                // Perform request
                CURLcode result = curl_easy_perform(curl);
                nlohmann::json response_json = nlohmann::json::parse(response);
                std::cout << "JSON response:\n";
                std::cout << response << '\n';
                auto& decision = response_json["answers"]["decision"];

                double i = 0;
                std::string sendRes;
                for (const auto& [key, probability] :
                    decision["probabilities"].items()) {
                    std::string actionStr = decision["legend"][key];
                    //Action actionEnum = stringToAction(actionStr);
                    std::cout << actionStr << ": " << probability.get<double>() << '\n';
                    if (i < probability.get<double>()) {
                        i = probability.get<double>();
                        sendRes = actionStr;
                    }
                }
                send(newfd, sendRes.c_str(), sendRes.size(), 0);
                std::cout << i << '\n';
            close(newfd);
            curl_slist_free_all(headers);
            exit(0);
        }
        close(newfd); // parent doesnt need this
        curl_easy_cleanup(curl);
    }
    return 0;
}