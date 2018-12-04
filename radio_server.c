#include <stdio.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>



#define SERVER_SEND_SONGS_RATE 64000
#define SERVER_SEND_SONGS_BUFFER 1024
#define HELLO_SIZE		3
#define WELCOME_SIZE 	9
#define ANNOUNCE_SIZE 	258
#define PERMIT_SIZE 	2
#define ASK_SIZE 		3
#define UPSONG_SIZE 	262
#define MAX_PROTCOLPACKET 262
#define INVALID_SIZE 	258
#define NEW_STATION_SIZE 3

#define Hello	 0
#define AskSong	 1
#define UpSong	 2
#define BUF_SIZE 1024
#define DEBUG 1
typedef unsigned char unit8_t;
typedef unsigned short unit16_t;
typedef unsigned int unit32_t;




typedef struct Station{

	unsigned char stationNum;
	int UDPsocketID; 
	struct sockaddr_in stationAddr; 
	unsigned char ipStation[4]; 
	char songName[256]; 
	unsigned char songLenName;
	unsigned char ipAsString[20]; 
}Station;




int serverNumPort;						//port number of server
int wellcomeTCPSocketId;				//welcome socket id
struct sockaddr_in addrWelcomeTCP;		//welcome socket address-struct

unsigned int ResiveSongSize=0;
unsigned char ResivesongNamesize;
unsigned char ResivesongName[256];

char numStations=-1;						//number of station in the radio
unsigned short portNumMulticast; 		// multicast port 
unsigned  char multicastarr [4]; 			//chars array  of multicast ip

int clientTCPSocket[100]={-1}; 
struct sockaddr_in addrClientTCP[100];	//socket address struct of clients
pthread_t clientThreadsId[100]; 		//threads of clients
int numClients=-1;

int helloFlags[100]={1};				//on if expecting to hello
int upSongFlags[100]={0};				//on if up song message came\ on a sending mode
int selectFlag[100]={0};
fd_set readClientfds[100];
int clientIndex=-1;

int sending=-1;
int thResopen=0;
pthread_t thRES;

struct timeval *ptv=NULL;
struct timeval *ptvClient[100]={NULL};
struct timeval tv; 
fd_set readfds; 
int exitProgramFlag=0;
Station **pStations;
pthread_t** threadId;
pthread_t* UdpTreads;
int mulicastIP=0;
int addNewSationFlag=0;
Station newStationTemp;
int selectIdMax=0;

//FUNCTIONS
int welcome();		
int EventFormUser();	
int ExitProgram();			
void *clientHandler(void *id);
int InvalidHandler(int clientID, int messageID);
int closeClient(int clientID);							
int WelcomeSender(int clientID);						
int AnnounceSender(int clientID,int Station);			
int PermitSongSender(int clientID, unsigned char *upSongBuffer);  
int newStationSender();
unsigned int user_string(char *str, int len);
int ReciveSong(int clientID, int* dontTouchme); 
int *UDPsender(int index);
void printStatus();

int freeMemory()
{
	int i=0;
	for(i=0; i<=numStations; i++)
	{
			free(pStations[i]);
			free(threadId[i]);
	}
	free(pStations);
	free(threadId);
	return 1;
}

