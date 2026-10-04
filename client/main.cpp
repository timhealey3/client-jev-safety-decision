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
#include <arpa/inet.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#define PORT "3490" // the port client will be connecting to
#define IP "127.0.0.1"  // the ip address
#define MAXDATASIZE 100 // max number of bytes we can get at once
using json = nlohmann::json;

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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

enum class Action {
    pullover,
    autonomous,
    attention,
    takeover,
    unkown
};

Action stringToAction(std::string s) {
    if (s == "pullover") return Action::pullover;
    if (s == "continue autonomously driving the vehicle") return Action::autonomous;
    if (s == "require human driver attention") return Action::attention;
    if (s == "require human driver to manually drive") return Action::takeover;
    return Action::unkown;
}

int main() {
    // socket file descriptor
    int sockfd, numbytes, rv;
    struct addrinfo hints, *servinfo, *dummy;
    char s[INET6_ADDRSTRLEN];
    char buf[MAXDATASIZE];
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
    // receive message
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
    std::cerr << "recv error" << std::endl;
        return 1;
    }
    buf[numbytes] = '\0';
    printf("client: received '%s'\n", buf);

    // JSON body
    std::string request_body = R"({
        "model": "typesafe/jev-1.13",
        "state": {
            "agent_details": "L4 autonomous driving agent",
            "environment": "It started to lightly rain while driving on the highway halfway to destination"
        },
        "questions": {
            "decision": {
                "type": "score",
                "instructions": "What should you do",
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
    json response_json = json::parse(response);
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
    send(sockfd, sendRes.c_str(), sendRes.size(), 0);
    std::cout << i << '\n';
    // cleanup
    close(sockfd);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 0;
}
