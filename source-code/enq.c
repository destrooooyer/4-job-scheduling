#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "job.h"
#define DEBUG
/* 
 * �����﷨��ʽ
 *     enq [-p num] e_file args
 */
void usage()
{
	printf("Usage: enq[-p num] e_file args\n"
		"\t-p num\t\t specify the job priority\n"
		"\te_file\t\t the absolute path of the exefile\n"
		"\targs\t\t the args passed to the e_file\n");
}

int main(int argc,char *argv[])
{
	int p=1;
	int fd;
	char c,*offset;
	struct jobcmd enqcmd;
	//ֻ��һ������ʱ�������ʽ����ȷ
	if(argc==1)
	{
		usage();
		return 1;
	}
	//-p ָ�����ȼ�0~3
	while(--argc>0 && (*++argv)[0]=='-')
	{
		//��Ч�� *(++argv[0])  == *argv[1]
		while(c=*++argv[0])
			switch(c)
		{
			case 'p':
				p=atoi(*(++argv));
				argc--;
				break;
			default:
				printf("Illegal option %c\n",c);
				return 1;
		}
	}

	if(p<1||p>3)
	{
		printf("invalid priority:must between 1 and 3\n");
		return 1;
	}

	//��д��ҵ����
	enqcmd.type=ENQ;
	enqcmd.defpri=p;
	enqcmd.owner=getuid();    //��ҵ�ύ��
	enqcmd.argnum=argc;
	offset=enqcmd.data;		  //��ִ���ļ���

	while (argc-->0)
	{
		strcpy(offset,*argv);
		strcat(offset,":");
		offset=offset+strlen(*argv)+1;
		argv++;
	}

    #ifdef DEBUG
		printf("enqcmd cmdtype\t%d\n"
			"enqcmd owner\t%d\n"
			"enqcmd defpri\t%d\n"
			"enqcmd data\t%s\n",
			enqcmd.type,enqcmd.owner,enqcmd.defpri,enqcmd.data);

    #endif 
		//��ֻд��ʽ���ļ�������ҵ��Ϣд��
		if((fd=open("/tmp/server",O_WRONLY))<0)
			error_sys("enq open fifo failed");

		if(write(fd,&enqcmd,DATALEN)<0)
			error_sys("enq write failed");

		close(fd);
		return 0;
}

