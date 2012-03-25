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
int portNumber; /* port number for connection */
char hostName[1024];
struct chainData cdPacked, cd;
char urlValue[255];
char ssId[255];

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
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

/***
 * populates SSaddr, SSport, and linkNumChosen with random chainlink from chainData cd
 */
void getRandomSS(struct chainData *cd, char **SSaddr, int *SSport, int *linkNumChosen)
{
    int i = 0;
    i = rand() % cd->numLinks;

    *SSaddr = cd->links[i].SSaddr;
    *SSport = cd->links[i].SSport;
    *linkNumChosen = i;
}

/***
 * Remove this stepping stone's entry from list of chainlinks
 */
void removeMeFromChainlinks(struct chainData *cData)
{
    int found = 0;
    for (int i = 0; i < cData->numLinks; i++)
    {
        if ((strcmp(cData->links[i].SSaddr, hostName) == 0) && (cData->links[i].SSport == portNumber))
        {
            /* found!  remove this entry.  if this is not the last element in the array,
             * replace the element at this index with the last element.
             */
            found = 1;
            if (i < cData->numLinks - 1)
            {
                strcpy(cData->links[i].SSaddr, cData->links[cData->numLinks - 1].SSaddr);
                cData->links[i].SSport = cData->links[cData->numLinks - 1].SSport;
            }
            cData->numLinks = cData->numLinks - 1;
        }
    }

    if (!found)
    {
        DieWithError ("Error while removing this hostname from links in chaindata");
    }
}

/***
 * Convert chaindata and url to a string format, cdString, for transport
 */
void chainDataAndURLToString(struct chainData *cd, char *urlValue, char **cdString)
{
    int i;
    int thisSize = 0;
    char thisLine[255];

    thisSize = 6 * sizeof(char) + strlen(urlValue) * sizeof(char);
    *cdString = (char*) malloc(thisSize);
    strcpy(*cdString, urlValue);
    strcat(*cdString, "\n");
    sprintf(thisLine, "%d\n", cd->numLinks);
    strcat(*cdString, thisLine);

    for (i = 0; i < cd->numLinks; i++)
    {
        thisSize += 7 * sizeof(char)
                + strlen(cd->links[i].SSaddr) * sizeof(char);
        *cdString = (char*) realloc(*cdString, thisSize);

        sprintf(thisLine, "%s,%d\n", cd->links[i].SSaddr, cd->links[i].SSport);
        strcat(*cdString, thisLine);
    }
}

/***
 * Takes fileStream and populates urlValue and chainData cd
 */
void streamToURLAndChainData(char *fileStream, char *urlValue, struct chainData *cd)
{
    char *token = NULL;
    char thisInt[6];
    int i;
    char *j;

    token = strtok(fileStream, "\n");
    strcpy(urlValue, token);
    token = strtok(NULL, "\n");
    cd->numLinks = atoi(token);
    for (i = 0; i < cd->numLinks; i++)
    {
        token = strtok(NULL, "\n");

        j = strchr(token, ',');
        strncpy(cd->links[i].SSaddr, token, j - token);
        cd->links[i].SSaddr[j - token] = '\0';
        cd->links[i].SSport = atoi(j + 1);
    }
}

char* sendToNextSS(struct chainData *cData, char *urlValue)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;
    struct timeval tv;
    int rv;
    int sockfd;
    fd_set read_fds; // temp file descriptor list for select()
    char s[INET6_ADDRSTRLEN];
    char returnMsg[MAXFILESIZE];
    char portNo[6];
    char *buf;
    int ssPort;
    char *ssAddr;
    int linkNumChosen;

    /* remove myself from list of chaindata's array of chainlinks */
    removeMeFromChainlinks(cData);

    /* get random SS to send to */
    getRandomSS(cData, &ssAddr, &ssPort, &linkNumChosen);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    sprintf(portNo, "%d", ssPort);
    if ((rv = getaddrinfo(ssAddr, portNo, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(0);
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
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

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    // convert chaindata and url to char array that we can send
    chainDataAndURLToString(cData, urlValue, &buf);

    fprintf(stdout, "URL and Chainlist are:\n*********\n%s\n*********\n", buf);
    fprintf(stdout, "The next SS is %s, %d\nWaiting...\n", ssAddr, ssPort);

    // send message
    if (send(sockfd, buf, strlen(buf), 0) == -1)
    {
        perror("send");
        exit(10);
    }
    else
    {
        printf("Sent Chain Data with %u links to %s:%s via TCP\n", cData->numLinks, ssAddr, portNo);
    }

    // prepare descriptor set
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    // wait for response
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    rv = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
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
            printf("Fetching %s\nRelay successfully completed!\n", urlValue);
        }
    }

    return returnMsg;
}

