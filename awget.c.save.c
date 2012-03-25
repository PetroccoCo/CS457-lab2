
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

char *serverName = NULL;        /* -s servername */


void chainDataAndURLToString(struct chainData *cd, char *urlValue, char **cdString) {
    int i;
    int thisSize = 0;
    char thisLine[255];

    thisSize = 6*sizeof(char) + strlen(urlValue)*sizeof(char);
    *cdString = malloc(thisSize);
    strcpy(*cdString,urlValue);
    strcat(*cdString,"\n");
    sprintf(thisLine,"%d\n",cd->numLinks);
    strcat(*cdString,thisLine);

    for (i = 0; i < cd->numLinks; i++) {
      thisSize += 7*sizeof(char) + strlen(cd->links[i].SSaddr) * sizeof(char);
      *cdString = realloc(*cdString,thisSize);

      sprintf(thisLine,"%s,%d\n",cd->links[i].SSaddr,cd->links[i].SSport);
      strcat(*cdString,thisLine);
    }
}


void removeLinkFromChainData(struct chainData *cd, int linkNum) {
    int i;
    for (i = 0; i < cd->numLinks; i++) {
      if (i >= linkNum) {
	strcpy(cd->links[i].SSaddr,cd->links[i+1].SSaddr);
	cd->links[i].SSport = cd->links[i+1].SSport;
      }
    }
    cd->numLinks = cd->numLinks - 1;
}

void getRandomSS (struct chainData *cd, char **SSaddr, int *SSport, int *linkNumChosen) {
  int i = 0;
  i = rand() % cd->numLinks;

  *SSaddr = cd->links[i].SSaddr;
  *SSport = cd->links[i].SSport;
  *linkNumChosen = i;
}

void packChainData (struct chainData *cd_in, struct chainData *cd_out) {
    int i;
    cd_out->numLinks = cd_in->numLinks;
    for (i = 0; i < cd_in->numLinks; i++) {
	strcpy(cd_out->links[i].SSaddr,cd_in->links[i].SSaddr);
	cd_out->links[i].SSport = htonl(cd_in->links[i].SSport);
    }
  }


int
printUsage ()
{
    fprintf (stdout, "Usage: awget <URL> [-c chainfile]\n");
    return 0;
}

int readChainFile(FILE *fd, struct chainData *cd) {

  char buf[255];
  char *token = NULL;
  int i;
  int iRead = 0;

  /* read and verify the integer on line 1 of the file */
  if (fgets(buf,255,fd) == NULL) {
    return 1;
  } else {
    if ((iRead = atoi(buf)) == 0) {
      return 1;
    }
  }
  cd->numLinks = iRead;

  for (i = 0; i < cd->numLinks; i++) {
    if (fgets(buf,255,fd) == NULL) {
      return 1;
    } else {
      token = strtok(buf,",");
      strcpy(cd->links[i].SSaddr,token);
      token = strtok(NULL,",");
      cd->links[i].SSport = atoi(token);
    }
  }

  return 0;
}

int sanityCheckFile (char *fname) {

  char buf[255];
  char SSaddr[255];
  char SSport[255];
  char *token = NULL;
  int i,j;
  char *comma = NULL;
  int commaPosition = 0;
  int sLen;
  int iRead = 0;
  int jRead = 0;

  FILE *fd = fopen(fname,"r");

  /* read and verify the integer on line 1 of the file */
  if (fgets(buf,255,fd) == NULL) {
    return 1;
  } else {
    if (strcmp("0",buf) != 0) {
      if ((iRead = atoi(buf)) == 0) {
        return 1;
      }
    }
  }

  for (i = 0; i < iRead; i++) {
    if (fgets(buf,255,fd) == NULL) {
      return(2+i);
    } else {
      sLen = strlen(buf);
      comma = strchr(buf,',');
      commaPosition = comma - buf;
      if ((commaPosition < 1) || (commaPosition > (sLen - 1))) {
	return (2+i);
      }
      token = strtok(buf,",");
      strcpy(SSaddr,token);
      token = strtok(NULL,",");
      strcpy(SSport,token);

      for (j=0; j<strlen(SSport)-1; j++) {
	if (!isdigit(SSport[j])) {
	  return (2+i);
	}
      }
      if ((jRead = atoi(SSport)) == 0) {
        return (2+i);
      }
      if ((jRead <= 0) || (jRead > MAXPORTNUMBER)) {
	DieWithError("Invalid port number.");
      }
    }
  }

  fclose(fd);

  return 0;
}

void
makeStream(char **buf, struct chainData *cd, char *urlValue) {

  *buf = malloc (255*sizeof(char) * sizeof(struct chainData));
  if (*buf == NULL) DieWithError("Malloc error.");

  memcpy(*buf,cd,sizeof(struct chainData));
  memcpy(*buf+sizeof(struct chainData),urlValue,255);

}