int main(int argc, char **argv)
{
	FILE *fp;
	int i=0;
	
	if (argc <4) {
		puts("RADIO_SERVER: radio_server <tcpport> <mulitcastip> <udpport> <file1 > <file2 > ...");
		return 1;
	}

	serverNumPort=atoi(argv[1]);
	if(serverNumPort<1000)    //port is out of range(<10000)
	{
		puts("TCP PORT is out of range");
		puts("RADIO_SERVER: radio_server <tcpport> <mulitcastip> <udpport> <file1 > <file2 > ..."); //make the user know that he should enter the right bash command
		return 1;
	}

	mulicastIP= inet_addr(argv[2]);  //recive the multicast address
	if(mulicastIP==-1){  

			puts("Muliticast ip isn't right");
			puts("RADIO_SERVER: radio_server <tcpport> <mulitcastip> <udpport> <file1 > <file2 > ...");
		return 1;
		}
	multicastarr[0]=mulicastIP;
	multicastarr[1]=mulicastIP/256;
	multicastarr[2]=mulicastIP/65536; //256*265
	multicastarr[3]=mulicastIP/16777216; //256^3
	
	portNumMulticast=atoi(argv[3]); //udp port from the command
	if(portNumMulticast<1000)
	{
		puts("UDP PORT is  out of range");
		puts("RADIO_SERVER: radio_server <tcpport> <mulitcastip> <udpport> <file1 > <file2 > ...");
		return 1;
		
	}
	
	for(i=4;i<argc;i++)
	{
		fp=fopen(argv[i],"r");
		if(fp==NULL){
			puts("Error in opening the file");
			puts("RADIO_SERVER: radio_server <tcpport> <mulitcastip> <udpport> <file1 > <file2 > ...");
		    return 1;
		}

		fclose(fp);
	}
	

	numStations=argc-5;  
	pStations=( Station **)calloc(numStations+1, sizeof( Station *));
	threadId=( pthread_t **)calloc(numStations+1, sizeof( pthread_t* ));
	for(i=0; i<=numStations; i++)
	{
		pStations[i]=( Station *)calloc(1, sizeof( Station ));
		threadId[i]=( pthread_t *)calloc(1, sizeof( pthread_t));
		strcpy(pStations[i]->songName,argv[4+i]);

		pStations[i]->stationNum=i;

		pStations[i]->songLenName=strlen(pStations[i]->songName);
		if(DEBUG) printf("%d:%s The length of the song name is %d\n",i,pStations[i]->songName,pStations[i]->songLenName );
		pthread_create(threadId[i], 0, UDPsender, (pStations[i]->stationNum));
	}


	newStationTemp.stationNum=255;
	strcpy(newStationTemp.songName,"Default station");
	newStationTemp.songLenName=0;
	for(i=0;i<100;i++){
		clientTCPSocket[i]=-1;
	}
	
	puts("\n\n\n\n\n\n\n\n\n");
	printStatus();
	welcome();
	for(i=0; i<=numStations; i++)
	{
			free(pStations[i]);
			free(threadId[i]);
	}
	free(pStations);
	free(threadId);
}

unsigned int user_string(char *str, int len)

{

	len=len+2; // this would help us to see if the user to exceeds the max length

	int length=0;

	do{

		fgets(str,len,stdin); 				

		str[strcspn(str, "\r\n")] = '\0';	//Change the last char from \n to 0

		length=strlen(str); 						

		if(length>=len-1) 							

			printf("The string is out of range");

		if(length==len-1) while(getchar()!='\n'); 	//clean  buffer 

	}while(length>=len-1);							//if need try again

	return strlen(str);

}

void addNewStationToDataBase()
{
	printf("\nThe file name is:%s with length %d, its number of stations is: %d \n\n\n ",newStationTemp.songName,newStationTemp.songLenName,numStations);

	addNewSationFlag=0;
	pStations=( Station **)realloc(pStations,numStations+1);
	threadId=( pthread_t **)realloc(threadId,numStations+1);

	pStations[numStations]=( Station *)calloc(1, sizeof( Station )); //pointers to the stations
	threadId[numStations]=( pthread_t *)calloc(1, sizeof( pthread_t)); //create thread for each station

	pStations[numStations]->songLenName=newStationTemp.songLenName; 
	strcpy(pStations[numStations]->songName,newStationTemp.songName);
	pStations[numStations]->stationNum=numStations;
	if(DEBUG) printf("\n%d:%s The  length is %d\n",numStations,pStations[numStations]->songName,pStations[numStations]->songLenName );
	sleep(1);

	if(pthread_create((threadId[numStations]), 0, UDPsender, pStations[numStations]->stationNum)!=0)
			puts("\nError in creating thread");
}


