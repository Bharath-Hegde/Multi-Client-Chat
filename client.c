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

void *client_subthread(void* param);
int sockval;

int clientMode = 0; // 0 - request mode, 1 - requested, not answered, 2 - connected mode
int acceptStatus; // 0 - requested & waiting for response, 1 - +ve response, 2 - -ve response

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

	// Client
	char myName[name_MAX];
	char otherName[name_MAX];// = "client1"; // Currently hardcoded - type name of other client
	char buff[MAX];
	
	// Input users name
	printf("Enter name: ");
	int n = 0;
	while ((myName[n++] = getchar()) != '\n');
	myName[n-1] = '\0';
	

	// Connect client to server
	if (connect(sockval, (SA*)&servaddr, sizeof(servaddr)) != 0) {
		printf("Could not find the server on %s:%d\n",argv[1], atoi(argv[2]));
		exit(0);
	} else{
		socklen_t len = sizeof(cli);
		if (getsockname(sockval, (struct sockaddr *)&cli, &len) == -1)
			perror("getsockname");
		else
			printf("%s:%d is connected to %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port), argv[1], atoi(argv[2]));
	}

	// Send name to server
	memset(buff, 0, MAX);
	strcpy(buff, "MESG SERVER username:");
	strncat(buff, myName, MAX);
	send(sockval, buff, strlen(buff), 0); 

	pthread_t tid;
	if(pthread_create(&tid, NULL, client_subthread, NULL)){
		perror("1");
		exit(EXIT_FAILURE);
	}

	// User types LIST or CONN <clientid> until connection established
	while(clientMode != 2){
		clientMode = 0;
		memset(buff, 0, MAX);
		printf("Input: ");
		n = 0;
		while ((buff[n++] = getchar()) != '\n');
		buff[n-1] = '\0';
		if(strlen(buff) == 4 && strncmp(buff,"LIST",4) == 0) {
			send(sockval, buff, strlen(buff), 0); 
		} else if(strncmp(buff,"CONN ",5) == 0 ) {
			clientMode = 1;
			send(sockval, buff, strlen(buff), 0); 
			acceptStatus = 0;
			while(!acceptStatus);
			if(acceptStatus == 1){
				// accepted
				strcpy(otherName, buff+5);
				break;
			}
		} else{
			if(clientMode == 2) break;
			printf("Expected:LIST/CONN <clientid>, Got:%s\n",buff);
		} 
	}

	strncat(otherName, " ", name_MAX);
	while(1){
		// Wait for user ip
		char ip_buff[MAX];
		printf("Input: ");
		int n = 0;
		while ((ip_buff[n++] = getchar()) != '\n');
		ip_buff[n-1] = '\0';

		memset(buff, 0, MAX);
		strcpy(buff, "MESG ");
		strncat(buff, otherName, MAX);
		strncat(buff, ip_buff, MAX);
		send(sockval, buff, strlen(buff), 0); 

		if(strcmp(ip_buff, "EXIT") == 0) exit(0);
	}

	pthread_join(tid, NULL);
}

void *client_subthread(void* param){
	char buff[MAX];
	time_t ltime;
	struct tm result;
	char stime[32];

	while(1){
		memset(buff, 0, MAX);
		recv(sockval, buff, MAX, 0);
		/* Received message of four types:
			1) CONN <client i> -> request from client i to connect to this client
			2) CONN <client i>:Y/N -> response of client i to request of this client to connect
			3) LIST <space sep list of users> -> response of server to LIST by this client
			4) MESG <client ic> <message> -> message from connected client (client ic)
		*/

		if(strncmp(buff,"LIST ",5) == 0){
			printf("Server: %s\n",buff);

		} else if(strncmp(buff,"MESG ",5) == 0){
			// get timestamp
			ltime = time(NULL);
			localtime_r(&ltime, &result);
			asctime_r(&result, stime);
			stime[strlen(stime)-1] = '\0';

			// Print the msg in the format <time> <fromClient>: <msg>
			printf("\n%s ",stime);
			int offset = 0, spaceCount = 0;
			while(spaceCount < 2){
				if(buff[offset] == ' ') {
					spaceCount++;
					offset++;
					continue;
				}
				if(spaceCount == 1){
					printf("%c",buff[offset]); // from client
				} 
				offset++;
			} 
			printf(": %s\n", buff+offset);

			if (strcmp(buff+offset, "EXIT") == 0) exit(0);

		} else if(strcmp(buff+strlen(buff)-2,":Y") == 0){
			// go to connection mode
			clientMode = 2;
			acceptStatus = 1;
			printf("Successfull connected. Enter chat mode.\n");

		} else if(strcmp(buff+strlen(buff)-2,":N") == 0){
			// go back to request mode
			clientMode = 0;
			acceptStatus = 2;
			printf("Could not connect.\n");

		} else { // CONN <client i>
			// ungetc('\n', stdin);
			printf("Got: %s\n",buff);
			char temp[name_MAX];
			strcpy(temp,buff+5);
			memset(buff, 0, MAX);
			strcpy(buff,"CONN ");
			strcpy(buff+5,temp);
			if(clientMode != 2){
				// in request mode, send Y
				strncat(buff,":Y",MAX);
				// go to connection mode
				clientMode = 2;
				acceptStatus = 1;
				printf("Successfully connected. Enter chat mode.\n");
			} else {
				// in connection mode, send N
				strncat(buff,":N",MAX);
				printf("Rejected connect request.\n");
			}
			send(sockval, buff, strlen(buff), 0); 
		}

	}
}
