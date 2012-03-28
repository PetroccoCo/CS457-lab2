/*
 CS457DL Project 2
 Steve Watts
 Jason Kim
 Pete Winterscheidt
 */

/* This is the ss program portion of the CS457DL Lab/Programming Assignnment 2 */

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
#include <iostream>
#include <fstream>
#include "awget.h"

#define MAXCHAINDATASIZE 33664 // max number of bytes for chain data
#define MAXPENDING 10           // how many pending connections queue will hold
#define MAXTIMEOUT 20 // max num seconds to wait before timeout
#define MAXFILESIZE 550544 // max number of bytes we can get retrieve from URL

int portNumber; /* port number for connection */
char hostName[1024];
struct chainData cdPacked, cd;
char urlValue[255];
char ssId[255];
int counter = 0;

using namespace std;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int stringToFile(char *s, char *fileName, int fileSize)
{
    FILE *fd = fopen(fileName, "w");
    if (fd == NULL)
    {
        DieWithError("File Open error.");
    }
    int i = 0;
    for (i; i < fileSize; i++)
    {
        fputc(s[i], fd);
    }
//    while (s[i] != '\0')
//    {
//        fputc(s[i++], fd);
//    }
    fclose(fd);

//    ofstream outfile ("newAwgetTest.pdf");
//    // write to outfile
//    outfile.write (s, 148544);
//    outfile.close();
}

int fileToString(char *fileName, char *s, int *fileSize)
{
    long size;
    ifstream infile (fileName, ifstream::binary);
    // get size of file
    infile.seekg(0,ifstream::end);
    *fileSize=infile.tellg();
    infile.seekg(0);

    // read content of infile
    char buffer[MAXFILESIZE];
    infile.read (buffer,*fileSize);
    infile.close();
    printf("Loaded Size: %ld\n\n", *fileSize);

    memcpy (s, buffer, *fileSize);

//    ofstream outfile ("newTest.pdf",ofstream::binary);
//    // write to outfile
//    outfile.write (s,*fileSize);
//    outfile.close();
}

int goGetFile(char *url)
{
// tested this using linux.about.com/index.html and it works.
// the -no-check.. stuff is an attempt to get around certificate issues
// when getting files from www.cs.colostate.edu (it doesn't quite work...)
  char cmdLine[255] = "";
  strcat(cmdLine,"wget ");
  strcat(cmdLine,url);
  strcat(cmdLine," --no-check-certificate");

    if (system(cmdLine) != 0)
    {
        DieWithError("System() call to wget file failed.\n");
    }
}

int rmFile(char *fileName)
{

    char cmdLine[255] = "";
    strcat(cmdLine, "rm ");
    strcat(cmdLine, fileName);

    if (system(cmdLine) != 0)
    {
        DieWithError("System() call to rm file failed.\n");
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
    int i = 0;
    for (i = 0; i < cData->numLinks; i++)
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
        thisSize += 7 * sizeof(char) + strlen(cd->links[i].SSaddr) * sizeof(char);
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

char* sendToNextSS(struct chainData *cData, char *urlValue, int *fileSize)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;
    struct timeval tv;
    int rv;
    int sockfd;
    fd_set read_fds; // temp file descriptor list for select()
    char s[INET6_ADDRSTRLEN];
    char portNo[6];
    char *buf;
    int ssPort;
    char *ssAddr;
    int linkNumChosen;
    char *totalMsg;
    int bytesRecv = 0;
    int recvRV = 0;
    char *buffer;
    int sizeBuffer;

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
    //    printf("client: connecting to %s\n", s);

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
    free(buf);

    // prepare descriptor set
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    // wait for response
    tv.tv_sec = MAXTIMEOUT;
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
            // receive the header - file size
            if (recv(sockfd, &sizeBuffer, 4, 0) == -1) { perror("Error in recv()\n"); exit(7); }
            sizeBuffer = ntohl(sizeBuffer);
            *fileSize = sizeBuffer;

            printf("GOT MESSAGE SIZE: %d\n\n", sizeBuffer);
            totalMsg = (char*) malloc(sizeBuffer);
            int i = 0;
            int msgIndex = 0;
            while (bytesRecv < sizeBuffer)
            {
                buffer = (char*) malloc(sizeBuffer - bytesRecv);
                printf("Bytes begin recv: %d\n\n", bytesRecv);
                recvRV = recv(sockfd, buffer, (sizeBuffer - bytesRecv), 0);
                if (recvRV == -1) { perror("Error in recv()\n"); exit(7); }
                else
                {
                    bytesRecv = bytesRecv + recvRV;
                }
                printf("Bytes recv: %d\n\n", bytesRecv);
                for (i = 0; i < recvRV; i++)
                {
                    totalMsg[msgIndex++] = buffer[i];
                }
                printf("Cat done\n");
            }
            printf("Received num bytes - %d\n\n", bytesRecv);
            close(sockfd);
            printf("Fetching %s\nRelay successfully completed!\n", urlValue);

            //XXX: REMOVE!
//            char servName[80];
//            sprintf(servName, "SKIPTestEarly-%d-%d.pdf", portNumber, counter++);
//            ofstream outfile (servName, ofstream::binary);
//            printf("Writing skip output file");
//            // write to outfile
//            outfile.write (totalMsg, *fileSize);
//            outfile.close();
        }
    }

    return totalMsg;
}

