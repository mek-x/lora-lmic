#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "udp.h"

#define PORT 19535 // 0x4c4f - 'LO'

void transmit(uint8_t *buf, size_t bufSize) {
    int sockfd;
    struct sockaddr_in     servaddr;

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&one,sizeof(one)) < 0) {
        perror("Error in setting Broadcast option");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

        // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = (in_addr_t) 0xff00007f;

    ssize_t n;

    n = sendto(sockfd, buf, bufSize, 0,
        (const struct sockaddr *)&servaddr, sizeof(servaddr));

    if (n < 0)
        perror("sendto() error:");

    printf("sendto(): sent %d bytes\n", n);

    close(sockfd);
}

void receive(uint8_t *buf, size_t bufSize) {
    int sockfd;
    struct sockaddr_in s_in_me, s_in_other;

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one)) < 0) {
        perror("Error in setting reuse option");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&s_in_me, 0, sizeof(s_in_me));

        // Filling server information
    s_in_me.sin_family = AF_INET;
    s_in_me.sin_port = htons(PORT);
    s_in_me.sin_addr.s_addr = (in_addr_t) 0xff00007f;

    if(bind(sockfd, (const struct sockaddr *)&s_in_me, sizeof(s_in_me)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    ssize_t n;
    socklen_t socklen = sizeof(struct sockaddr_in);

    n = recvfrom(sockfd, buf, bufSize, 0,
        (struct sockaddr *)&s_in_other, &socklen);

    if (n < 0)
        perror("recvfrom() error:");

    printf("recvfrom(): received %d bytes\n", n);

    close(sockfd);
}