int welcome()
{

	int errorbit=0, Event=0;
	int temp=0,i=0, j=0;
	unsigned char buffer[256];			//buffer for the welcome message


	socklen_t clientAddrSize;


	//create a socket
	wellcomeTCPSocketId = socket(AF_INET, SOCK_STREAM, 0);
	if(wellcomeTCPSocketId==-1){
		perror("\n\nRADIO_SERVER: Cannot open Socket, The program will terminate\n\n");
		ExitProgram();
	}

	
	memset (& addrWelcomeTCP, 0, sizeof(addrWelcomeTCP)); // reset with 0 
	addrWelcomeTCP.sin_family = AF_INET;  // Address family =   IPV4
	addrWelcomeTCP.sin_port = htons(serverNumPort); // Set port number
	addrWelcomeTCP.sin_addr.s_addr = htonl(INADDR_ANY);;// Set IP address of the TCP receiver

	if( bind(wellcomeTCPSocketId, &addrWelcomeTCP,sizeof (struct sockaddr_in))==-1){
		perror("\n\nCouldnt bind the socket with the struct\n\n");
		return 1;
	  }

	if(listen(wellcomeTCPSocketId,5)==-1){
		perror("n\nCouldn't listen, The program will terminate\n\n");
		return 1;
	 }
	
	selectIdMax=(((fileno(stdin))>(wellcomeTCPSocketId))?(fileno(stdin)):(wellcomeTCPSocketId)); //select maximum id
	addNewSationFlag=0;
	while(1)
	{

		FD_ZERO (&readfds);

		if(addNewSationFlag){
			FD_CLR(fileno(stdin),&readfds);


			addNewStationToDataBase();


			addNewSationFlag=0;
			
		}

		FD_SET (fileno(stdin),&readfds);
		FD_SET (wellcomeTCPSocketId,&readfds);


			Event=select(selectIdMax+1,&readfds, NULL,NULL,ptv);


			//STDIN File Descriptor check
			if(FD_ISSET(fileno(stdin),&readfds))        
			 {
				//Handle user inputs here
				printf("\nStatus: User input event accured\n");
				EventFormUser();
				FD_CLR(fileno(stdin),&readfds);
				
			 }
			if(FD_ISSET(wellcomeTCPSocketId,&readfds))
        	{
             
				for(i=0;i<100;i++){
	
				if(clientTCPSocket[i]==-1){
					clientIndex=i;
			}
		
	}
			
				printf("\nClient ID is: %d",clientIndex);

				if(clientIndex==-1)	{
					printf("\n RADIO_SERVER: The server is overflowed\n");

				}
				clientAddrSize=sizeof (struct sockaddr_in);
				clientTCPSocket[clientIndex]=accept(wellcomeTCPSocketId, &(addrClientTCP[clientIndex]), &clientAddrSize);
				if(clientTCPSocket[clientIndex]==-1){
					perror("\n\nRADIO_SERVER: could not accept call, this program ends\n\n");
					
					FD_CLR(wellcomeTCPSocketId,&readfds);
					continue;
				}

				printf("\n welcome() socket number: %d\n",clientTCPSocket[clientIndex]);
				temp=recv(clientTCPSocket[clientIndex], buffer, MAX_PROTCOLPACKET, 0);
				if(temp==-1){
					printf("\n\nFailed to receive data from client: %d, That client was removed\n\n",clientIndex);
					
					temp=close(clientTCPSocket[clientIndex]);
					if(temp==-1){
							printf("\n\n The TCP SOCKET on client %d wasn't closed\n\n",clientIndex); 
					}
					clientTCPSocket[clientIndex]=-1;
					FD_CLR(wellcomeTCPSocketId,&readfds);
					continue;

				}

				puts("\nAfter received Hello message");
				if(buffer[0]!=0)
				{
					printf("\nDidnt get Hello message ");
					InvalidHandler(clientIndex,buffer[0]);
					temp=close(clientTCPSocket[clientIndex]);
					if(temp==-1){
						printf("\n\n The TCP SOCKET of client %d wasn't closed\n\n",clientIndex); 
					}
					clientTCPSocket[clientIndex]=-1;
				}
				else
				{
					WelcomeSender(clientIndex);
				}
				printf("\nWill open socket for client %d\n",clientIndex);

				if(pthread_create(&clientThreadsId[clientIndex], 0, clientHandler, (void *)&clientIndex)!=0)
					puts("\nERROR:thread wasn't created");


				printf("\nThread was created for the client%d\n",clientIndex);
				numClients++;

       		   FD_CLR(wellcomeTCPSocketId,&readfds);

            }

	}
}

