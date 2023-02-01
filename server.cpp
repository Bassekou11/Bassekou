
//Server side code

#include<bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////

 
#define BUFF_SIZE 256
#define MAX_CONNECTION 5



queue<int> available_indices ;
pthread_mutex_t global_mutex; /* multiple thread trying to access client_sockfd map at the same time */
map<int, int> client_sockfd; /* sockfd -> index of array - status and client_mutex */
int status[MAX_CONNECTION]; /* -1 if free, sockfd of connected client if busy */
pthread_mutex_t client_mutex[MAX_CONNECTION]; /* multiple client trying to make connection with one particular client */


/* client_mutex[i] will remain locked if i is present in queue available_indices */

void send_connection_status(int src_fd, int dest_fd, bool toStart){
	char buffer[BUFF_SIZE] ;
	int msg_status ;
	
	string action = toStart ? "CONNECTED" : "DISCONNECTED" ;
	string res = "SERVER: " + action + " with client_"  + to_string(dest_fd) + "\n" ;
	strcpy(buffer, res.c_str());

	msg_status = write(src_fd, buffer , BUFF_SIZE); 
	if(msg_status < 0){
	    perror("ERROR: on writing-4\n");
	}
}


void busy_client(int sock_fd){
	int clientID = client_sockfd[sock_fd];

    char buffer[BUFF_SIZE] ;   
    int msg_status ;

	/* server only acts as a middle-node between two clients*/
			
	bzero(buffer, BUFF_SIZE);
	msg_status = read(sock_fd, buffer, BUFF_SIZE);
	if(msg_status < 0){
		perror("ERROR: on reading-7\n");
	}

	
	if(strcmp(buffer, "### dummy message\n") == 0) return;
	
	bool endConnection = false;
	if(strcmp(buffer, "goodbye\n") == 0) endConnection = true;
	
	string res = "CLIENT_" + to_string(sock_fd) + ": " + string(buffer);
	strcpy(buffer, res.c_str());
	msg_status = write(status[clientID], buffer , BUFF_SIZE); 
	if(msg_status < 0){
	    perror("ERROR: on writing-10\n");
	}

	if(endConnection){
		send_connection_status(sock_fd, status[clientID], false);
		send_connection_status(status[clientID], sock_fd, false);
		
		printf("client_%d disconnected with client_%d\n", sock_fd, status[clientID]);
				
		status[client_sockfd[status[clientID]]] = -1;
		status[clientID] = -1;
		return;
	}

}





