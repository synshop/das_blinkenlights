#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void main(void)
{
  char pidline[1024];
  char *pid;
  int i =0;
  int pidno[64];
  FILE *fp = popen("/bin/pidof blinkenlights","r");
  fgets(pidline,1024,fp);

  pid = strtok (pidline," ");
  while(pid != NULL)
  {
    pidno[i] = atoi(pid);
    printf("%d\n",pidno[i]);
    if(pidno[i])
    {
      kill(pidno[i], SIGUSR1);
    }
    pid = strtok (NULL , " ");
    i++;
  }
  pclose(fp);
}
