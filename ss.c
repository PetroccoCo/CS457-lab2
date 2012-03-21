
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

#define MAXDATASIZE 33664 // max number of bytes we can get at once
#define MAXPENDING 10           // how many pending connections queue will hold
int portNumber;                 /* port number for connection */
struct chainData cdPacked,cd;
char urlValue[255];

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
    int tArgEntered = 0;
    int pArgEntered = 0;

    char hostName[1024];
    hostName[1023] = '\0';
    gethostname(hostName,1023);

    struct chainData *chaindata;
    char *cPortNumber = NULL;
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;
    int nbytes;
    char remoteIP[INET6_ADDRSTRLEN];
    char buf[MAXDATASIZE];    // buffer for client data
    int rv;
    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    int fdmax;        // maximum file descriptor number
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    struct sockaddr_storage remoteaddr; // client address

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
    printf ("\n\nStepping Stone Program\nhost name: %s port: %d\n\n",hostName,portNumber);

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
	printf("Server started on port %s via TCP\n", cPortNumber);

	// main loop
	for(;;) {
		read_fds = master; // copy fd set
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
		{
			fprintf(stderr, "Server Error: Error while checking for data to be recv()");
			exit(4);
		}

		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++)
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
				else // data came into socket meant for data, not on the listener socket for new cxn's
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
						printf("The size is: %u\n", ntohl(chaindata->numLinks));

						// send response message
						/*
						replydatas = new replymsg();
						replydatas->header = 1;
						if (send(i, (char*)replydatas, 1, 0) == -1) // sending 1 byte message
						{
							perror("Error in send()\n");
							exit(7);
						}
						else
							printf("Sent reply\n");
						 */
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!

    return 0;

}
