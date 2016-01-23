#include <sys/types.h>  
#include <sys/stat.h>  
#include <sys/ioctl.h>  
#include <fcntl.h>  
#include <stdio.h>  
#include "scull_ioctl.h" 

#define SCULL_DEVICE_SZ "/dev/scull_dev0"

int main(int argc, char *argv[])
{
	int fd = 0;
	int quantum = 8000;
	int quantum_old = 0;
	int qset = 2000;
	int qset_old = 0;

	fd = open(SCULL_DEVICE_SZ, O_RDWR);

	if(fd < 0)
	{
		printf("open scull device error!\n");
		return 0;
	}

	printf("SCULL_IOC_SQUANTUM_SZ: quantum = %d\n", quantum);
	ioctl(fd, SCULL_IOC_SQUANTUM_SZ, &quantum);
	quantum -= 500;
	printf("SCULL_IOC_TQUANTUM_SZ: quantum = %d\n", quantum);
	ioctl(fd, SCULL_IOC_TQUANTUM_SZ, quantum);

	ioctl(fd, SCULL_IOC_GQUANTUM_SZ, &quantum);
	printf("SCULL_IOC_GQUANTUM_SZ: quantum = %d\n", quantum);
	quantum = ioctl(fd, SCULL_IOC_QQUANTUM_SZ);
	printf("SCULL_IOC_QQUANTUM_SZ: quantum = %d\n", quantum);

	quantum -= 500;
	quantum_old = ioctl(fd, SCULL_IOC_HQUANTUM_SZ, quantum);
	printf("SCULL_IOC_HQUANTUM_SZ: quantum = %d, quantum_old = %d\n",
		quantum, quantum_old);
	quantum -= 500;
	printf("SCULL_IOC_XQUANTUM_SZ: quantum = %d\n", quantum);
	ioctl(fd, SCULL_IOC_XQUANTUM_SZ, &quantum);
	printf("SCULL_IOC_XQUANTUM_SZ: old quantum = %d\n", quantum);

	printf("SCULL_IOC_SQSET_SZ: qset = %d\n", qset);
	ioctl(fd, SCULL_IOC_SQSET_SZ, &qset);
	qset += 500;
	printf("SCULL_IOC_TQSET_SZ: qset = %d\n", qset);
	ioctl(fd, SCULL_IOC_TQSET_SZ, qset);

	ioctl(fd, SCULL_IOC_GQSET_SZ, &qset);
	printf("SCULL_IOC_GQSET_SZ: qset = %d\n", qset);
	qset = ioctl(fd, SCULL_IOC_QQSET_SZ);
	printf("SCULL_IOC_QQSET_SZ: qset = %d\n", qset);

	qset += 500;
	qset_old = ioctl(fd, SCULL_IOC_HQSET_SZ, qset);
	printf("SCULL_IOC_HQSET_SZ: qset = %d, qset_old = %d\n",
		qset, qset_old);
	qset += 500;
	printf("SCULL_IOC_XQSET_SZ: qset = %d\n", qset);
	ioctl(fd, SCULL_IOC_XQSET_SZ, &qset);
	printf("SCULL_IOC_XQSET_SZ: old qset = %d\n", qset);
	return 0;
}