void startServer(char* cPortNumber)
{
    struct chainData *chaindata;
    fd_set master; // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;
    int nbytes;
    char remoteIP[INET6_ADDRSTRLEN];
    int recvMsgSize = 8192;
    char buf[8192];
    char returnMsg[MAXFILESIZE]; // buffer for file to retrieve
    int rv;
    int listener; // listening socket descriptor
    int newfd; // newly accept()ed socket descriptor
    int fdmax; // maximum file descriptor number
    int yes = 1; // for setsockopt() SO_REUSEADDR, below
    socklen_t addrlen;
    struct sockaddr_storage remoteaddr; // client address

    // Start server
    FD_ZERO(&master); // clear the master and temp sets
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

    for (p = ai; p != NULL; p = p->ai_next)
    {
        // create listener socket
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue; // try with next addrinfo
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
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
    if (listen(listener, 10) == -1)
    {
        fprintf(stderr, "Server Error: Listener socket setup error");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one
    printf("Server started successfully on port %s via TCP\n", cPortNumber);

    // main loop
    for (;;)
    {
        read_fds = master; // copy fd set
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            fprintf(stderr, "Server Error: Error while checking for data to be recv()");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for (int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            { // we got one!!
                if (i == listener)
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);

                    if (newfd == -1)
                    {
                        perror("Server Error: While creating new socket for accept()");
                        exit(5);
                    }
                    else
                    {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax)
                        { // keep track of the max
                            fdmax = newfd;
                        }
                        printf("New connection from %s on socket %d\n",
                                inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*) &remoteaddr), remoteIP,
                                        INET6_ADDRSTRLEN), newfd);
                    }
                }
                else // Data has returned on the socket meant for data transfer
                {
                    int vv;
                    vv = 5;
                    // handle data from a client
                    if ((nbytes = recv(i, buf, recvMsgSize, 0)) <= 0)
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
                        chaindata = new chainData();
                        streamToURLAndChainData(buf, urlValue, chaindata);

                        printf("%s - NumLinks remaining in chaindata is: %u\n", ssId, chaindata->numLinks);

                        if (chaindata->numLinks > 1)
                        { // Need to forward to other SS's
                            // Forward this to next SS and get return msg
                            fprintf(stdout, "Receiving request\n");
                            strcpy(returnMsg, sendToNextSS(chaindata, urlValue));
                        }
                        else if (chaindata->numLinks == 1)
                        { // Last SS, time to retrieve the document
                            fprintf(stdout, "This is the last SS, wget %s\n", urlValue);
                            /* TODO: (URL HANDLER) EXECUTE SYSTEM() TO ISSUE WGET.
                             * The commented line of code below is a suggestion of how to implement the URL handler.
                             * Feel free to change it as long as there is a populated returnMsg variable after the
                             * file is retrieved.
                             */
                            // strcpy(returnMsg, getFile(&chaindata));
                            // XXX: TEST CODE
                            strcpy(returnMsg, "TEST TEST TEST TEST TEST TEST");
                            printf("Relay file...\nSuccessful!\n");
                        }

                        // successful message retrieval.  return message to sender
                        if (send(i, returnMsg, MAXFILESIZE, 0) == -1) // sending 1 byte message
                        {
                            perror("Error in send()\n");
                            exit(7);
                        }
                        else
                        {
                            /* TODO: (CLEANUP) Message has returned, do necessary cleanup to cover tracks.
                             * Needs to be aware of the (URL HANDLER) code because that is where a local copy is stored.  Coordinate
                             * to clean up those files.
                             */
                            FD_CLR(i, &master); // remove from master set
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
}

int printUsage()
{
    fprintf(stdout, "Usage: ss -p <port number>\n");
    return 0;
}

int main(int argc, char **argv)
{

    int c;
    int i;
    int iMaxPortNumber = 65535;
    char *argValue = NULL;
    char *cPortNumber;
    int tArgEntered = 0;
    int pArgEntered = 0;

    hostName[1023] = '\0';
    gethostname(hostName, 1023);

    opterr = 0;

    while ((c = getopt(argc, argv, "p:")) != -1)
    {
        argValue = optarg;

        switch (c)
        {
            case 'p':

                pArgEntered = 1;
                cPortNumber = argValue;

                for (i = 0; i < strlen(argValue); i++)
                {
                    if (i == 0)
                    {
                        if (argValue[i] == '-')
                        {
                            fprintf(stderr, "Value given for port number cannot be negative.\n");
                            printUsage();
                            exit(1);
                        }
                    }
                    if (isdigit(argValue[i]) == 0)
                    {
                        fprintf(stderr, "Value given for port number must only contain digits 0-9.\n");
                        printUsage();
                        exit(1);
                    }
                }
                if ((portNumber = atoi(argValue)) == 0)
                {
                    fprintf(stderr,
                            "Error returned converting port number argument value to integer. Value given for port number is (%s).\n",
                            argValue);
                    printUsage();
                    exit(1);
                }
                if (portNumber > iMaxPortNumber)
                {
                    fprintf(stderr, "Value given for port number (%s) cannot exceed %i.\n", argValue, iMaxPortNumber);
                    printUsage();
                    exit(1);
                }
                break;
            default:
                fprintf(stderr, "Unknown command line option: -%c\n", optopt);
                printUsage();
                exit(1);
        }
    }

    if (pArgEntered == 0)
    {
        fprintf(stderr, "Command line argument '-p <port number>' is required.\n");
        printUsage();
        exit(1);
    }

    /* start the actual server portion of the task */
    printf("\n\nStepping SsStone Program\nhost name: %s port: %d\n\n", hostName, portNumber);
    strcpy(ssId, hostName);
    strcat(ssId, ":");
    strcat(ssId, cPortNumber);
    startServer(cPortNumber);

    return 0;

}
