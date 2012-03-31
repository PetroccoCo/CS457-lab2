typedef struct chainLink {
  char SSaddr[128];
  uint32_t SSport;
};

struct chainData
{
  uint32_t numLinks;
  struct chainLink links[255];
};

void
DieWithError (char *errorMessage)
{
    perror (errorMessage);
    exit (1);
}

void 
dbgPrintChainData (struct chainData *cd) 
{
    unsigned int i;
    //    printf("<debug>\n<debug>\n");
    //    printf("<debug>size = %d\n",sizeof(struct chainData));
    //    printf("<debug>%d links\n",cd->numLinks);
    printf("Chainlist is\n");
    for (i = 0; i < cd->numLinks; i++) {
      printf("%s,%d\n",cd->links[i].SSaddr,cd->links[i].SSport);
    }
    //    printf("<debug>\n<debug>\n");
}

void
memDump (char *buf, int num) {
  int i;

  FILE *fd = fopen("/tmp/swatts_debug.txt","w");

  fprintf(fd,"<debug>\n");
  for(i=0; i<num; i++) {

    if ((i % 4) == 0) fprintf(fd, "\n<%lX>",&buf[i] - &buf[0]);
    fprintf(fd, "%x...%c...",buf[i],buf[i]);

  }
  fprintf(fd,"<debug>\n");
  fclose(fd);
}
