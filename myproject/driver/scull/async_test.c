#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

int got_data = 0;
void sighandler_sz(int signo)
{
	printf("%s enter\n", __func__);
	if(SIGIO == signo)
	{
		got_data++;
	}
	return;
}

char buffer[4096];
int main(int argc, char *argv[])
{
	int count;
	struct sigaction action;
	action.sa_handler = sighandler_sz;
	action.sa_flags = 0;

	sigaction(SIGIO, &action, NULL);

	printf("STDIN_FILENO = %d, pid = %d\n", 
		STDIN_FILENO, getpid());
	fcntl(STDIN_FILENO, F_SETOWN, getpid());
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | FASYNC);

	while(1)
	{
		printf("before sleep\n");
		sleep(86400);
		printf("after sleep\n");
		if(!got_data)
		{
			continue;
		}
		count = read(0, buffer, 4096);
		write(1, buffer, 4096);
		got_data = 0;
	}
	
	return 0;
}