void *clientHandler(void *id)
{
	int clientID=0, Event=-2;
	int temp=0, tempStation=-1;
	unsigned char buffer[MAX_PROTCOLPACKET]={0};
	int messageID=-1;
	int dontTouchme=0; 

	clientID=*(int*)id;
	clientIndex=-1;
	printf("\nClient handler is working on client : %d\n",clientID);

	printf("\nThe client socket is: %d\n",clientTCPSocket[clientID]);


	while(1)
	{
		if(dontTouchme)continue; 
		if(sending==clientID){

			FD_CLR(clientTCPSocket[clientID],&readClientfds[clientID]); 
			FD_ZERO (&readClientfds[clientID]);
			dontTouchme=1; 
			ReciveSong(clientID, &dontTouchme); 
			printf("\n Recive song number %d\n",addNewSationFlag);
			
		}
		while(addNewSationFlag);
		if(sending!=clientID)
		{

			FD_ZERO (&readClientfds[clientID]);
			FD_SET (clientTCPSocket[clientID],&readClientfds[clientID]);
			Event=-2; 
			do{ 
				Event=select(clientTCPSocket[clientID]+1,&readClientfds[clientID], NULL,NULL,NULL);
			}while(Event==-2); 

			if(FD_ISSET(clientTCPSocket[clientID],&readClientfds[clientID]))
			{
				//puts("After select-CLIENT HANDLER");

				temp=recv(clientTCPSocket[clientID], buffer, MAX_PROTCOLPACKET, 0);
				if(temp==-1){
					printf("\n\nFailed to receive DATA from client: %d, this client was removed\n\n",clientID);// coudnt read data from server
					InvalidHandler(clientID,messageID);
					closeClient(clientID);
				}
				if(temp==0)
				{
					closeClient(clientID);
				}
				//puts("After recv-CLIENT HANDLER");
				messageID=buffer[0];
				printf("\nThe massage id is: %d",messageID);
				switch(messageID){
				case (Hello):
						{
						puts("Hello");
							if(!helloFlags[clientID]){
									printf("\nUnexpected Hello message from client %d",clientID);
									InvalidHandler(clientID,messageID);
									closeClient(clientID);
								}
							helloFlags[clientID]=0;
							WelcomeSender(clientID);
							break;
						}


				case (AskSong):
						{
						puts("\nASKSONG");
							tempStation=buffer[1]*256+buffer[2];
							printf("\nThe wanted station is %d",tempStation);
							if(tempStation>numStations)
							{
								printf("\n RADIO_SERVER: Station exception, %d client removed\n",clientID);
								InvalidHandler(clientID,messageID);
								closeClient(clientID);
							}
							else
							{
								AnnounceSender(clientID,tempStation);
							}
							break;
						}

				case (UpSong):
						{
							printf("\nStatus: UpSong message\n");
							PermitSongSender(clientID,buffer);
							break;
						}

				 default: 
						 {
							printf("\n RADIO_SERVER: Invalid massage from the client %d",clientID);
							InvalidHandler(clientID,messageID);
							closeClient(clientID);
							break;
						 }

				}

		
			FD_CLR(clientTCPSocket[clientID],&readClientfds[clientID]);
			}
		}
	}


}


