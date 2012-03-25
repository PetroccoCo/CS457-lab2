/*
 CS457DL Project 2
 Steve Watts
 */

/* This is the awget program portion of the CS457DL Lab/Programming Assignnment 1 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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
#include <netdb.h>
#include "awget.h"

#define MAXPORTNUMBER 65535
#define MAXFILESIZE 50000 // max number of bytes we can get retrieve from URL

int printUsage()
{
    fprintf(stdout, "Usage: awget <URL> [-c chainfile]\n");
    return 0;
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
 * populates SSaddr, SSport, and linkNumChosen with random chainlink from chainData cd
 */
void getRandomSS(struct chainData *cd, char **SSaddr, int *SSport,
        int *linkNumChosen)
{
    int i = 0;
    i = rand() % cd->numLinks;

    *SSaddr = cd->links[i].SSaddr;
    *SSport = cd->links[i].SSport;
    *linkNumChosen = i;
}

/***
 * Success returns 0
 * Else returns line number where error found
 */
int sanityCheckFile(char *fname)
{

    char buf[255];
    char SSaddr[255];
    char SSport[255];
    char *token = NULL;
    int i, j;
    char *comma = NULL;
    int commaPosition = 0;
    int sLen;
    int iRead = 0;
    int jRead = 0;

    FILE *fd = fopen(fname, "r");

    /* read and verify the integer on line 1 of the file */
    if (fgets(buf, 255, fd) == NULL)
    {
        return 1;
    } else
    {
        if (strcmp("0", buf) != 0)
        {
            if ((iRead = atoi(buf)) == 0)
            {
                return 1;
            }
        }
    }

    printf("FILE LINES = %d\n", iRead);

    for (i = 0; i < iRead; i++)
    {
        if (fgets(buf, 255, fd) == NULL)
        {
            return (2 + i);
        } else
        {
            sLen = strlen(buf);
            comma = strchr(buf, ',');
            commaPosition = comma - buf;
            if ((commaPosition < 1) || (commaPosition > (sLen - 1)))
            {
                return (2 + i);
            }
            token = strtok(buf, ",");
            strcpy(SSaddr, token);
            token = strtok(NULL, ",");
            strcpy(SSport, token);
            printf("DBG strlen is %d\n", strlen(SSport));
            for (j = 0; j < strlen(SSport) - 1; j++)
            {
                printf("Digits %s%d\n", SSport, j);
                /* this check is needed because text files in windows have \n\r instead of just \n */
                if ((SSport[j] != '\n') && (SSport[j] != '\r')
                        && (!isdigit(SSport[j])))
                {
                    printf("Digit %c\n", SSport[j]);
                    return (2 + i);
                }
            }
            if ((jRead = atoi(SSport)) == 0)
            {
                return (2 + i);
            }
            if ((jRead <= 0) || (jRead > MAXPORTNUMBER))
            {
                DieWithError("Invalid port number.");
            }
        }
    }

    fclose(fd);

    return 0;
}

/***
 * Success returns 0
 * Error returns 1
 */
int readChainFile(FILE *fd, struct chainData *cd)
{

    char buf[255];
    char *token = NULL;
    int i;
    int iRead = 0;

    /* read and verify the integer on line 1 of the file */
    if (fgets(buf, 255, fd) == NULL)
    {
        return 1;
    } else
    {
        if ((iRead = atoi(buf)) == 0)
        {
            return 1;
        }
    }
    cd->numLinks = iRead;

    for (i = 0; i < cd->numLinks; i++)
    {
        if (fgets(buf, 255, fd) == NULL)
        {
            return 1;
        } else
        {
            token = strtok(buf, ",");
            strcpy(cd->links[i].SSaddr, token);
            token = strtok(NULL, ",");
            cd->links[i].SSport = atoi(token);
        }
    }

    return 0;
}

