#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h> 
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#define MAX 150
#define name_MAX 50
#define clients_MAX 50
#define SA struct sockaddr

void *server_subthread(void* param);

int len, sockval, active;
char clientNames[clients_MAX][name_MAX];
int clientVals[clients_MAX];

typedef struct {
	int clientVal;
	int clientNum;
	char ip_str[INET_ADDRSTRLEN];
} client_details;

int main(int argc, char *argv[]){
	// create socket
	sockval = socket(AF_INET, SOCK_STREAM, 0);
	if (sockval == -1) {
		printf("Socket creation failed\n");
		exit(0);
	} else printf("Socket created\n");

	struct sockaddr_in servaddr, cli;
	memset(&servaddr, 0, sizeof(servaddr));
	// Assign the IP and Port
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(argv[1]);
	servaddr.sin_port = htons(atoi(argv[2]));

	// SERVER

	// Bind given IP to created socket
	if ((bind(sockval, (SA*)&servaddr, sizeof(servaddr))) != 0) {
		printf("Bind failed\n");
		exit(0);
	}
	else printf("Bind success\n");

	// Server is listening
	if ((listen(sockval, 5)) != 0) {
		printf("Listen failed\n");
		exit(0);
	}
	else printf("Server listening...\n");

	pthread_t client_tids[clients_MAX];
	int ind = 0;
	active = 0;
	while(1){
		client_details *myStruct = malloc(sizeof(client_details));
		myStruct->clientNum = ind;
		myStruct->clientVal = accept(sockval, (SA*)&cli, (socklen_t *)&len);

		clientVals[ind] = myStruct->clientVal;

		struct in_addr ipAddr = cli.sin_addr;
		inet_ntop(AF_INET, &ipAddr, myStruct->ip_str, INET_ADDRSTRLEN);

		pthread_t tid;
		if(pthread_create(&tid, NULL, server_subthread, myStruct)){
			perror("1");
			exit(EXIT_FAILURE);
		}
		client_tids[ind] = tid;
		ind++;
		active++;
	}

	close(sockval);
	
	for(int i = 0; i < ind; i++){
		pthread_join(client_tids[i], NULL);
	}
}

void *server_subthread(void* param){
	char buff[MAX];
	char temp_buff[MAX];
	// char clientName[name_MAX];

	client_details* temp = (client_details*)param;
	int clientVal = temp->clientVal;
	int clientNum = temp->clientNum;
	free(temp); // done using the struct

	// Get username of client
	memset(buff, 0, MAX);
	recv(clientVal, buff, MAX, 0);
	strcpy(clientNames[clientNum], buff+21); // MESG SERVER username:<username>


	while(1){
		// listen for client
		memset(buff, 0, MAX);
		recv(clientVal, buff, MAX,0);
		printf("Received msg from %s: %s\n",clientNames[clientNum], buff);

		/* Received message of four types:
			1) CONN <client i> -> request from this client to connect to client i
			2) CONN <client i>:Y/N -> response of this client to a connection request from client i
			3) LIST -> request of this client for list of users
			4) MESG <client i> <message> -> message from this client to send to the connected client
		*/

		if(strncmp(buff,"LIST",4) == 0){
			memset(buff, 0, MAX);
			strcpy(buff,"LIST ");
			for(int i = 0; i < active; i++){
				strncat(buff,clientNames[i],MAX);
				strncat(buff,", ",MAX);
			}
			send(clientVal, buff, strlen(buff), 0); 

		} else if(strncmp(buff,"MESG ",5) == 0){
			// Find otherClients name & message posn, assuming data sent in correct format of MESG <clienti> <message>
			int offset = 0, spaceCount = 0, ind = 0;
			char otherName[name_MAX];
			memset(otherName, 0, name_MAX);
			while(spaceCount < 2){
				if(buff[offset] == ' ') {
					offset++;
					spaceCount++;
					continue;
				}
				if(spaceCount == 1) otherName[ind++] = buff[offset];
				offset++;
			} 
			otherName[ind] = '\0';
			char msg[MAX]; 
			memset(msg, 0, MAX);
			strcpy(msg, buff+offset);

			// format message as clienti: <message>
			char fileBuff[MAX];
			memset(fileBuff, 0, MAX);
			strncat(fileBuff, clientNames[clientNum], MAX);
			strncat(fileBuff, ": ", MAX);
			strncat(fileBuff, msg, MAX);

			// write msg to file
			FILE* fptr = fopen("clientid1_clientid2.txt", "a");
			if(fptr == NULL){
				printf("File opening error 1.");
				exit(0);
			}
			fputs(fileBuff,fptr);
			fputs("\n", fptr);
			fclose(fptr);

			while(active < 2);
			// send msg to the other client
			memset(buff, 0, MAX);
			strcpy(buff, "MESG ");
			strncat(buff, clientNames[clientNum], MAX); 
			strncat(buff, " ", MAX); 
			strncat(buff, msg, MAX); 
			send(clientVals[!clientNum], buff, strlen(buff), 0); // RN !clientNum cuz only 0 and 1
																 // if multiclient - need to figure out clientNum from name??
			// printf("Message sent to %s\n", otherName);
			printf("Message sent to %s\n", clientNames[1-clientNum]);

			if (strcmp(msg, "EXIT") == 0) exit(0);
		} else if(strcmp(buff+strlen(buff)-2,":Y") == 0 || strcmp(buff+strlen(buff)-2,":N") == 0){
			char temp[name_MAX], ind = 0, spacecount = 0, temp_ind = 0;
			while(buff[ind] != ':'){
				if(buff[ind] == ' ') spacecount++;
				else if(spacecount == 1) temp[temp_ind++] = buff[ind];
				ind++;
			}
			// find the sockval of the client i to send the response of clienti's req
			for(int i = 0; i < active; i++){
				if(strcmp(temp,clientNames[i]) == 0){
					send(clientVals[i], buff, strlen(buff), 0); 
					break;
				}
			}
		} else { // CONN <client i>
			char temp[name_MAX];
			strcpy(temp,buff+5);
			memset(buff, 0, MAX);
			strcpy(buff,"CONN ");
			strcpy(buff+5,clientNames[clientNum]);
			// find the sockval of client i to send the CONN req to 
			for(int i = 0; i < active; i++){
				if(strcmp(temp,clientNames[i]) == 0){
					send(clientVals[i], buff, strlen(buff), 0); 
					break;
				}
			}
		}
	}
}