int EventFormUser()
{
	char str[300];
	int len=300 , strlen=0,i =0;
	strlen= user_string(str,len); //get input from user

	if(strcmp(str,"q") == 0)
	{
		printf("\nRADIO_SERVER: Goodbye!!!\n\n");
		exit(1);
	}
	//~
	else if(strcmp(str,"p") == 0)
	{
			printStatus();
	}
//~
	else{
			printf("\nBad command:Please enter p for data base and q for Quit\n");
			return 1;//other?
		}
	return 1;
}


 int WelcomeSender(int clientID)
{
	int temp=0;
	char buffer[WELCOME_SIZE];
	buffer[0]=0;				//welcome type
	buffer[1]=(char)numStations/256;	//num station high
	buffer[2]=(char)numStations%256+1;	//num station low
	buffer[6]=multicastarr[0];		//multicast
	buffer[5]=multicastarr[1];		//multicast
	buffer[4]=multicastarr[2];		//multicast
	buffer[3]=multicastarr[3];	//multicast
	buffer[7]=portNumMulticast/256;	//multicast
	buffer[8]=portNumMulticast%256;	//multicast

	temp=send(clientTCPSocket[clientID],buffer,WELCOME_SIZE,0);
	if(temp==-1)
	{
		perror("\n RADIO_SERVER: Couldn't send Welcome message");
			closeClient(clientID);
	}
	puts("\Hello was sent");
	helloFlags[clientID]=0;
	return 1;
}

int AnnounceSender(int clientID,int Station)
{
	int temp=0;
	unsigned char buffer[260];

	memset (& buffer, 0, sizeof(buffer));
	buffer[0]=1;							//ANNOUNCE type
	buffer[1]=pStations[Station]->songLenName;	//num station high

	printf("\nAnnounce: %s with len %d\n",pStations[Station]->songName, pStations[Station]->songLenName);

	strcpy(&buffer[2],pStations[Station]->songName);
	temp=sendto(clientTCPSocket[clientID],buffer,2+pStations[Station]->songLenName,0,(struct sockaddr *)&clientTCPSocket[clientID],sizeof(clientTCPSocket[clientID]));

	printf("\n IN Announce: the song name we send to client is: %s",&buffer[2]);
	if(temp==-1){
		perror("\n RADIO_SERVER: could not send Welcome message");
			closeClient(clientID);
	}
	puts("\nANOUNCE SENDER done");
		//free(buffer);
	return 1;
}

int PermitSongSender(int clientID, unsigned char upSongBuffer[MAX_PROTCOLPACKET])
{
	unsigned char buffer [2];
	unsigned char permit=1;
	int temp=0;
	int i;
	buffer[0]=2;		//permit type
	puts("\nPermit Sender\n");
	if(sending==-1)
	{
		ResiveSongSize=0;
		ResivesongNamesize=0;
		memset (& ResivesongName, 0, sizeof(ResivesongName));
		sending=clientID;
		ResiveSongSize=0;
		ResiveSongSize=upSongBuffer[1]*16777216;
		ResiveSongSize+=upSongBuffer[2]*65536;
		ResiveSongSize+=upSongBuffer[3]*256;
		ResiveSongSize+=upSongBuffer[4];
		ResivesongNamesize=upSongBuffer[5];
		
		for(i=0;i<ResivesongNamesize;i++)
		{
			ResivesongName[i]=upSongBuffer[6+i];
		}
		ResivesongName[i]='\0';
		
		puts("PERMIT =1\n");
		for(i=0;i<=numStations;i++)
		{
			puts("PERMIT SENDER:Songs names compare");
			printf("\nResivesongName:%s  and pStations[i]->songName:%s",ResivesongName, pStations[i]->songName);
			if(strcmp(ResivesongName,pStations[i]->songName)==0) //if equal
			{
				ResiveSongSize=0;
				ResivesongNamesize=0;
				memset (& ResivesongName, 0, sizeof(ResivesongName));
				sending=-1;
				permit=0;
				puts("PERMIT =0\n");
				break;
			}
		}

	}
	else
	{
		permit=0;
	}
	buffer[1]=permit;

	puts("Sending permit massage");
	temp=sendto(clientTCPSocket[clientID],buffer,PERMIT_SIZE,0,(struct sockaddr *)&clientTCPSocket[clientID],sizeof(clientTCPSocket[clientID]));
		if(temp==-1)
		{
			perror("\n RADIO_SERVER: could not send Welcome message");
			closeClient(clientID);
		}

	return 1;
}


