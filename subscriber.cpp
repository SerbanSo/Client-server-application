#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN+5];
	struct sockaddr src_addr;
	socklen_t addrlen;

	if (argc < 4) {
		usage(argv[0]);
	}

	// Deschide socket-ul
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// Initializare adresa serverului
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret < 0, "inet_aton");

	// Cerere de conectare catre server
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// Trimite ID_CLIENT la server
	memcpy(buffer, argv[1], strlen(argv[1])+1);
	send(sockfd, buffer, strlen(buffer)+1, 0);

	// Seteaza TCP_NODELAY
	int nodelay = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(int));
	DIE(ret < 0, "setsockopt");

	fd_set read_fds;	// Multimea de citire folosita in select()
	fd_set tmp_fds;		// Multime folosita temporar
	int fdmax;			// Valoare maxima fd din multimea read_fds

	// Se goleste multimea de descriptori de citire (read_fds) si 
	// multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Se adauga socketul care este conectat la server si 
	// file descriptorul pentru STDIN in multimea read_fds
	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds;

  		// se citeste de la tastatura
		memset(buffer, 0, BUFLEN);

        ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

        for(int i = 0; i <= fdmax; i++){
            if (FD_ISSET(i, &tmp_fds)){
                if(i == STDIN_FILENO){
					// Se citeste mesajul de la stdin
					fgets(buffer, BUFLEN-1, stdin);

					// Daca clientul tasteaza "exit" se inchide conexiunea
					if(!strncmp(buffer, EXIT, strlen(EXIT))) {
						close(sockfd);
						exit(0);
					}

					// Se trimite mesajul la server
					n = send(sockfd, buffer, strlen(buffer)+1, 0);
					DIE(n < 0, "send");
                }
                else{
                    ret = recv(i, buffer, BUFLEN, 0);
                    DIE(ret < 0, "Server Error!");

					int max_bytes = ret;
					while(max_bytes < strlen(EXIT)) {
                    	ret = recv(i, buffer+max_bytes, BUFLEN-max_bytes, 0);
                    	max_bytes += ret;
					}

                    // Daca serverul se inchide, trimite mesajul "exit"
					if(!strncmp(buffer, EXIT, strlen(EXIT))) {
						printf("Server closed!\n");
						exit(0);
					}

					while(max_bytes < strlen(CLOSED)) {
                    	ret = recv(i, buffer+max_bytes, BUFLEN-max_bytes, 0);
                    	max_bytes += ret;
					}

                    // Daca serverul refuza conexiunea (ID este deja conectat),
                    // trimite mesajul "closed"
					if(!strncmp(buffer, CLOSED, strlen(CLOSED))) {
						printf("ID already connected!\n");
						return 0;
					}

					while(max_bytes < strlen(INVALID)) {
                    	ret = recv(i, buffer+max_bytes, BUFLEN-max_bytes, 0);
                    	max_bytes += ret;
					}

					// Daca este trimisa o comanda invalida
					if(!strncmp(buffer, INVALID, strlen(INVALID))) {
						printf("Invalid command!\n");
						continue;
					}

					while(max_bytes < strlen(SUBSCRIBED)) {
                    	ret = recv(i, buffer+max_bytes, BUFLEN-max_bytes, 0);
                    	max_bytes += ret;
					}

					if(!strncmp(buffer, SUBSCRIBED, strlen(SUBSCRIBED))) {
						printf("%s\n", buffer);
						continue;
					}

					while(max_bytes < strlen(SIZE)) {
                    	ret = recv(i, buffer+max_bytes, BUFLEN-max_bytes, 0);
                    	max_bytes += ret;
					}

					// Daca este trimis un ID mai mare de 10 caractere
					if(!strncmp(buffer, SIZE, strlen(SIZE))) {
						printf("ID is too long!\n");
						return 0;
					}

					if(!strncmp(buffer, UNSUBSCRIBED, strlen(UNSUBSCRIBED))) {
						printf("%s\n", buffer);
						continue;
					}

					while(max_bytes < strlen(ARGPROBLEM)) {
                    	ret = recv(i, buffer+max_bytes, BUFLEN-max_bytes, 0);
                    	max_bytes += ret;
					}

					// Daca formatul comenzilor subscribe/unsubscribe este gresit
					if(!strncmp(buffer, ARGPROBLEM, strlen(ARGPROBLEM))) {
						printf("A problem has occured!\n");
						continue;
					}

					while(max_bytes < BUFLEN) {
                    	ret = recv(i, buffer+max_bytes, BUFLEN-max_bytes, 0);
                    	max_bytes += ret;
					}

					// S-a primit un mesaj de tip news topic
					// Afisarea adresei IP si a portului in functie de tipul
					// adresei IP: IPv4 sau IPv6
					Topic_packet from_UDP;
					memcpy(&from_UDP, &buffer, sizeof(buffer));

					if(from_UDP.src_addr.sa_family == AF_INET) {
						struct sockaddr_in *sockaddr = (struct sockaddr_in *)&(from_UDP.src_addr);
						printf("%s:%d - ", inet_ntoa(sockaddr->sin_addr), ntohs(sockaddr->sin_port));
					} else if(src_addr.sa_family == AF_INET6) {
						struct sockaddr_in6 *sockaddr = (struct sockaddr_in6 *)&(from_UDP.src_addr);
						char addr_buf[INET6_ADDRSTRLEN];
						inet_ntop(AF_INET6, &(sockaddr->sin6_addr), addr_buf, sizeof(INET6_ADDRSTRLEN));
						printf("%s:%d - ", addr_buf, ntohs(sockaddr->sin6_port));
					}

					char news_buff[MAXBUFLEN];

					Packet news;
					memcpy(&news, &from_UDP.payload, sizeof(from_UDP.payload));

					memcpy(&news_buff, &news.topic, sizeof(news.topic));
					news_buff[sizeof(news.topic)] = '\0';
					printf("%s - ", news_buff);

					switch(news.tip_date) {
						case 0:
							printf("INT - ");
							Integer tmp_int;
							memcpy(&tmp_int, &news.continut, sizeof(tmp_int));
							if(tmp_int.sign == 1)
								printf("-");
							printf("%u\n", ntohl(tmp_int.number));
							break;
						case 1:
							printf("SHORT_REAL - ");
							Real_positive tmp_real_pos;
							memcpy(&tmp_real_pos, &news.continut, sizeof(tmp_real_pos));
							// printf("%.2f\n", ntohs(tmp_real_pos.real)/100.);
							printf("%.2f\n", ntohs(tmp_real_pos.real)/100.);
							break;
						case 2:
							printf("FLOAT - ");
							Real tmp_real;
							memcpy(&tmp_real, &news.continut, sizeof(tmp_real));
							if(tmp_real.sign == 1)
								printf("-");
							printf("%f\n", ntohl(tmp_real.number)/pow(10, tmp_real.power));
							break;
						case 3:
							printf("STRING - ");
							char message[MAXBUFLEN];
							message[MAXBUFLEN-1] = '\0';
							memcpy(&message, &news.continut, sizeof(news.continut));
							printf("%s\n", message);
							break;
					}
                }
            }
        }
	}

	close(sockfd);

	return 0;
}
