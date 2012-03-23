
/*
CS457DL Project 2
Steve Watts
Jason Kim
*/


/* This is the ss program portion of the CS457DL Lab/Programming Assignnment 1 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include "awget.h"

#define MAXCHAINDATASIZE 33664 // max number of bytes for chain data
#define MAXFILESIZE 50000 // max number of bytes we can get retrieve from URL
#define MAXPENDING 10           // how many pending connections queue will hold
int portNumber;                 /* port number for connection */
struct chainData cdPacked,cd;
char urlValue[255];
char ssId[255];

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

char* sendToNextSS(struct chainLink *cLink, struct chainData *cData)
{
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct addrinfo *p;
	struct timeval tv;
	int rv;
	int sockfd;
	fd_set read_fds;  // temp file descriptor list for select()
	char s[INET6_ADDRSTRLEN];
	char returnMsg[MAXFILESIZE];
	char portNo[6];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	sprintf(portNo, "%d", cLink->SSport);
	if ((rv = getaddrinfo(cLink->SSaddr, portNo, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(0);
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "client: failed to connect\n");
		exit(0);
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	// prepare chaindata msg before sending - convert to network byte order
	for (int i = 0; i < (cData->numLinks-1); i++)
	{
		cData->links[i].SSport = htonl(cData->links[i].SSport);
	}
	int oldNumLinks = cData->numLinks;
	cData->numLinks = htonl(oldNumLinks);

	// send message
	if (send(sockfd, (char*)cData, MAXCHAINDATASIZE, 0) == -1) // sending MAXDATASIZE byte message
	{
		perror("send");
		exit(10);
	}
	else
	{
		printf("Sent Chain Data with %u links to %s:%s via TCP\n", oldNumLinks, cLink->SSaddr, portNo);
	}

	// prepare descriptor set
	FD_ZERO(&read_fds);
	FD_SET(sockfd, &read_fds);

	// wait for response
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	rv = select(sockfd+1, &read_fds, NULL, NULL, &tv);
	if (rv == -1) // ERROR
	{
		perror("Error in select()");
		exit(1);
	}
	else if (rv == 0) // ERROR - TIMEOUT
	{
		printf("Timeout while waiting for response from server\n");
		exit(2);
	}
	else // SUCCESSFUL RESPONSE
	{
		if (FD_ISSET(sockfd, &read_fds))
		{
			recv(sockfd, returnMsg, sizeof returnMsg, 0);
			close(sockfd);
		}
	}

	return returnMsg;
}

char* getFile(struct chainData *chaindata)
{
	// TODO: (URL HANDLER) Code to retrieve the document from the URL and returns a char* with the contents of the document.
	return "";
}

void startServer(char* cPortNumber)
{
    struct chainData *chaindata;
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;
    int nbytes;
    char remoteIP[INET6_ADDRSTRLEN];
    char buf[33664];    // buffer for client data
    char returnMsg[MAXFILESIZE]; // buffer for file to retrieve
    int rv;
    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    int fdmax;        // maximum file descriptor number
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    struct sockaddr_storage remoteaddr; // client address

	// Start server
	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);
	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, cPortNumber, &hints, &ai)) != 0)
	{
		fprintf(stderr, "Server Error: %s\n", gai_strerror(rv));
		exit(1);
	}

	for(p = ai; p != NULL; p = p->ai_next)
	{
		// create listener socket
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			continue;  // try with next addrinfo
		}

		// lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break; // successful socket creation
	}





	// XXX: Test Code
	// XXX: Test Code
	// XXX: Test Code
	int portNumber = atoi (cPortNumber);
	if (portNumber == 17)
	{
		chaindata = new chainData();
		chaindata->numLinks = 4;
		chainLink link1;
		strcpy(link1.SSaddr, "127.0.0.1");
		link1.SSport = 30030;
		chainLink link2;
		strcpy(link2.SSaddr, "127.0.0.1");
		link2.SSport = 30031;
		chainLink link3;
		strcpy(link3.SSaddr, "127.0.0.1");
		link3.SSport = 30032;
		chainLink link4;
		strcpy(link4.SSaddr, "127.0.0.1");
		link4.SSport = 30033;
		chaindata->links[0] = link1;
		chaindata->links[1] = link2;
		chaindata->links[2] = link3;
		chaindata->links[3] = link4;

		char ssAddr[128] = "";
		strcpy (ssAddr, chaindata->links[chaindata->numLinks-1].SSaddr);
		int ssPort = chaindata->links[chaindata->numLinks-1].SSport;

		if (chaindata->numLinks >= 1)
		{ // Need to forward to other SS's
			printf("The URL to forward to is %s with port %d\n\n", ssAddr, ssPort);

			// Forward this to next SS and get return msg
			strcpy(returnMsg, sendToNextSS(&chaindata->links[chaindata->numLinks-1], chaindata));

			printf("%s - File retrieved successfully, contents: %s\n\n", ssId, returnMsg);

			exit(1);
		}
	}
	// XXX: END Test Code
	// XXX: END Test Code
	// XXX: END Test Code






	if (p == NULL) // if we got here, it means we didn't get bound
	{
		fprintf(stderr, "Server Error: Failed to bind to socket for listener\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

	// listen
	if (listen(listener, 10) == -1) {
		fprintf(stderr, "Server Error: Listener socket setup error");
		exit(3);
	}

	// add the listener to the master set
	FD_SET(listener, &master);

	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one
	printf("Server started successfully on port %s via TCP\n", cPortNumber);

	// main loop
	for(;;) {
		read_fds = master; // copy fd set
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
		{
			fprintf(stderr, "Server Error: Error while checking for data to be recv()");
			exit(4);
		}

		// run through the existing connections looking for data to read
		for(int i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &read_fds))
			{ // we got one!!
				if (i == listener)
				{
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

					if (newfd == -1)
					{
						perror("Server Error: While creating new socket for accept()");
						exit(5);
					}
					else
					{
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax)
						{    // keep track of the max
							fdmax = newfd;
						}
						printf("New connection from %s on socket %d\n",
							inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN),
								newfd);
					}
				}
				else // Data has returned on the socket meant for data transfer
				{
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
					{ // ERROR STATE
						// got error or connection closed by client
						if (nbytes == 0)
						{
							// connection closed
							printf("Server: socket %d hung up. (Cxn closed)\n", i);
						}
						else
						{
							perror("Server Error: Error in recv");
							exit(6);
						}
						close(i); // bye!
						FD_CLR(i, &master); // remove from master set
					}
					else
					{ // Received data properly - HANDLE DATA HERE
						// print out the data to the server console.
						chaindata = (struct chainData *)buf;

						// return chaindata msg data to host byte order
						chaindata->numLinks = ntohl(chaindata->numLinks) - 1;
						for (int i = 0; i < chaindata->numLinks; i++)
						{
							chaindata->links[i].SSport = ntohl(chaindata->links[i].SSport);
						}

						printf("%s - NumLinks remaining in chaindata is: %u\n", ssId, chaindata->numLinks);

						if (chaindata->numLinks >= 1)
						{ // Need to forward to other SS's
							/* TODO: (SSHANDLER) Right now, I am just taking the last chainlink from the list and decreasing the numlinks counter.
							 * Need to add random selection of chainlink and recreate the chaindata->links array properly so that after the random
							 * link is removed, the new array is reordered accordingly with one less link.
							 */
							char ssAddr[128] = "";
							strcpy (ssAddr, chaindata->links[chaindata->numLinks-1].SSaddr);
							int ssPort = chaindata->links[chaindata->numLinks-1].SSport;

							printf("%s - The host to forward to is %s on port %d\n\n", ssId, ssAddr, ssPort);

							// Forward this to next SS and get return msg
							strcpy(returnMsg, sendToNextSS(&chaindata->links[chaindata->numLinks-1], chaindata));
						}
						else if (chaindata->numLinks == 0)
						{ // Last SS, time to retrieve the document
							// TODO: (URL HANDLER) CALL WGET HERE
							// strcpy(returnMsg, getFile(&chaindata));

							// XXX: TEST CODE
							printf("%s - Retrieved file successfully!! \n\n", ssId);
							strcpy(returnMsg, "Yadda Yadda Yadda Yadda Yadda Yadda");
						}

						// successful message retrieval.  return message to sender
						if (send(i, returnMsg, MAXFILESIZE, 0) == -1) // sending 1 byte message
						{
							perror("Error in send()\n");
							exit(7);
						}
						else
						{
							// TODO: (CLEANUP) Message has returned, do necessary cleanup to cover tracks
							printf("%s - Sent return message back\n", ssId);
							FD_CLR(i, &master); // remove from master set
						}
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!
}

int
printUsage ()
{
    fprintf (stdout, "Usage: ss -p <port number>\n");
    return 0;
}

int
main (int argc, char **argv)
{

    int c;
    int i;
    int iMaxPortNumber = 65535;
    char *argValue = NULL;
    char *cPortNumber;
    int tArgEntered = 0;
    int pArgEntered = 0;

    char hostName[1024];
    hostName[1023] = '\0';
    gethostname(hostName,1023);

    opterr = 0;

    while ((c = getopt (argc, argv, "p:")) != -1)
      {
          argValue = optarg;

          switch (c)
            {
            case 'p':

                pArgEntered = 1;
                cPortNumber = argValue;

                for (i = 0; i < strlen (argValue); i++)
                  {
                      if (i == 0)
                        {
                            if (argValue[i] == '-')
                              {
                                  fprintf (stderr, "Value given for port number cannot be negative.\n");
                                  printUsage ();
                                  exit (1);
                              }
                        }
                      if (isdigit (argValue[i]) == 0)
                        {
                            fprintf (stderr, "Value given for port number must only contain digits 0-9.\n");
                            printUsage ();
                            exit (1);
                        }
                  }
                if ((portNumber = atoi (argValue)) == 0)
                  {
                      fprintf (stderr, "Error returned converting port number argument value to integer. Value given for port number is (%s).\n", argValue);
                      printUsage ();
                      exit (1);
                  }
                if (portNumber > iMaxPortNumber)
                  {
                      fprintf (stderr, "Value given for port number (%s) cannot exceed %i.\n", argValue, iMaxPortNumber);
                      printUsage ();
                      exit (1);
                  }
                break;
            default:
                fprintf (stderr, "Unknown command line option: -%c\n", optopt);
                printUsage ();
                exit (1);
            }
      }

    if (pArgEntered == 0)
      {
          fprintf (stderr, "Command line argument '-p <port number>' is required.\n");
          printUsage ();
          exit (1);
      }

    /* start the actual server portion of the task */
    printf ("\n\nStepping SsStone Program\nhost name: %s port: %d\n\n",hostName,portNumber);
    strcpy(ssId, hostName);
    strcat(ssId, ":");
    strcat(ssId, cPortNumber);
    startServer(cPortNumber);

    return 0;

}