int ExitProgram()
{
	int i=0, error=0;
	for(i=0;i<numClients;i++)
	{
		error=close(clientTCPSocket[i]);
			if(error==-1)
			{
				printf("\n\n the TCP SOCKET on client %d wasn't closed\n\n",i);
			}
	}
	pthread_exit(1);
	return 1;
}

int closeClient(int clientID)
{
	int error=0;
	error=close(clientTCPSocket[clientID]);
			if(error==-1)
			{
				printf("\n\n the TCP SOCKET on client %d wasn't closed\n\n",clientID);
			}
	clientTCPSocket[clientID]=-1;
	numClients--;
	puts("\nClient left!");
	pthread_exit(1);
	return 1;
}

int InvalidHandler(int clientID, int messageID)
{
	int temp=0;
	unsigned char buffer[INVALID_SIZE];
	char* massge1="Unexpected hello message";
	char* massge2="Undefined Station";
	char* massge3="could not receive data";
	char* massged="Undefined Command";
	memset (& buffer, 0, sizeof(buffer));
	buffer[0]=3;					//INVALID type
	switch(messageID){
		case (Hello):
				{
					
					stpcpy(&buffer[2],massge1);
					puts("INVALID: Unexpected Hello message");
					buffer[1]=strlen(massge1);
					break;
				}
		case (AskSong):
				{
				
					stpcpy(&buffer[2],massge2);
					puts("INVALID: Undefined Station");
					buffer[1]=strlen(massge2);
					break;
				}

		case (UpSong):
				{
				
				stpcpy(&buffer[2],massge3);
				puts("INVALID: Could not receive data");
				buffer[1]=strlen(massge3);
					break;
				}

		 default: //notexpected
				 {
					
					stpcpy(&buffer[2],massged);
					puts("INVALID: Undefined Command");
					buffer[1]=strlen(massged);
					break;
				 }
	}
	


	if(clientTCPSocket[clientID]!=-1){
	temp=send(clientTCPSocket[clientID],buffer,2+(int)buffer[1],0);
		if(temp==-1)
		{
			perror("\n RADIO_SERVER: could not send Welcome messege");
			closeClient(clientID);
		}
	}
	return 1;
}


int newStationSender()
{
	int temp=0,i;
	char buffer[NEW_STATION_SIZE]={0};
	
	buffer[0]=4;							
	buffer[1]=numStations/256;					
	buffer[2]=numStations%256+1;

	for(i=0;i<100;i++)
	{
		if(clientTCPSocket[i]!=-1)
		{
			temp=send(clientTCPSocket[i],buffer,NEW_STATION_SIZE,0);
			if(temp==-1)
			{
				perror("\n RADIO_SERVER: couldn't send Welcome message");
				closeClient(i);
			}
		}
	}
	return 1;

}


