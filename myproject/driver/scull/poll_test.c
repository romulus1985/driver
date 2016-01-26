#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

static void open_pipe(int *fd, const char *pipe_name)
{
	*fd = open(pipe_name, O_RDWR);
	if(*fd < 0)
	{
		printf("open %s error\n", pipe_name);
		exit(-1);
	}
	printf("%s exit, fd = %d\n", __func__, *fd);
}
int main(int argc, char *argv[])
{
	printf("%s enter.\n", __func__);
	const int count = 4;
	int fds[count];
	int i;
	struct pollfd poll_fd[4];
	char buff[100];
	int retval;
	if(2 != argc 
		|| (0 != strcmp("read", argv[1])
		&& 0 != strcmp("write", argv[1])))
	{
		printf("usage: ./poll_test read|write\n");
		return -1;
	}
	const char* op = argv[1];

	const char *pipes[] = {"/dev/scullp_sz0",
		"/dev/scullp_sz1",
		"/dev/scullp_sz2",
		"/dev/scullp_sz3",
	};

	printf("open pipes\n");
	for(i = 0; i < count; i++)
	{
		open_pipe(&fds[i], pipes[i]);
		poll_fd[i].fd = fds[i];
	}

	for(i = 0; i < count; i++)
	{
		printf("poll_fd[%d].fd = %d\n", i, poll_fd[i].fd);
	}

	printf("set events\n");
	if(0 == strcmp(op, "read"))
	{
		for(i = 0; i < count; i++)
		{
			poll_fd[i].events = POLLIN | POLLRDNORM;
		}
	}
	else
	{
		for(i = 0; i < count; i++)
		{
			poll_fd[i].events = POLLOUT | POLLWRNORM;
		}
	}
	printf("start poll\n");
	retval = poll(poll_fd, 4, 10 * 1000);
	printf("end poll, retval = %d\n", retval);

	if(-1 == retval)
	{
		printf("poll error\n");
		return -1;
	}
	else if(retval)
	{
		if(0 == strcmp("read", op))
		{
			for(i = 0; i < count; i++)
			{
				if(poll_fd[i].revents 
					& (POLLIN | POLLRDNORM))
				{
					printf("%s is readable\n", pipes[i]);
					memset(buff, 0, 100);
					read(fds[i], buff, 100);
					printf("%s\n", buff);
				}
			}
		}
		else
		{
			for(i = 0; i < count; i++)
			{
				if(poll_fd[i].revents
					& (POLLOUT | POLLWRNORM))
				{
					printf("%s is writable\n", pipes[i]);
					
				}
			}
		}
	}
	else
	{
		if(0 == strcmp("read", op))
		{
			printf("No data within ten seconds\n");
		}
		else
		{
			printf("Can not write within ten seconds\n");
		}
	}
	
	return 0;
}
