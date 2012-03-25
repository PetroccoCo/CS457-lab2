
/*
CS457DL Project 2
Steve Watts
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

#define MAXPENDING 10           // how many pending connections queue will hold
int portNumber;                 /* port number for connection */
struct chainData cdPacked,cd;
char urlValue[255];


void streamToURLAndChainData(char *fileStream, char *urlValue, struct chainData *cd) {
  char *token = NULL;
  char thisInt[6];
  int i;
  char *j;

  token = strtok(fileStream,"\n");
  strcpy(urlValue,token);

  token = strtok(NULL,"\n");
  cd->numLinks = atoi(token);

  for (i=0; i<cd->numLinks; i++) {
    token = strtok(NULL,"\n");

    j = strchr(token,',');
    strncpy(cd->links[i].SSaddr,token,j - token);
    cd->links[i].SSaddr[j-token] = '\0';
    cd->links[i].SSport = atoi(j+1);
  }
}

void
undoStream(char *buf, struct chainData *cd, char *urlValue) {

  memcpy(cd,buf,sizeof(struct chainData));
  memcpy(urlValue,buf+sizeof(struct chainData),255);

}

void unpackChainData (struct chainData *cd_in, struct chainData *cd_out) {
    int i;
    cd_out->numLinks = cd_in->numLinks;
    for (i = 0; i < cd_in->numLinks; i++) {
	strcpy(cd_out->links[i].SSaddr,cd_in->links[i].SSaddr);
	cd_out->links[i].SSport = ntohl(cd_in->links[i].SSport);
    }
  }


int goGetFile (char *url) {

  char cmdLine[255] = "";
  strcat(cmdLine,"wget ");
  strcat(cmdLine,url);
  strcat(cmdLine," --no-check-certificate");

  if (system(cmdLine) != 0) {
    DieWithError("System() call failed.\n");
  }
}

void
ProcessTCPClient (int clientSocket)
{
    int recvMsgSize = 8192;
    int gotMsgSize;

    char buf[8192];

    printf("recv is getting the message\n");
    if ((gotMsgSize = recv (clientSocket, buf, recvMsgSize , 0)) < 0)
        DieWithError ("recv() failed");
    printf("gotMsgSize = %d\n",gotMsgSize);
    printf("(received)*****\n%s\n*****\n",buf);
    fflush(stdout);

    /* Close client socket */
    if (close (clientSocket) != 0)
        DieWithError ("close() failed");

    streamToURLAndChainData(buf,urlValue,&cd);
    dbgPrintChainData (&cd);

    if (cd->numLinks == 0) {
      goGetFile(urlValue);
    }



}

void
doTCPServer ()
{

    int serverSocket;           /* Socket descriptor for server */
    int clientSocket;           /* Socket descriptor for client */
    struct sockaddr_in myServAddr;      /* Local address */
    struct sockaddr_in myClntAddr;      /* Client address */
    unsigned short myServPort;  /* Server port */
    unsigned int clntLen;       /* Length of client address data structure */

    /* Create socket for connections */
    if ((serverSocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError ("socket() failed");

    /* Fill address structure */
    memset (&myServAddr, 0, sizeof (myServAddr));       /* Zero out structure */
    myServAddr.sin_family = AF_INET;    /* Internet address family */
    myServAddr.sin_addr.s_addr = htonl (INADDR_ANY);    /* Any incoming interface */
    myServAddr.sin_port = htons (portNumber);   /* Local port */

    /* Bind to the local address */
    if (bind (serverSocket, (struct sockaddr *) &myServAddr, sizeof (myServAddr)) < 0)
        DieWithError ("bind() failed");

    /* set up the socket to listen */
    if (listen (serverSocket, MAXPENDING) < 0)
        DieWithError ("listen() failed");

    for (;;)
      {
          /* Set the size of the parameter */
          clntLen = sizeof (myClntAddr);

          /* Wait for a client to connect */
          if ((clientSocket = accept (serverSocket, (struct sockaddr *) &myClntAddr, &clntLen)) < 0)
              DieWithError ("accept() failed");

          /* Announce that message was received */
          printf ("Received message from client %s\n", inet_ntoa (myClntAddr.sin_addr));

          ProcessTCPClient (clientSocket);
      }
}

void
sigchld_handler (int s)
{
    while (waitpid (-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *
get_in_addr (struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
      {
          return &(((struct sockaddr_in *) sa)->sin_addr);
      }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
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


    opterr = 0;

    while ((c = getopt (argc, argv, "p:")) != -1)
      {

          argValue = optarg;

          switch (c)
            {
            case 'p':

                pArgEntered = 1;

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
    doTCPServer ();

    return 0;

}
