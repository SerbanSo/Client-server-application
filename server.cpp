#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <unordered_map>
#include <queue>
#include <set>
#include <iostream>

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int newsockfd, portno;
	int sockTCP, sockUDP;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;
	struct sockaddr src_addr;
	socklen_t addrlen;

	addrlen = sizeof(src_addr);

	fd_set read_fds;	// Multimea de citire folosita in select()
	fd_set tmp_fds;		// Multime folosita temporar
	int fdmax;			// Valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// Se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Se deschide socket pentru ascultarea pachetelor UDP
	sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sockUDP < 0, "socket UDP");

	// Se deschide socket pentru ascultarea pachetelor TCP
	sockTCP = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockTCP < 0, "socket TCP");

	// Get port number
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	// Set server address
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// Asociaza socketul UDP cu <port, adresa_IP>:
	ret = bind(sockUDP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind UDP");

	// Asociaza socketul TCP cu <port, adresa_IP>:
	ret = bind(sockTCP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind TCP");

	// Asculta la portul TCP pentru clienti
	// ret = listen(sockfd, MAX_CLIENTS);
	ret = listen(sockTCP, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// Se adauga socketul pe care se asculta datagrame in multimea read_fds
	FD_SET(sockUDP, &read_fds);

	// Se adauga socketul pe care se asculta conexiuni in multimea read_fds
	FD_SET(sockTCP, &read_fds);

	// Se adauga descriptorul pentru input de la tastatura in multimea read_fds
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = sockTCP;

	// Hash_map in care se tine minte care ID-uri sunt conectate
	// Key = ID_CLIENT; Value = 0 (deconectat), sockfd (conectat)
	unordered_map<string, int> TCPconnections;

	// Hash_map in care se tine minte care socket ii corespunde carui ID
	// Key = sockfd; Value = ID_CLIENT
	unordered_map<int, string> sockID;

	// Hash_map in care se tin minte subscriberii unui topic
	// Key = topic; Value = set<int> ce contine toti sockfd abonati
	unordered_map<string, set<string>> topic_subs;

	// Hash_map in care se tine minte coada cu mesaje pentru fiecare
	// subscriber
	// Key = ID_CLIENT; Value = Coada de Topic_packet
	unordered_map<string, queue<Topic_packet>> clients_queue;

	// Hash_map in care se tine minte pentru fiecare subscriber pentru fiecare
	// topic daca vrea sa primeasca mesajele care vin cand e deconectat sau nu
	// Key = ID_CLIENT; Value = unordered_map
	// Key = topic; Value = 0/1
	unordered_map<string, unordered_map<string, int>> store_and_forward;

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockTCP) {
					// A venit o cerere de conexiune pe socketul inactiv (cel cu listen)
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockTCP, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					ret = recv(newsockfd, buffer, BUFLEN, 0);
					DIE(ret < 0, "TCP recv");

					// Se copiaza ID-ul clientului intr-un buffer local
					char id[IDLEN];
					id[IDLEN-1] = '\0';
					memcpy(&id, buffer, strlen(buffer)+1);

					// Se verifica daca ID-ul are mai mult decat limita de 10 caractere
					if(strlen(buffer) > 10) {
						memcpy(buffer, SIZE, sizeof(SIZE));
						send(newsockfd, buffer, strlen(buffer)+1, 0);
						continue;
					}

					// Se verifica daca ID-ul este conectat deja pe server
					if(TCPconnections[id]) {
						printf("ID %s tried to connect, but it's already connected!\n", id);
						memcpy(buffer, CLOSED, sizeof(CLOSED));
						send(newsockfd, buffer, strlen(buffer)+1, 0);
						continue;
					}

					// Seteaza TCP_NODELAY pentru socket-ul accpetat
					int nodelay = 1;
					ret = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(int));
					DIE(ret < 0, "setsockopt");

					// Se tine minte la ce socket e conectat ID-ul
					TCPconnections[id] = newsockfd;

					// Se tine minte ce ID e conectat la socket
					sockID[newsockfd] = id;

					// Daca sunt mesaje primite in coada, aceastea se trimit catre client
					while(!clients_queue[id].empty()) {
						Topic_packet tmp_pack = clients_queue[id].front();
						clients_queue[id].pop();

						send(newsockfd, &tmp_pack, sizeof(tmp_pack), 0);
					}

					// Se adauga noul socket la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}
					
					// Se afiseaza feedback
					printf("New client (%s) connected from %s:%d.\n", id, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
				} else if(i == sockUDP) {
					Packet pachetUDP;
					Topic_packet to_subscribers;

					memset(&pachetUDP, 0, sizeof(pachetUDP));
					memset(&to_subscribers, 0, sizeof(to_subscribers));

					// Se primeste pachetul de tip UDP
					ret = recvfrom(sockUDP, &pachetUDP, sizeof(Packet), 0, &src_addr, &addrlen);

					char topic_buf[TOPICLEN];
					topic_buf[TOPICLEN-1] = '\0';

					// Se copiaza in buffer-ul local topicul din pachet
					memcpy(topic_buf, &pachetUDP.topic, sizeof(pachetUDP.topic));

					// Se copiaza informatiile necesare pentru a le trimite la clientii abonati
					memcpy(&to_subscribers.payload, &pachetUDP, sizeof(pachetUDP));
					memcpy(&to_subscribers.src_addr, &src_addr, sizeof(src_addr));
					memcpy(&to_subscribers.addrlen, &addrlen, sizeof(addrlen));

					// Pentru fiecare client abonat la topic, se trimite mesajul
					// Daca clientul nu este conectat, si are setat S&F la 1,
					// mesajul se pune in coada
					for(auto x : topic_subs[topic_buf]) {
						if(TCPconnections[x] != 0)
							send(TCPconnections[x], &to_subscribers, sizeof(to_subscribers), 0);
						else if(store_and_forward[x][topic_buf] == 1)
							clients_queue[x].push(to_subscribers);
					}
				} else if(i == STDIN_FILENO){
					// Se citeste mesajul de la tastatura
					fgets(buffer, BUFLEN-1, stdin);

					// Se verifica daca mesajul primit este "exit"
					if(!strncmp(buffer, "exit", 4)) {
						// Deconectarea tuturor clientilor TCP conectati
						printf("Server closed!\n");

						// Se trimite mesaj catre clienti ca serveul se inchide,
						// se inchid conexiunile cu clientii, si in final se 
						// inchide serverul
						for(auto x : TCPconnections) {
							if(x.second) {
								send(x.second, EXIT, sizeof(EXIT), 0);

								close(x.second);
							}
						}

						close(sockUDP);
						close(sockTCP);

						return 0;
					}
                } else {
					// S-au primit date pe unul din socketii de clienti
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);

					if (n <= 0) {
						// Clientul s-a deconectat
						cout << "Client (" << sockID[i] << ") disconnected.\n";
						TCPconnections[sockID[i]] = 0;
						sockID[i] = "";
						close(i);
						
						// Se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					} else {
						char *tmp_buf = strtok(buffer, " ");

						// Se verifica daca primul cuvant primit e "subscribe"/"unsubscribe"
						if(!strcmp(tmp_buf, "subscribe")) {
							tmp_buf = strtok(NULL, " ");

							// Daca e doar un cuvant se trimite mesaj la client
							if(tmp_buf == NULL) {
								send(i, ARGPROBLEM, strlen(ARGPROBLEM)+1, 0);
								continue;
							}

							char tmp_topic[TOPICLEN];
							tmp_topic[TOPICLEN-1] = '\0';

							// Se scrie in buffer-ul local topicul la care se doreste
							// sa se dea subscribe
							memcpy(&tmp_topic, tmp_buf, strlen(tmp_buf)+1);

							// Se adauga in arborele de topic, ID clientului care
							// tocmai a dat subscribe
							topic_subs[tmp_topic].insert(sockID[i]);

							memcpy(&tmp_topic, tmp_buf, strlen(tmp_buf)+1);
							tmp_buf = strtok(NULL, " ");

							// Se verifica daca exista un al treilea parametrul la comanda
							// de subscribe, daca nu exista sau daca acesta este diferit 
							// de 0 sau 1, se scoate ID-ul clientului din arborele de topic
							// si se trimite mesaj catre client
							if(tmp_buf == NULL || (tmp_buf[0] != '0' && tmp_buf[0] != '1')) {
								topic_subs[tmp_topic].erase(sockID[i]);
								send(i, ARGPROBLEM, strlen(ARGPROBLEM)+1, 0);
								continue;
							}

							cout << "Client (" << sockID[i] << ") subscribed to " << tmp_topic << "\n";

							// Se tine minte daca abonarea e de tip S&F = 0 sau S&F = 1
							store_and_forward[sockID[i]][tmp_topic] = tmp_buf[0] - '0';

							// Se trimite mesaj cu confirmarea abonarii catre client
							sprintf(buffer, "subscribed %s", tmp_topic);
							send(i, buffer, strlen(buffer)+1, 0);
						} else if(!strcmp(tmp_buf, "unsubscribe")) {
							tmp_buf = strtok(NULL, " ");

							// Daca e doar un cuvant se trimite mesaj la client
							if(tmp_buf == NULL) {
								send(i, ARGPROBLEM, strlen(ARGPROBLEM)+1, 0);
								continue;
							}

							char tmp_topic[TOPICLEN];
							tmp_topic[TOPICLEN-1] = '\0';

							// Se scrie un buffer-ul local topicul la care se doreste
							// sa se dea unsubscribe
							memcpy(&tmp_topic, tmp_buf, strlen(tmp_buf)+1);

							// Se suprascrie '\n' cu '\0'
							tmp_topic[strlen(tmp_buf)] = '\0';
							// tmp_topic[strlen(tmp_buf)-1] = '\0';
							tmp_topic[strlen(tmp_topic)] = '\0';
							tmp_topic[strlen(tmp_topic)-1] = '\0';

							// Se sterge ID-ul clientului din arborele topicului
							topic_subs[tmp_topic].erase(sockID[i]);
							cout << "ID " << sockID[i] << " unsubscribed from " << tmp_topic << "\n";
							
							// Se trimite mesaj cu confirmarea dezabonarii catre client
							sprintf(buffer, "unsubscribed %s", tmp_topic);
							send(i, buffer, strlen(buffer)+1, 0);
						} else {
							// Daca este alta comanda inafara de subscribe/unsubscribe
							// se trimite mesaj inapoi semnificat "invalid command"
							send(i, INVALID, strlen(INVALID)+1, 0);
							continue;
						}
					}
				}
			}
		}
	}

	close(sockUDP);
	close(sockTCP);

	return 0;
}