void sendURLandChainData(struct chainData *cd, char *urlValue, char *returnMsg)
{
    int sock; /* Socket descriptor */
    struct sockaddr_in myServAddr; /* server address */
    unsigned short myServPort; /* server port */
    char rcvBuffer; /* Buffer for reply string */
    int bytesRcvd, totalBytesRcvd; /* Bytes read in single recv()
     and total bytes read */
    struct hostent *hostentStruct; /* helper struct for gethostbyname() */
    char *serverName;
    char *SSaddr;
    int SSport, portNumber;
    int i;
    char *buf;
    int chosenLinkNum;
    int ipAddressEntered = 1; /* flag to indicate an IP address was entered as -s arg */
    struct chainData cdSend;
    fd_set read_fds;  // temp file descriptor list for select()
    struct timeval tv;
    int rv;

    getRandomSS(cd, &SSaddr, &SSport, &chosenLinkNum);
    /* if all digits and periods, then IP address was entered */
    for (i = 0; i < strlen(SSaddr); i++)
    {
        if (SSaddr[i] != '.')
        {
            if (isdigit(SSaddr[i]) == 0)
            {
                ipAddressEntered = 0;
            }
        }
    }
    serverName = SSaddr;
    portNumber = SSport;

    /* Create a stream socket using TCP */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    /* establish 3 seconds as the timeout value */
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof tv))
        DieWithError("setsockopt() failed");

    /* Construct the server address structure */
    memset(&myServAddr, 0, sizeof(myServAddr)); /* Zero out structure */
    myServAddr.sin_family = AF_INET; /* Internet address family */
    if (ipAddressEntered)
    {
        /* load the address */
        myServAddr.sin_addr.s_addr = inet_addr(serverName);
    } else
    {
        /* if a hostname was entered, we need to look for IP address */
        hostentStruct = gethostbyname(serverName);
        if (hostentStruct == NULL)
            DieWithError("gethostbyname() failed");

        /* get the correct "format" of the IP address */
        serverName = inet_ntoa(
                *(struct in_addr *) (hostentStruct->h_addr_list[0]));
        if (serverName == NULL)
        {
            fprintf(stderr, "inet_ntoa() failed.\n");
            exit(1);
        }

        /* load the address */
        memcpy(&myServAddr.sin_addr, hostentStruct->h_addr_list[0],
                hostentStruct->h_length);
    }
    /* load the port */
    myServAddr.sin_port = htons(portNumber); /* Server port */

    /* Establish the connection to the server */
    if (connect(sock, (struct sockaddr *) &myServAddr, sizeof(myServAddr)) < 0)
        DieWithError("connect() failed");

    /* Send the struct to the server */
    chainDataAndURLToString(cd, urlValue, &buf);
    printf("<debug> cd+url string: %s\n", buf);

    if (send(sock, buf, strlen(buf), 0) != strlen(buf))
        DieWithError("send() sent a different number of bytes than expected");

    /* let everyone know the URL was sent */
    fprintf(stdout, "send buffer size = %d\n", strlen(buf));
    fprintf(stdout, "Sent chainfile contents to server %s:%d via TCP\n",
            serverName, portNumber);
    fprintf(stdout, "(sent)*****\n%s\n*****\n", buf);
    free(buf);

    /* prepare descriptor set */
//    FD_ZERO(&read_fds);
//    FD_SET(sock, &read_fds);

    // wait for response
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    rv = select(sock+1, &read_fds, NULL, NULL, &tv);
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
        if (FD_ISSET(sock, &read_fds))
        {
            recv(sock, returnMsg, sizeof returnMsg, 0);
            close(sock);
        }
    }

    /* sent the bytes to server */
    /* so close things up */
    if (close(sock) != 0)
        DieWithError("close() failed");
    fprintf(stderr, "Send was a Success - exiting.\n");
    exit(0);
}

int main(int argc, char **argv)
{
    struct chainData cd;
    char urlValue[255] = "";
    char *argValue = NULL;
    char *SSaddr;
    int SSport;
    char chainfileName[255] = "./chaingang.txt";
    int c;
    int gotCArg = 0;
    int i;
    int chosenLinkNum;
    int iMaxPortNumber = 65535;
    char returnMsg[MAXFILESIZE];

    opterr = 0;

    srand(time(NULL));

    if (argc >= 2)
    {

        while ((c = getopt(argc, argv, "c:")) != -1)
        {
            argValue = optarg;
            switch (c)
            {
                case 'c':
                    strcpy(chainfileName, argValue);
                    gotCArg = 1;
                    break;
                default:
                    fprintf(stderr, "Unknown command line option: -%c\n",
                            optopt);
                    printUsage();
                    exit(1);
            }
        }
        for (i = optind; i < argc; i++)
            strcpy(urlValue, argv[i]);
    } else
    {
        fprintf(stderr, "Command line argument '<URL>' is required.\n");
        printUsage();
        exit(1);
    }

    if (urlValue == "")
    {
        fprintf(stderr, "Command line argument '<URL>' is required.\n");
        printUsage();
        exit(1);
    }

    /* LOAD CHAINFILE */
    // open file
    FILE *fd = fopen(chainfileName, "r");
    if (fd == NULL)
    {
        fprintf(stderr, "Error opening chainfile specified ('%s').\n",
                chainfileName);
        exit(1);
    }
    // check file for errors
    if ((i = sanityCheckFile(chainfileName)) != 0)
    {
        fprintf(stderr,
                "Error in line number %d of Chainfile specified ('%s').\n", i,
                chainfileName);
        exit(1);
    }
    // populate chaindata struct from file
    if (readChainFile(fd, &cd))
    {
        fclose(fd);
        fprintf(stderr, "Error reading chainfile ('%s').\n", chainfileName);
        exit(1);
    }
    fclose(fd);

    dbgPrintChainData(&cd);
    printf("\n");

    // test random SS retrieval
//    for (i = 0; i < 10; i++)
//    {
//        getRandomSS(&cd, &SSaddr, &SSport, &chosenLinkNum);
//        printf("%s,%d,%d\n", SSaddr, SSport, chosenLinkNum);
//    }

    sendURLandChainData (&cd, urlValue, returnMsg);
    //    if (goGetFile(urlValue)) {
    //      DieWithError("Error retreiving URL.\n");
    //    }
    return 0;
}