int *UDPsender(int index){

	int packetTTL=60;
	//int index=5;
	int temp=0;
	int i=0;
	struct Station* current;
	int errorbit=0,error;
	unsigned int fileLen=0;
	unsigned long countBytesSent=0, bufferSize=SERVER_SEND_SONGS_BUFFER,x;
	size_t result;
	unsigned char* buffer;
	unsigned char bufferStack[1024];
	FILE* fp;

	struct sockaddr_in multicastAddr;
	int multicastSocket;

	current=pStations[index];
	current->ipStation[0]=multicastarr[0];
	current->ipStation[1]=multicastarr[1];
	current->ipStation[2]=multicastarr[2];
	current->ipStation[3]=multicastarr[3]+current->stationNum;

	sprintf(current->ipAsString,"%d.%d.%d.%d \n", current->ipStation[0],current->ipStation[1],current->ipStation[2],current->ipStation[3]);
	
	current->UDPsocketID = socket(AF_INET, SOCK_DGRAM, 0);//create a UDP multi cast socket
	if(current->UDPsocketID==-1){
		perror("Cannot open socket");
		return 1;
	}

	memset (& current->stationAddr, 0, sizeof( current->stationAddr)); // reset with 0 all the  sockaddr_in multicastAddr struct we going to use
	(current->stationAddr).sin_family = AF_INET;  // Address family =   IPV4
	(current->stationAddr).sin_port = htons(portNumMulticast); // Set port number, using htons function to use proper byte order
	(current->stationAddr).sin_addr.s_addr = inet_addr((current->ipAsString));// Set IP address of the UDP receiver



	packetTTL=60; //TTL for UDP packet 
	errorbit=setsockopt(current->UDPsocketID, IPPROTO_IP,IP_MULTICAST_TTL, &packetTTL, sizeof(packetTTL)); //configure the TTL

	if(errorbit==-1){
		perror("\n\n\n Failed to configure TTL \n"); //faild to configure socket options
		return 1;
	}


	if((fp=fopen(current->songName,"rb"))==NULL){

		ExitProgram();
	}
	

	fseek(fp, 0, SEEK_END);
	fileLen=ftell(fp);  //find size of file 
	rewind (fp);

	buffer=(char *)calloc(fileLen,sizeof(char));
	 
		  result = fread (buffer,1,fileLen,fp);
	
		  fclose(fp);
	
	while(!exitProgramFlag)
	{

		for(countBytesSent=0;countBytesSent<=fileLen;)
		{
			if(countBytesSent+bufferSize > fileLen) break;

			temp=sendto(current->UDPsocketID,&buffer[countBytesSent],bufferSize,0,(struct sockaddr *)&current->stationAddr,sizeof(current->stationAddr));
			usleep(SERVER_SEND_SONGS_RATE);//make 128kib/s 62500

			countBytesSent+=temp;
			if(temp==-1){
				perror("\n Error while sending  song");
				ExitProgram();
			}
		}
		printf("\nSong SIZE:%ld\n",countBytesSent);
		x=fileLen-countBytesSent;

		if(x>0){
			temp=sendto(current->UDPsocketID,&buffer[countBytesSent],x,0,(struct sockaddr *)&current->UDPsocketID,sizeof(current->UDPsocketID));
			usleep(SERVER_SEND_SONGS_RATE);
			countBytesSent+=temp;
		}

	}

	error=close(current->UDPsocketID);
	if(error==-1) {
	  	perror("\n\n\n\n\nFailed to close the UDP socket");

	  	return 1;
	}

	return 0;
}

