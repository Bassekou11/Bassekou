
////// Client side code


#include<bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
using namespace std;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BUFF_SIZE 256

pthread_mutex_t output_mutex;



void *send_message(void *arg){
    int sock_fd = *(int*)arg;
    char buffer[BUFF_SIZE];
    int msg_status;

    while(1){

    	bzero(buffer, BUFF_SIZE);
    	fgets(buffer, BUFF_SIZE, stdin);

		msg_status = write(sock_fd, buffer, BUFF_SIZE);
		if(msg_status < 0){
			perror("ERROR: on writing\n");
		}

		pthread_mutex_lock(&output_mutex);
		printf("YOU: %s\n", buffer);
		pthread_mutex_unlock(&output_mutex);

	 	if(strcmp(buffer, "close\n") == 0) break;
    }

    return 0;
}

int socket_setup(char *arguments[]){

	int sock_fd, port_no ;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	port_no = atoi(arguments[2]); //taking port number from arguments[2]

	server = gethostbyname(arguments[1]);
				/*	The gethostbyname() function returns a structure of type hostent
					for the given host name.  Here name is either a hostname or an
					IPv4 address in standard dot notation (as for inet_addr(3)). 	*/

	if(server == NULL){
		perror("ERROR: host does not exists\n");
		exit(1);
	}

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	/*	The socket function is used to create a new socket descriptor.
		It takes three arguments: the address family of the socket to be created,
		the type of socket to be created, and the protocol to be used with the socket.
		SOCK_STREAM for TCP.
	*/

	if(sock_fd < 0){
		perror("ERROR: on opening socket\n");
		exit(1);
	}


	bzero((char *) &serv_addr, sizeof(serv_addr));
	/* The bzero() function erases the data in the n bytes of the memory
       starting at the location pointed to by s, by writing zeros (bytes
       containing '\0') to that area.*/


	serv_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(port_no);


	/*
		The connect() system call connects the socket referred to by the
       file descriptor sockfd to the address specified by addr.  The
       addrlen argument specifies the size of addr.
	*/
	if(connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		perror("ERROR: on connection\n");
		exit(1);
	}

	printf("\033[1;31m------------------------------------------------------------------------------------------\033[0m\n\n");

	return sock_fd ;
}

void *recieve_message(void *arg){
    int sock_fd = *(int*)arg;
    char buffer[BUFF_SIZE];
    int msg_status;

    while(1){
    	bzero(buffer, BUFF_SIZE);
		msg_status = read(sock_fd, buffer, BUFF_SIZE);
		if(msg_status < 0){
			perror("ERROR: on reading\n");
		}


		if(strncmp(buffer, "SERVER: DISCONNECTED", 20) == 0) {
		printf("\033[1;31m");
            msg_status = write(sock_fd, "### dummy message\n", BUFF_SIZE);
		    if(msg_status < 0){
			    perror("ERROR: on writing\n");
		    }
        }

		if(strncmp(buffer, "SERVER: CONNECTED", 17) == 0) {
			printf("\033[1;32m");
            msg_status = write(sock_fd, "### dummy message\n", BUFF_SIZE);
		    if(msg_status < 0){
			    perror("ERROR: on writing\n");
		    }
        }

		pthread_mutex_lock(&output_mutex);
		printf("%s\033[0m\n", buffer);
		pthread_mutex_unlock(&output_mutex);

    }

    return 0;
}


int main(int argc , char *argv[]){

	if(argc < 3){
		printf("Enter %s <host_name> <port_no>\n", argv[0]);
		exit(1);
	}

	int sock_fd = socket_setup(argv);

    printf("\033[1;32mConnected to server.\033[0m\n\n" );

	pthread_mutex_init(&output_mutex, NULL); /*The pthread_mutex_init() function initialises the mutex
												referenced by mutex with attributes specified by attr.*/

    pthread_t th_read, th_write;



	 /* The pthread_create() function starts a new thread in the calling
       process.  The new thread starts execution by invoking
       start_routine(); arg is passed as the sole argument of
       start_routine().*/
    if( pthread_create(&th_read, NULL, &recieve_message, &sock_fd) != 0 ){
		perror("ERROR: Thread creation for recieving messages failed\n");
		exit(1);
	}

    if( pthread_create(&th_write, NULL, &send_message, &sock_fd) != 0 ){
		perror("ERROR: Thread creation for sending messages failed\n");
		exit(1);
	}


    /*The pthread_join() function waits for the thread specified by
       thread to terminate.  If that thread has already terminated, then
       pthread_join() returns immediately.  The thread specified by
       thread must be joinable.*/
    if( pthread_join(th_write, NULL) != 0 ){
		perror("ERROR: Thread joining of sending messages failed\n");
		exit(1);
	}

	/*
	thread for sending messages has been joined. It means client wants to end connection.
	So forcefully cancel the thread for recieving messages.
	*/
    if( pthread_cancel(th_read) != 0 ){
		perror("ERROR: Thread cancellation of recieving messages failed\n");
		exit(1);
	}
	printf("\033[1;31mSession Ended.\033[0m\n" );

	close(sock_fd);

	return 0;
}
/*
execute below commands to run the program
-  g++ client.cpp -o client -lpthread
-  tty
-  ./client <host> <port number> <output of tty command>
*/
