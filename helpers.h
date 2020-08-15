#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <netinet/tcp.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

// #define BUFLEN		2560	// dimensiunea maxima a calupului de date
#define BUFLEN		1571	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	10	// numarul maxim de clienti in asteptare
#define MAXBUFLEN 1501
#define IDLEN 11
#define TOPICLEN 51

#define INVALID "invalid"
#define ARGPROBLEM "A problem has occured!"
#define CLOSED "closed"
#define EXIT "exit"
#define SIZE "invalid size"
#define SUBSCRIBED "subscribed"
#define UNSUBSCRIBED "unsubscribed"

typedef struct {
	char topic[50];
	char tip_date;
	char continut[1500];
} Packet;

typedef struct __attribute__((packed)){
	Packet payload;
	struct sockaddr src_addr;
	socklen_t addrlen;
} Topic_packet;

typedef struct  __attribute__((packed)){
	char sign;
	uint32_t number;
	// char number[4];
} Integer;

typedef struct __attribute__((packed)){
	uint16_t real;
} Real_positive;

typedef struct __attribute__((packed)){
	char sign;
	uint32_t number;
	uint8_t power;
} Real;

#endif