void
sendURLandChainData (struct chainData *cd, char *urlValue)
{
    int sock;                   /* Socket descriptor */
    struct sockaddr_in myServAddr;      /* server address */
    unsigned short myServPort;  /* server port */
    char rcvBuffer;             /* Buffer for reply string */
    int bytesRcvd, totalBytesRcvd;      /* Bytes read in single recv()
                                           and total bytes read */
    struct hostent *hostentStruct;      /* helper struct for gethostbyname() */
    char *serverName;
    char *SSaddr;
    int SSport,portNumber;
    int i;
    char *buf;
    int chosenLinkNum;
    int ipAddressEntered = 1;       /* flag to indicate an IP address was entered as -s arg */
    struct chainData cdSend;

    getRandomSS(cd,&SSaddr,&SSport,&chosenLinkNum);
    /* if all digits and periods, then IP address was entered */
    for (i = 0; i < strlen (SSaddr); i++)
      {
        if (SSaddr[i] != '.')
          {
            if (isdigit (SSaddr[i]) == 0)
              {
                ipAddressEntered = 0;
              }
          }
      }
    serverName = SSaddr;
    portNumber = SSport;

    /* Create a stream socket using TCP */
    if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError ("socket() failed");

    /* establish 3 seconds as the timeout value */
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof tv))
        DieWithError ("setsockopt() failed");

    /* Construct the server address structure */
    memset (&myServAddr, 0, sizeof (myServAddr));       /* Zero out structure */
    myServAddr.sin_family = AF_INET;    /* Internet address family */
    if (ipAddressEntered)
      {
          /* load the address */
          myServAddr.sin_addr.s_addr = inet_addr (serverName);
      }
    else
      {
          /* if a hostname was entered, we need to look for IP address */
          hostentStruct = gethostbyname (serverName);
          if (hostentStruct == NULL)
              DieWithError ("gethostbyname() failed");

          /* get the correct "format" of the IP address */
          serverName = inet_ntoa (*(struct in_addr *) (hostentStruct->h_addr_list[0]));
          if (serverName == NULL)
            {
                fprintf (stderr, "inet_ntoa() failed.\n");
                exit (1);
            }

          /* load the address */
          memcpy (&myServAddr.sin_addr, hostentStruct->h_addr_list[0], hostentStruct->h_length);
      }
    /* load the port */
    myServAddr.sin_port = htons (portNumber);   /* Server port */

    /* Establish the connection to the server */
    if (connect (sock, (struct sockaddr *) &myServAddr, sizeof (myServAddr)) < 0)
        DieWithError ("connect() failed");

    /* Send the struct to the server */
    chainDataAndURLToString(cd,urlValue,&buf);
    
    if (send (sock, buf, strlen(buf), 0) != strlen(buf))
        DieWithError ("send() sent a different number of bytes than expected");

    /* let everyone know the URL was sent */
    fprintf (stdout, "send buffer size = %d\n", strlen(buf));
    fprintf (stdout, "Sent chainfile contents to server %s:%d via TCP\n", serverName, portNumber);
    fprintf (stdout, "(sent)*****\n%s\n*****\n", buf);
    free(buf);

    /* sent the bytes to server */
    /* so close things up */
    if (close (sock) != 0)
        DieWithError ("close() failed");
    fprintf (stderr, "Send was a Success - exiting.\n");
    exit (0);
}

int
main (int argc, char **argv)
{
    struct chainData cd;
    char urlValue[255] = "";
    char *argValue = NULL;
    char *SSaddr;
    int SSport;
    char chainfileName[255] = "./chaingang.txt";
    int c;
    int i;
    int chosenLinkNum;
    int iMaxPortNumber = 65535;

    opterr = 0;

    srand(time(NULL));

    while ((c = getopt (argc, argv, "c:")) != -1)
      {
	argValue = optarg;
          switch (c)
            {
            case 'c':
		strcpy(chainfileName,argValue);
                break;
            default:
                fprintf (stderr, "Unknown command line option: -%c\n", optopt);
                printUsage ();
                exit (1);
            }
      }
    for (i=optind; i < argc; i++)
      strcpy(urlValue,argv[i]);

    if (urlValue == "")
      {
          fprintf (stderr, "Command line argument '<URL>' is required.\n");
          printUsage ();
          exit (1);
      }

    FILE *fd = fopen(chainfileName,"r");
    if (fd == NULL) {
        fprintf (stderr, "Error opening chainfile specified ('%s').\n",chainfileName);
        exit (1);
    }
    if ((i = sanityCheckFile(chainfileName)) != 0) {
      fprintf (stderr, "Error in line number %d of Chainfile specified ('%s').\n",i,chainfileName);
        exit (1);
    }
    if (readChainFile(fd,&cd)) {
      fclose(fd);
      fprintf (stderr, "Error reading chainfile ('%s').\n",chainfileName);
      exit (1);
    }
    fclose(fd);

    dbgPrintChainData(&cd);

    printf("\n");
    for (i=0; i< 10; i++) {
      getRandomSS(&cd,&SSaddr,&SSport,&chosenLinkNum);
      printf("%s,%d,%d\n",SSaddr,SSport,chosenLinkNum);
    }

    sendURLandChainData (&cd, urlValue);

  //    if (goGetFile(urlValue)) {
  //      DieWithError("Error retreiving URL.\n");
  //    }

    return 0;
}