void startServer(char* cPortNumber)
{
    struct chainData *chaindata = new chainData();
    fd_set master; // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;
    int nbytes;
    char remoteIP[INET6_ADDRSTRLEN];
    int recvMsgSize = 8192;
    socklen_t addrlen;
    struct sockaddr_storage remoteaddr; // client address

    char *returnMsg;
    int fileSize = 0;
    int fileSizeMsg = 0;
    int bytesSent = 0;
    char *sendBuffer;
    int rv;
    int listener; // listening socket descriptor
    int newfd; // newly accept()ed socket descriptor
    int fdmax; // maximum file descriptor number
    int yes = 1; // for setsockopt() SO_REUSEADDR, below

    //    printf("into startServer\n");

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
    //    printf("past getaddrinfo()\n");
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
    //    printf("past listener socket loop\n");

    if (p == NULL) // if we got here, it means we didn't get bound
    { fprintf(stderr, "Server Error: Failed to bind to socket for listener\n"); exit(2); }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1)
    { fprintf(stderr, "Server Error: Listener socket setup error"); exit(3); }

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
                    char buf[8192] = {};

                    // handle data from a client
                    if ((nbytes = recv(i, buf, recvMsgSize, 0)) <= 0)
                    { // ERROR STATE
                      // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("Server: socket %d hung up. (Cxn closed)\n", i);
                        } else {
                            perror("Server Error: Error in recv");
                            exit(6); }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    }
                    else
                    { // Received data properly - HANDLE DATA HERE
                        streamToURLAndChainData(buf, urlValue, chaindata);
                        returnMsg = (char*) malloc(MAXFILESIZE+20);

                        printf("%s - NumLinks remaining in chaindata is: %u\n", ssId, chaindata->numLinks);

                        if (chaindata->numLinks > 1)
                        { // Need to forward to other SS's
                            fprintf(stdout, "Receiving request\n");
                            strncpy(returnMsg, sendToNextSS(chaindata, urlValue, &fileSize), fileSize);
                            printf("Done stepping\n\n");

                            //XXX: REMOVE!
//                            char servName[80];
//                            sprintf(servName, "SKIPTest-%d-%d.pdf", portNumber, counter);
//                            printf("Done naming\n\n");
//                            ofstream outfile (servName, ofstream::binary);
//                            // write to outfile
//                            outfile.write (returnMsg, fileSize);
//                            outfile.close();
                        }
                        else if (chaindata->numLinks == 1)
                        { // Last SS, time to retrieve the document
                            goGetFile(urlValue);

                            fileToString(basename(urlValue), returnMsg, &fileSize);
                            // sendStringToParentThread(fileContents);

                            //XXX: REMOVE!
//                            char servName[80];
//                            sprintf(servName, "SKIPTestSource-%d-%d.pdf", portNumber, counter);
//                            ofstream outfile(servName, ofstream::binary);
//                            // write to outfile
//                            outfile.write (returnMsg, fileSize);
//                            outfile.close();
                        }

                        // send filesize as a 4 byte header
                        fileSizeMsg = htonl(fileSize);
                        if (send(i, &fileSizeMsg, 4, 0) == -1) { perror("Error in send()\n"); exit(7); }

                        printf("Sending payload (filesize is %d)\n\n", fileSize);

                        // send payload; keep send()ing until it is complete
                        bytesSent = 0;
                        while (bytesSent < fileSize)
                        {
                            sendBuffer = (char*) malloc(fileSize-bytesSent);
                            memcpy(sendBuffer, returnMsg+bytesSent, fileSize-bytesSent);
                            bytesSent = bytesSent + send(i, sendBuffer, (fileSize-bytesSent), 0);
                            printf("Sent back %d bytes\n\n", bytesSent);
                        }
                        if (bytesSent == -1) { // sending message
                            perror("Error in send()\n");
                            exit(7);
                        }
                        else
                        {
                            free(returnMsg);
                            printf("DBG: Bytes sent - %d\n\n", bytesSent);
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
    //    printf("calling startServer\n");
    startServer(cPortNumber);

    return 0;

}