int free_client(int sock_fd){
	int clientID = client_sockfd[sock_fd];

    char buffer[BUFF_SIZE] ;  
    int msg_status ;

	
	bzero(buffer, BUFF_SIZE);
	msg_status = read(sock_fd, buffer, BUFF_SIZE);


	if(msg_status < 0){
		perror("ERROR: on reading-1\n");
	}


	if(strcmp(buffer, "### dummy message\n") == 0) return 0;
	
            
	if(strcmp(buffer, "close\n" ) == 0) return 1; 
	else if(strcmp(buffer, "get clients\n") == 0)
	{ 
		bzero(buffer, BUFF_SIZE); 
		string res = "SERVER: Connected clients are:\n";

		for(auto it = client_sockfd.begin(); it!=client_sockfd.end(); it++)
		{
			res = res + "        " + "client " +  to_string(it->first)  ;
			if( (it->first) == sock_fd) 
				res = res + "(YOU)" ;
			if(status[it->second] == -1) // checking whether client is free or not!
				res = res + ": FREE\n" ;
			else 
				res = res + ": BUSY\n ";
		}


		strcpy(buffer, res.c_str());
		msg_status = write(sock_fd, buffer, BUFF_SIZE);


		if(msg_status < 0)
		{
	        perror("ERROR: on writing-2\n");
		}	

	}
	else if(strncmp(buffer, "connect ", 8) == 0)
	{
		int to_connect_fd = atoi(buffer + 7);
		if(to_connect_fd == sock_fd)
		{ // if client trying to connect with himself
			msg_status = write(sock_fd, "SERVER: You are trying to connect with yourself. NOT possible\n", BUFF_SIZE);
			if(msg_status < 0)
			{
	        	perror("ERROR: on writing-2\n");
			}
		}

		if(pthread_mutex_trylock(&(client_mutex[clientID])) == 0)
		{	
			int to_connect_ID = -1;

			if(client_sockfd.find(to_connect_fd) != client_sockfd.end())
			{ // finding the client over all the clients that are connected to server
				to_connect_ID = client_sockfd[to_connect_fd] ;
			}
		
			if(to_connect_ID == -1)
			{
				msg_status  = write(sock_fd, "SERVER: The requested client is NOT connected to server\n", BUFF_SIZE);
				if(msg_status < 0){
	        		perror("ERROR: on writing-3\n");
				}
			}

			if(pthread_mutex_trylock(&(client_mutex[to_connect_ID])) == 0)
			{
	    		status[clientID] = to_connect_fd;
				status[to_connect_ID] = sock_fd;
				pthread_mutex_unlock(&(client_mutex[to_connect_ID]));

				/* send message to both clients that connection is established */

				send_connection_status(sock_fd, to_connect_fd, true);
				send_connection_status(to_connect_fd, sock_fd, true);

				printf("client_%d connected with client_%d\n", sock_fd, to_connect_fd);
			}
			pthread_mutex_unlock(&(client_mutex[clientID]));
		}

	}
	else
	{
		msg_status = write(sock_fd, "SERVER: Invalid command!\n", BUFF_SIZE);
		if(msg_status < 0)
		{
	        perror("ERROR: on writing-6\n");
		}
	}
	
	return 0;
}



void *handle_client(void *arg)
{
    int sock_fd = *(int*)arg;
	int clientID = client_sockfd[sock_fd];
	int msg_status;

    printf("\033[1;32mClient with sock_fd: %d connected\033[0m\n", sock_fd);

    while(1)
	{
		if(status[clientID] == -1)
		{

			if( free_client(sock_fd) == 1 ) 
			{
				/*before disconnecting, check some other client has made connection with this client in the meanwhile*/
				pthread_mutex_lock(&(client_mutex[clientID]));
				if(status[clientID] != -1)
				{ // checking if connected with client
					msg_status = write(sock_fd, "SERVER: First close your client connections by goodbye message\n" , BUFF_SIZE); 
					if(msg_status < 0)
					{
	    				perror("ERROR: on writing-11\n");
					}
					pthread_mutex_unlock(&(client_mutex[clientID]));
				}
				else break;
			}
		}
		else
		{
			busy_client(sock_fd) ;
		}
	}
	
	pthread_mutex_lock(&global_mutex);
	client_sockfd.erase(sock_fd); // making the client free
	available_indices.push(clientID) ; // pushing the client to available_indices
	pthread_mutex_unlock(&global_mutex);

    close(sock_fd); // closing the connection with the client
    free((int*)arg); // releasing the memory
    printf("\033[1;31mClient with sock_fd: %d disconnected\033[0m\n", sock_fd);
    
    return 0;
}

void *server_command(void *arg)
{
	string query;

	while(1)
	{
		getline(cin, query);
		if(query== "get clients")
		{
			if(client_sockfd.size() == 0)
			{
				printf(">> no client connected\n");
			}
			else
			{
				printf(">> Connected clients are:\n");
				for(auto it = client_sockfd.begin(); it!= client_sockfd.end(); it++)
				{  /* Iterating over the map to get all the connected clients*/
					printf(">> client_%d ", it->first);
					if(status[it->second] == -1) 
						printf("FREE\n");  /* Checking whether client is free or busy*/
					else 
						printf("BUSY with client_%d\n", status[it->second]);
				}
			}
			
		}
		else if(query == "get free_clients")
		{
			vector<int> free_clients;

			for(auto it = client_sockfd.begin(); it!= client_sockfd.end(); it++)
			{
				if(status[it->second] == -1)
				{
					free_clients.push_back(it->first); /* pushing all the clients that are free in the free_clients vector*/
				} 
			}
			
			if(free_clients.size() == 0)
			{
				printf(">> no client free\n");
			}
			else
			{
				printf(">> Free clients are:\n");
				for(auto x: free_clients)
				{
					printf(">> client_%d\n", x); // printing all the free clients
				} 
			}
		}
		else
		{
			printf(">> Command is not Valid! Try commands :\n>> get clients\n>> get free_clients\n");	
		}
	}
	return 0;
}

