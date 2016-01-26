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
	fd_set rfds, wfds;
	
	const int count = 4;
	int fds[count], i;

	char buff[100];
	int retval;

	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if(2 != argc
		|| (0 != strcmp("read", argv[1])
		&& 0 != strcmp("write", argv[1])))
	{
		printf("usage: ./select_test read|write\n");
		return -1;
	}
	const char* op = argv[1];

	const char *pipes[] = {"/dev/scullp_sz0",
		"/dev/scullp_sz1",
		"/dev/scullp_sz2",
		"/dev/scullp_sz3",
	};

	for(i = 0; i < count; i++)
	{
		open_pipe(fds + i, pipes[i]);
	}

	if(0 == strcmp("read", op))
	{
		FD_ZERO(&rfds);
		for(i = 0; i < count; i++)
		{
			FD_SET(fds[i], &rfds);
		}
		retval = select(fds[count -1] + 1, &rfds, NULL, NULL, &tv);
	}
	else
	{
		FD_ZERO(&wfds);
		for(i = 0; i < count; i++)
		{
			FD_SET(fds[i], &wfds);
		}
		retval = select(fds[count - 1] + 1, NULL, &wfds, NULL, &tv);
	}

	if(-1 == retval)
	{
		printf("select error\n");
		return -1;
	}
	else if(retval)
	{
		if(0 == strcmp("read", op))
		{
			for(i = 0; i < count; i++)
			{
				if(FD_ISSET(fds[i], &rfds))
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
				if(FD_ISSET(fds[i], &wfds))
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
