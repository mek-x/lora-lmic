#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "lmic.h"
#include "debug.h"

#define SERVER "router.eu.thethings.network"
#define PORT 1700
#define PORTNAME "1700"

#define PROTOCOL_VERSION  	0x02
#define PKT_PUSH_DATA 		0x00
#define PKT_PUSH_ACK  		0x01
#define PKT_PULL_DATA 		0x02
#define PKT_PULL_RESP 		0x03
#define PKT_PULL_ACK  		0x04

#define STATUS_SIZE 1024

const unsigned char gwEui[] = {0x5E, 0x57, 0x50, 0xFF, 0xFF, 0xF7, 0x6A, 0x7E};

struct sockaddr_in si_other;
int s, slen=sizeof(si_other);

// LMIC application callbacks not used in his example
void os_getArtEui (u1_t* buf) {
}

void os_getDevEui (u1_t* buf) {
}

void os_getDevKey (u1_t* buf) {
}

void onEvent (ev_t ev) {
}

static void sendMsg(char *msg, int length)
{
    struct addrinfo hints;
    struct addrinfo* res = NULL;

    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_ADDRCONFIG;

    int err = getaddrinfo(SERVER, PORTNAME, &hints, &res);
    if (err!=0) {
        printf("failed to resolve remote socket address (err=%d)", err);
    }

    if (sendto(s, (char *)msg, length, 0 , res->ai_addr, res->ai_addrlen)==-1)
    {
        perror("sendMsg()");
        exit(1);
    }
}

static void sendStatus() {
    static char msg[STATUS_SIZE];
    char timestamp[24];
    time_t t;

    /* Set protocol version */
    msg[0] = PROTOCOL_VERSION;

	/* Random token */
    msg[1] = (char) rand();
    msg[2] = (char) rand();

	/* Packet type */
    msg[3] = PKT_PUSH_DATA;

	/* Gateway UID */
    msg[4] = (unsigned char) gwEui[0];
    msg[5] = (unsigned char) gwEui[1];
    msg[6] = (unsigned char) gwEui[2];
    msg[7] = (unsigned char) gwEui[3];
    msg[8] = (unsigned char) gwEui[4];
    msg[9] = (unsigned char) gwEui[5];
    msg[10] = (unsigned char) gwEui[6];
    msg[11] = (unsigned char) gwEui[7];

	/* The offset to start writing the JSON data */
    int offset = 12;

    /* Get the current time */
    t = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%F %T %Z", gmtime(&t));

	/* Prepare the JSON status data */
    int jsonLen = snprintf(msg + offset, STATUS_SIZE - offset, "{\"stat\":{\"time\":\"%s\",\"lati\":%.5f,\"long\":%.5f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u}}", timestamp, 0.0, 0.0, 0, 0, 0, 0, 0.0, 0, 0);
    printf("stat update: %s\n", msg + offset);

    /* send the update to the network server */
    sendMsg(msg, offset + jsonLen);
}

static void cyclic (osjob_t* job) {
    sendStatus();
    os_setTimedCallback(job, os_getTime()+sec2osticks(30), cyclic);
}

int main () {
    osjob_t initjob;

    // initialize runtime env
    os_init();
    // initialize debug library
    debug_init();
    // setup initial job
    os_setCallback(&initjob, cyclic);
    // execute scheduled jobs and events

    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror("main()");
        exit(1);
    }
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);

    /* display result */
    printf("Gateway EUI: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
           (unsigned char) gwEui[0],
           (unsigned char) gwEui[1],
           (unsigned char) gwEui[2],
           (unsigned char) gwEui[3],
           (unsigned char) gwEui[4],
           (unsigned char) gwEui[5],
           (unsigned char) gwEui[6],
           (unsigned char) gwEui[7]);

    os_runloop();
    // (not reached)
    return 0;
}