int socket_setup(char *arguments[])
{
	int sock_fd, port_no ;
	struct sockaddr_in serv_addr ;
	
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_fd < 0)
	{
		perror("ERROR: on opening socket\n");
		exit(1);
	}
	
	port_no = atoi(arguments[1]);
	bzero((char *) &serv_addr, sizeof(serv_addr));
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port_no);/*The htons() function converts the unsigned short integer 
											hostshort from host byte order to network byte order.*/
	


	/* 
	bind()
       assigns the address specified by addr to the socket referred to
       by the file descriptor sockfd.  addrlen specifies the size, in
       bytes, of the address structure pointed to by addr.
	*/
	if(bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("ERROR: on binding\n");
		exit(1);
	}
	
	listen(sock_fd, 5); /*listen() marks the socket referred to by sockfd as a passive
       socket*/
    printf("\033[1;33mSERVER LISTENING on PORT: %d\033[0m\n\n", port_no);

	return sock_fd ;
}

int main(int argc , char *argv[])
{
	if(argc < 2)
	{
		printf("Enter %s <port_no>\n", argv[0]);
		exit(1);
	}

	int sock_fd = socket_setup(argv);
	
	pthread_t server_query_thread ;
	if( pthread_create(&server_query_thread, NULL, &server_command, NULL) != 0 )
	{
		perror("ERROR: Thread creation for server query failed\n");
		exit(1);
	}

	struct sockaddr_in client_address;
	socklen_t client_address_len = sizeof(client_address);
	
    
	for(int i=0;i<MAX_CONNECTION;i++)
	{
	    available_indices.push(i) ;
		pthread_mutex_init(&(client_mutex[i]), NULL); 
		pthread_mutex_lock(&(client_mutex[i]));
	}
	
	pthread_mutex_init(&global_mutex, NULL); 

	// server running infinitely
    while(1)
	{
        int new_sock_fd = accept(sock_fd, (struct sockaddr *) &client_address, (socklen_t *) &client_address_len);
        if(new_sock_fd < 0)
		{
		    perror("ERROR: on accepting\n");
			continue;
	    }

        int* new_sock_ptr = (int *)malloc(sizeof(int));
        *new_sock_ptr = new_sock_fd;


		if(available_indices.empty())
		{
			write(new_sock_fd, "MAX_CONNECTION limit reached. Try connecting again later.\n" , BUFF_SIZE); 
			close(new_sock_fd);
			continue;
		}

		int next_avl = available_indices.front();

		pthread_mutex_lock(&global_mutex);
		available_indices.pop(); //popping from the available_indices
		client_sockfd[new_sock_fd] = next_avl ;
		status[next_avl] = -1; // making it free
		pthread_mutex_unlock(&global_mutex);

		pthread_mutex_unlock(&(client_mutex[next_avl]));
		
	    pthread_t th ;

        if( pthread_create(&th, NULL, &handle_client, new_sock_ptr) !=0 ) 
		{
			write(new_sock_fd, "Some ERROR occured in server side. Try connecting again later.\n" , BUFF_SIZE); 
			close(new_sock_fd);

			pthread_mutex_lock(&global_mutex);
			client_sockfd.erase(new_sock_fd);
			pthread_mutex_unlock(&global_mutex);

			perror("ERROR: Thread creation failed\n");
        }

    }
	
	close(sock_fd);
	
	return 0;
}

/*

execute below commands to run the program
-  g++ server.cpp -o server -lpthread
-  ./server <port number>
  
*/