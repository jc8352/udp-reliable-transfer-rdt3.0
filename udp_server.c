/*
* Description: This program sets up a UDP server that receives a file in 10 byte packets from a client using rdt3.0 and simulates an ACK being dropped or corrupted.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

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

void serverSend(Packet ack, struct sockaddr_in clientAddr, socklen_t addrLen, int sockfd) {
	int checksum;
	if (rand() % 5 != 0) { //checksum

        checksum = compute_checksum(ack);
        ack.header.cksum = checksum;

	}
	else { 
		ack.header.cksum = 0;
	}

	if (rand() % 5 != 0) { //send ack

		sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*) &clientAddr, addrLen);
		printf("ACK sent- ack number: %d checksum: %d\n\n", ack.header.seq_ack, ack.header.cksum);
	}
	else { //simulate ack being dropped
		printf("ACK dropped\n\n");
	}
}


int main(int argc, char* argv[]) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s <port> <dst_filename>\n", argv[0]);
		exit(1);
	}


	int sockfd, connfd;
	//char rbuf[BUF_SIZE];
	struct sockaddr_in servAddr, clientAddr;

	socklen_t addrLen = sizeof(struct sockaddr);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(argv[1]));
	servAddr.sin_addr.s_addr = INADDR_ANY;

	bind(sockfd, (struct sockaddr *)&servAddr, sizeof(struct sockaddr));

	printf("Server waiting for client at port %s\n", argv[1]);

	//char sbuf[BUF_SIZE];



	Packet packet;
	Packet ack;
	ack.header.seq_ack = 1;
	ack.header.len = 0;
	int checksum;
	int client_checksum;
	int ack_checksum;


    checksum = compute_checksum(ack);
    ack.header.cksum = checksum;


	FILE *file = fopen(argv[2], "w");
	ssize_t bytesRead;
	while ((bytesRead = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&clientAddr, &addrLen)) > 0) {

        client_checksum = packet.header.cksum;
        checksum = compute_checksum(packet);

        if (checksum == client_checksum) { //if checksums match
        	if (ack.header.seq_ack == (packet.header.seq_ack+1)%2) { //if received packet is a new packet
        		fwrite(packet.data, 1, sizeof(packet.data), file);
        		printf("packet received by server: \n");
        		printf("{server computed checksum: %d  length of packet: %d seq number: %d data: %s}\n", checksum, packet.header.len, packet.header.seq_ack, packet.data);
        	}
        	else {
        		printf("duplicate packet received by server, resending ACK\n");
        	}
        	ack.header.seq_ack = packet.header.seq_ack;
        	ack.header.len = 0;

        	serverSend(ack, clientAddr, addrLen, sockfd);
        }
        else { //checksums don't match resend last ack to resend packet
        	printf("SERVER CHECKSUM DOES NOT EQUAL CLIENT CHECKSUM server computed checksum: %d client checksum: %d\n", checksum, client_checksum);

        	serverSend(ack, clientAddr, addrLen, sockfd);
        }

	}

	printf("message with zero bytes received\n");
	ack.header.seq_ack = (packet.header.seq_ack+1)%2;
	ack.header.len = 0;

	serverSend(ack, clientAddr, addrLen, sockfd);



	fclose(file);

	printf("Client with IP: %s and Port: %d sent file\n", inet_ntoa(clientAddr.sin_addr),ntohs(clientAddr.sin_port));

	close(sockfd);

	return 0;
}