int ReciveSong(int clientID, int* dontTouchme) 
{
	FILE* fp;
	char buffer[1000];
	int countReciveBytes=0;
    int	temp=0;
	struct timeval tv_res; //timeout struct - we use this to check if timeout expired between 2 events filds:tv_sec, tx_usec
	tv_res.tv_sec=3;
	tv_res.tv_usec=0;
	struct timeval *ptv_res=&tv_res;

	fd_set readResfds; //pointer to the place we want to read data from
	int res_event=-2;

	printf("\nFile name is:%s\n",ResivesongName);
	//puts("opening file- RESIVER\n");

	strcpy(newStationTemp.songName,ResivesongName);
	newStationTemp.songLenName=strlen(ResivesongName);

	fp=fopen(ResivesongName,"wb");
	if(fp==NULL)
	{
		puts("Could not receive the file\n");
		ResiveSongSize=0;
		ResivesongNamesize=0;
		memset (& ResivesongName, 0, sizeof(ResivesongName));
		sending =-1;
		fclose(fp); 
		closeClient(clientID);
		return 1;
	}
	puts("Success in opening the file\n");
	while(countReciveBytes<ResiveSongSize)
	{
		FD_ZERO (&readClientfds[clientID]);
		FD_SET (clientTCPSocket[clientID],&readClientfds[clientID]);


		res_event=-2;
			do{//wait for event form {welcome, std, or any client}

				res_event=select(clientTCPSocket[clientID]+1,&readClientfds[clientID], NULL,NULL,ptv_res);

			}while(res_event==-2);

			if(res_event==-1){//timeout
				puts("\nEROR OUT WHILE CLIENT SEND AS SONG\n");
				FD_CLR(clientTCPSocket[clientID],&readClientfds[clientID]);
				ResiveSongSize=0;
				ResivesongNamesize=0;
				memset (& ResivesongName, 0, sizeof(ResivesongName));
				sending =-1;
				fclose(fp); 
				return -1; 

			}

	
		if(res_event==0){///timeout
			puts("\nTIME OUT WHILE CLIENT SEND AS SONG");
			FD_CLR(clientTCPSocket[clientID],&readClientfds[clientID]);
			ResiveSongSize=0;
			ResivesongNamesize=0;
			memset (& ResivesongName, 0, sizeof(ResivesongName));
			sending =-1;
			fclose(fp);
			return 0; 

		}

		if(FD_ISSET(clientTCPSocket[clientID],&readClientfds[clientID]))
		{
			FD_CLR(clientTCPSocket[clientID],&readClientfds[clientID]);
			temp=recv(clientTCPSocket[clientID], buffer, 1000, 0);
			if(temp==-1)
			{
				printf("\n\nFailed to receive DATA from client: %d, this client removed\n\n",clientID);
				InvalidHandler(clientID,UpSong);
				ResiveSongSize=0;
				ResivesongNamesize=0;
				memset (& ResivesongName, 0, sizeof(ResivesongName));
				sending =-1;
				fclose(fp);
				closeClient(clientID);
				return -1;
			}
			ptv_res->tv_sec=3;//restart

			countReciveBytes+=temp;
			if(countReciveBytes==countReciveBytes-temp)
			{
				ResiveSongSize=0;
				ResivesongNamesize=0;
				memset (& ResivesongName, 0, sizeof(ResivesongName));
				sending =-1;
				fclose(fp);
				closeClient(clientID);
				puts("\nClient closed the connection without Notice!!!");
				return -1;
			}

			printf("\nReciving: %d\n",countReciveBytes);
			fwrite(buffer, temp, 1, fp);

			if(countReciveBytes>=ResiveSongSize)
			{
				printf("\nfilename is:%s",ResivesongName);
				ResiveSongSize=0;
				ResivesongNamesize=0;
				memset (& ResivesongName, 0, sizeof(ResivesongName));

				numStations++;
				printf("\nnumStations++  %d \n", numStations);
				//~
				addNewSationFlag=1;
				addNewStationToDataBase(); 
				fclose(fp);
				
				newStationSender();
				sending =-1;
				*dontTouchme=0; 
				puts("\n Done Updating about the new station!");

				return 1;
			}

		}
	}
	return 1;
}

void printStatus()
{
	int i=0, j=0;;
	printf("\nRADIO_SERVER");
	printf("\n----------------");
	printf("\n Current Num Of Stations %d:",numStations);
	for(i=0; i<=numStations; i++)
	{
		printf("\nStation %d: %s",i, pStations[i]->ipAsString);
		printf("Song: %s", pStations[i]->songName);
	}
	printf("\n----------------");
	printf("\n Current Num Of Clients %d:",numClients);
	for(i=0, j=0; i<100; i++)
	{
		if(clientTCPSocket[i]!=-1){
			printf("\nClient %d: %s",j,inet_ntoa(addrClientTCP[i].sin_addr));
			j++;
		}
	}
	printf("\n-------------------------\n");
}
