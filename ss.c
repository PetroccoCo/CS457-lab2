
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

    return 0;

}
