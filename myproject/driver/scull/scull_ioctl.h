#define SCULL_IOC_MAGIC_SZ 'j'

#define SCULL_IOC_RESET_SZ _IO(SCULL_IOC_MAGIC_SZ, 0)

#define SCULL_IOC_SQUANTUM_SZ _IOW(SCULL_IOC_MAGIC_SZ, 1, int)
#define SCULL_IOC_SQSET_SZ _IOW(SCULL_IOC_MAGIC_SZ, 2, int)
#define SCULL_IOC_TQUANTUM_SZ _IO(SCULL_IOC_MAGIC_SZ, 3)
#define SCULL_IOC_TQSET_SZ _IO(SCULL_IOC_MAGIC_SZ, 4)

#define SCULL_IOC_GQUANTUM_SZ _IOR(SCULL_IOC_MAGIC_SZ, 5, int)
#define SCULL_IOC_GQSET_SZ _IOR(SCULL_IOC_MAGIC_SZ, 6, int)
#define SCULL_IOC_QQUANTUM_SZ _IO(SCULL_IOC_MAGIC_SZ, 7)
#define SCULL_IOC_QQSET_SZ _IO(SCULL_IOC_MAGIC_SZ, 8)

#define SCULL_IOC_XQUANTUM_SZ _IOWR(SCULL_IOC_MAGIC_SZ, 9, int)
#define SCULL_IOC_XQSET_SZ _IOWR(SCULL_IOC_MAGIC_SZ, 10, int)
#define SCULL_IOC_HQUANTUM_SZ _IO(SCULL_IOC_MAGIC_SZ, 11)
#define SCULL_IOC_HQSET_SZ _IO(SCULL_IOC_MAGIC_SZ, 12)

#define SCULL_P_IOC_TSIZE_SZ _IO(SCULL_IOC_MAGIC_SZ, 13)
#define SCULL_P_IOC_QSIZE_SZ _IO(SCULL_IOC_MAGIC_SZ, 14)

#define SCULL_IOC_MAXNR_SZ 14