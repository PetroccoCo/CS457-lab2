
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

int
printUsage ()
{
    fprintf (stdout, "Usage: awget <URL> [-c chainfile]\n");
    return 0;
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
    int gotCArg = 0;
    int i;
    int chosenLinkNum;
    int iMaxPortNumber = 65535;

    opterr = 0;

    srand(time(NULL));

    if (argc >= 2) {

      while ((c = getopt (argc, argv, "c:")) != -1)
        {
   	  argValue = optarg;
            switch (c)
              {
              case 'c':
		strcpy(chainfileName,argValue);
		gotCArg = 1;
                break;
              default:
                fprintf (stderr, "Unknown command line option: -%c\n", optopt);
                printUsage ();
                exit (1);
              }
        }
      for (i=optind; i < argc; i++)
        strcpy(urlValue,argv[i]);
    } else {
        fprintf (stderr, "Command line argument '<URL>' is required.\n");
        printUsage ();
        exit (1);
    }

    if (urlValue == "")
      {
          fprintf (stderr, "Command line argument '<URL>' is required.\n");
          printUsage ();
          exit (1);
      }

    return 0;
}
