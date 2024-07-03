/*
* Description: This program sets up a UDP client that sends a file in 10 byte packets to a server using rdt3.0 and simulates packets being dropped or corrupted.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>

#define BUF_SIZE 1024

typedef struct {
    int seq_ack;
    int len;
    int cksum;
} Header;

typedef struct {
    Header header;
    char data[10];
} Packet;

int compute_checksum(Packet packet) {
    int checksum;
    packet.header.cksum = 0;
    checksum = 0;
    char *ptr = (char *)&packet;
    char *end = ptr+sizeof(packet.header)+sizeof(packet.data);
    while(ptr < end) {
        checksum ^= *ptr++;
    }
    return checksum;
}

void clientSend(Packet packet, int checksum, struct sockaddr_in servAddr, socklen_t addrLen, int sockfd) {
    if (rand() % 5 == 0) { //simulate checksum
        packet.header.cksum = 0;
    }
    else {
        packet.header.cksum = checksum;
    }

    if (rand() % 5 != 0) { //simulate packet being skipped
        sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*) &servAddr, addrLen);
        printf("packet sent by client:\n");
        printf("{checksum of packet: %d  length of packet: %d seq number of packet: %d data: %s}\n\n", packet.header.cksum, packet.header.len, packet.header.seq_ack, packet.data);
    }
    else {
        printf("PACKET DROPPED\n\n");
    }
}

int ack_in_time(int sockfd) {
    int rv = 0;
    struct timeval tv;
    //int rv;
    fd_set readfds;
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    rv = select(sockfd+1, &readfds, NULL, NULL, &tv);

    return rv;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <IP address> <port> <src_filename>\n", argv[0]);
        exit(1);
    }



    int sockfd;
    struct sockaddr_in servAddr;
    socklen_t addrLen = sizeof(struct sockaddr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    struct hostent *host;
    host = (struct hostent *)gethostbyname(argv[1]);

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(atoi(argv[2]));
    servAddr.sin_addr = *((struct in_addr *)host->h_addr);


    //char sbuf[BUF_SIZE];
    //char rbuf[BUF_SIZE];


    Packet packet;
    Packet ack;
    int seq = 0;
    int checksum;
    int ack_checksum;
    int retries;

    FILE *file = fopen(argv[3], "rb");
    ssize_t bytesRead;
    while ((bytesRead = fread(packet.data, 1, sizeof(packet.data), file)) > 0) {
        packet.header.seq_ack = seq;
        packet.header.len = bytesRead;

        checksum = compute_checksum(packet);

        clientSend(packet, checksum, servAddr, addrLen, sockfd);

        int rv = 0;

        while (rv == 0) {

            rv = ack_in_time(sockfd);

            if (rv == 0) {
                printf("TIMEOUT. NO ACK RECEIVED. RESENDING LAST PACKET\n");

                clientSend(packet, checksum, servAddr, addrLen, sockfd);
            }
            else if (rv == 1) {
                recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*) &servAddr, &addrLen);
                printf("ACK received- ack number: %d checksum: %d\n\n", ack.header.seq_ack, ack.header.cksum);

                ack_checksum = ack.header.cksum;
                checksum = compute_checksum(ack);

                if (ack_checksum != checksum || (ack.header.seq_ack != packet.header.seq_ack)) {
                    rv = 0;

                    checksum = compute_checksum(packet);
                    printf("CHECKSUMS DON'T MATCH OR ACK DOESN'T MATCH SEQ NUMBER. RESENDING LAST PACKET\n");
                    clientSend(packet, checksum, servAddr, addrLen, sockfd);
                }
            }
        }

        seq = (seq+1)%2;
    }
    

    retries = 0;
    while (retries <= 3) {
        sendto(sockfd, "", 0, 0, (struct sockaddr*) &servAddr, addrLen);
        printf("message with zero bytes sent\n");
        if (ack_in_time(sockfd) == 0 && retries != 3) {
            printf("TIMEOUT. NO ACK RECEIVED. RESENDING LAST PACKET\n");
        }
        else if (ack_in_time(sockfd) == 1) {
            recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*) &servAddr, &addrLen);
            printf("ACK received- ack number: %d checksum: %d\n\n", ack.header.seq_ack, ack.header.cksum);
            break;
        }
        retries += 1;
    }

    fclose(file);

    close(sockfd);

    return 0;
}