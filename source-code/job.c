#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include "job.h"

int jobid=0;
int siginfo=1;
int fifo;
int globalfd;

/*
���һ��next�Ͱ�����ԭ������ɾ�����ȵ��´�switch()�ٷŻ�ȥ
*/



/**************************************************************************************************/
int jobk = -2; //������¼��ҵ��ʼִ�к���ͣ����    
/*
����׼����0��ʼ��ʱ��������jobk��0����ҵ���������Ѿ�����1s��
������������ҵ������ȼ�Ϊ3�Ļ���û��ʼ�ͽ����ˣ�������jobk��-1��ʼ��ʱ����1s��׼��ʱ��
��Ϊscheduler()������ִ��Ҳ��Ҫ����1s��ʱ�䣬����������һ��ʱ��2s��
*/
int mark = 0; //������¼��ǰ��û����ҵ��ִ�У�0û�У�1��
int nowjobqueuenumber = 0; //����ִ����ҵ���ڵĶ��У��ӵ͵��� 1,2,3
int nowjobpri = 0; //����ִ����ҵ���ȼ�

int goon = 0; //������̼�ͨ������

void setGoon(){
	goon++;
}
/**************************************************************************************************/

/**************************************************************************************************/
struct waitqueue *highhead=NULL;
struct waitqueue *middlehead=NULL;
struct waitqueue *lowhead=NULL;

struct waitqueue *newinjob=NULL; //������ҵ��ռ
/**************************************************************************************************/


//struct waitqueue *head=NULL;
struct waitqueue *next=NULL,*current =NULL;

/* ���ȳ��� */
void scheduler()
{
	struct waitqueue *p,*prev,*select,*selectprev;
	struct jobinfo *newjob=NULL;
	struct jobcmd cmd;
	int  count = 0;
	//���cmd�е�����
	bzero(&cmd,DATALEN);
	//��fifo�ļ��ж�ȡһ����ҵ������cmd��
	if((count=read(fifo,&cmd,DATALEN))<0)
		error_sys("read fifo failed");

//��ӡ��ҵ��Ϣ
#ifdef DEBUG
	if(count){
		printf("cmd cmdtype\t%d\ncmd defpri\t%d\ncmd data\t%s\n",cmd.type,cmd.defpri,cmd.data);
	}
	else
		printf("no data read\n");
#endif

	//  ���µȴ������е���ҵ�ĵȴ�ʱ�䣬���ȼ�������������ҵ������ʱ�� 
	 updateall();
	switch(cmd.type){
	case ENQ:
		do_enq(newjob,cmd);
		
/**************************************************************************************************/
		goon = 0;  //ֻ����Ӻ����Ͼ�Ҫִ�е��ӽ��̲ſ��ܻ�û�������
		if(newinjob->job->curpri > nowjobpri){
			//�л���ҵ������ԭ��ҵ���ȼ������У���״̬
			next = newinjob;
			//���¶�ȡ����ҵ�Ӷ�����ȥ��
			select = NULL;
			selectprev = NULL;
			switch(newinjob->job->queuenumber){
				case 1:
					for(prev=lowhead,p=lowhead;p!=NULL;prev=p,p=p->next)
						if(p->job->jid==newinjob->job->jid){
							select=p;
							selectprev=prev;
							break;
					}
					selectprev->next=select->next;
					if(select==selectprev)
						lowhead=lowhead->next;
					break;
				case 2:
					for(prev=middlehead,p=middlehead;p!=NULL;prev=p,p=p->next)
						if(p->job->jid==newinjob->job->jid){
							select=p;
							selectprev=prev;
							break;
					}
					selectprev->next=select->next;
					if(select==selectprev)
						middlehead=middlehead->next;
					
					break;
				case 3:
					for(prev=highhead,p=highhead;p!=NULL;prev=p,p=p->next)
						if(p->job->jid==newinjob->job->jid){
							select=p;
							selectprev=prev;
							break;
					}
					selectprev->next=select->next;
					if(select==selectprev)
						highhead=highhead->next;
					
					break;
			}
			next->next = NULL;
			jobswitch(); 
		}
/**************************************************************************************************/
		break;
	case DEQ:
		do_deq(cmd);
		break;
	case STAT:
		do_stat(cmd);
		break;
	default:
		break;
	}

/**************************************************************************************************/
	//jobk >= 0˵���µ���ҵ��ʼִ����
	if(jobk >= -1){
		jobk++;
		
	}

	if((jobk%1 == 0 && jobk > 0) || mark == 0){
		if(nowjobqueuenumber == 3){
			
			next=jobselect();
			jobswitch();
		}
	}
	if((jobk%2 == 0 && jobk > 0) || mark == 0){
		if(nowjobqueuenumber == 2){
			next=jobselect();
			jobswitch();
		}
	}
	if((jobk%5 == 0 && jobk > 0) || mark == 0){
		if(nowjobqueuenumber == 1){
			
			next=jobselect();
			jobswitch();
		}
	}
/**************************************************************************************************/





	// /* ѡ������ȼ���ҵ */
	// next=jobselect();
	// /* ��ҵ�л� */
	// jobswitch();
}

int allocjid()
{
	return ++jobid;
}

void updateall()
{
	struct waitqueue *p,*prev,*temp;

	/* ������ҵ����ʱ�� */
	if(current)
		current->job->run_time += 1; /* ��1����1000ms */

	/* ������ҵ�ȴ�ʱ�估���ȼ�,ÿ��5000ms���ȼ���1 */
	// for(p = head; p != NULL; p = p->next){
	// 	p->job->wait_time += 1000;
	// 	if(p->job->wait_time >= 5000 && p->job->curpri < 3){
	// 		p->job->curpri++;
	// 		p->job->wait_time = 0;
	// 	}
	// }
/**************************************************************************************************/
		//��������������Ϣ
		for(p = highhead; p != NULL; p = p->next){
			p->job->wait_time += 1000;
		}
		for(prev= middlehead,p = middlehead; p != NULL;){
			p->job->wait_time += 1000; 
			if(p->job->wait_time >= 5000){  //�л�����
				p->job->curpri++;
				/*****************************/
				p->job->queuenumber++;
				if(prev == p){
					middlehead = middlehead->next;
					if(highhead){
						for(temp = highhead; temp->next != NULL; temp = temp->next);
							temp->next = p;
					}else{
						highhead = p;
					}
					p->job->wait_time = 0;
					p->next = NULL;
					p = middlehead;
					prev = middlehead;
					
				}
				else{
					prev->next = p->next;
					if(highhead){
						for(temp = highhead; temp->next != NULL; temp = temp->next);
							temp->next = p;
					}else{
						highhead = p;
					}
					p->job->wait_time = 0;
					p->next = NULL;
					p = prev->next;
				}
			}
			else{
				prev = p;
				p = p->next;
			}
		}
		for(prev = lowhead,p = lowhead; p != NULL;){
			p->job->wait_time += 1000;
			if(p->job->wait_time >= 5000){  //�л�����
				p->job->curpri++;
				/*****************************/
				p->job->queuenumber++;
				if(prev == p){
					lowhead = lowhead->next;
					if(middlehead){
						for(temp = middlehead; temp->next != NULL; temp = temp->next);
							temp->next = p;
					}else{
						middlehead = p;
					}
					p->job->wait_time = 0;
					p->next = NULL;
					p = lowhead;
					prev = lowhead;
				}
				else{
					prev->next = p->next;
					if(middlehead){
						for(temp = middlehead; temp->next != NULL; temp = temp->next);
							temp->next = p;
					}else{
						middlehead = p;
					}
					p->job->wait_time = 0;
					p->next = NULL;
					p = prev->next;
				}
				
			}
			else{
				prev = p;
				p = p->next;
			}
		}
/**************************************************************************************************/
}

struct waitqueue* jobselect()
{
	struct waitqueue *p,*prev,*select,*selectprev;
	int highest = -1;

	select = NULL;
	selectprev = NULL;
	// if(head){
	// 	/* �����ȴ������е���ҵ���ҵ����ȼ���ߵ���ҵ */
	// 	for(prev = head, p = head; p != NULL; prev = p,p = p->next)
	// 		if(p->job->curpri > highest){
	// 			select = p;
	// 			selectprev = prev;
	// 			highest = p->job->curpri;
	// 		}
	// 		selectprev->next = select->next;
	// 		if (select == selectprev)
	// 			head = NULL;
	// }
/*********************************************************************************/
	if(highhead){
		
		select = highhead;
		highhead = highhead->next;
		select->next = NULL;
	}
	else if(middlehead){
		select = middlehead;
		middlehead = middlehead->next;
		select->next = NULL;
	}
	else if(lowhead){
		
		select = lowhead;
		lowhead = lowhead->next;
		select->next = NULL;
	}
/*********************************************************************************/
	return select;
}

void jobswitch()
{
	
	struct waitqueue *p;
	int i;

/*********************************************************************************/
	if(next != NULL){
		jobk = -1;
		mark = 1;  //�Ѿ���ʼִ����ҵ��
	}
/*********************************************************************************/


	if(current && current->job->state == DONE){ /* ��ǰ��ҵ��� */
		/* ��ҵ��ɣ�ɾ���� */
		for(i = 0;(current->job->cmdarg)[i] != NULL; i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i] = NULL;
		}
		/* �ͷſռ� */
		free(current->job->cmdarg);
		free(current->job);
		free(current);
		current = NULL;
	}

	if(next == NULL && current == NULL){ /* û����ҵҪ���� */
/*********************************************************************/
		jobk = -2; 
		mark = 0;   //��û����ҵ��
		nowjobqueuenumber = 0;
/*********************************************************************/
		return;
	}
	else if (next != NULL && current == NULL){ /* ��ʼ�µ���ҵ */

		printf("begin start new job\n");
		current = next;
		next = NULL;
		current->job->state = RUNNING;
/********************************************/
		//��¼����ִ����ҵ�����ȼ��Ͷ���
		nowjobqueuenumber = current->job->queuenumber;
		nowjobpri = current->job->curpri;
		while(goon < 1){
			jobk = -1;
		};
/********************************************/
		kill(current->job->pid,SIGCONT);
		return;
	}
	else if (next != NULL && current != NULL){ /* �л���ҵ */

		printf("switch to Pid: %d\n",next->job->pid);
/********************************************/
		while(goon < 1){
			jobk = -1;
		};
/********************************************/
		kill(current->job->pid,SIGSTOP);
		//current->job->curpri = current->job->defpri;
/*******************************************************************/
		//�������ȼ�
		if(current->job->curpri>1){
			current->job->curpri = current->job->curpri-1;
		}
/*******************************************************************/
		current->job->wait_time = 0;
		current->job->state = READY;

		/* �Żصȴ����У��ŵ��ȴ����е���� */
		// if(head){
		// 	for(p = head; p->next != NULL; p = p->next);
		// 	p->next = current;
		// }else{
		// 	head = current;
		// }
/**************************************************************************************************/
		//��current�Żض�����
		if(current->job->queuenumber == 3){
			current->job->queuenumber = 2;
			if(middlehead){
				for(p = middlehead; p->next != NULL; p = p->next);
					p->next = current;
			}else{
					printf("in\n");
					middlehead = current;
			}
			current->next = NULL;
			
		}
		else if(current->job->queuenumber == 2){
			current->job->queuenumber = 1;
			if(lowhead){
				for(p = lowhead; p->next != NULL; p = p->next);
					p->next = current;
			}else{
					lowhead = current;
			}
			current->next = NULL;
			
		}
		else{
			if(lowhead){
				for(p = lowhead; p->next != NULL; p = p->next);
					p->next = current;
			}else{
					lowhead = current;
			}
			current->next = NULL;
		}
/**************************************************************************************************/

		current = next;
		next = NULL;
		current->job->state = RUNNING;
		current->job->wait_time = 0;
/*******************************************************************/
		//��¼����ִ����ҵ�����ȼ��Ͷ���
		nowjobqueuenumber = current->job->queuenumber;
		nowjobpri = current->job->curpri;
/*******************************************************************/
		kill(current->job->pid,SIGCONT);
		
		return;
	}else{ /* next == NULL��current != NULL�����л� */
		return;
	}
}

void sig_handler(int sig,siginfo_t *info,void *notused)
{
	int status;
	int ret;

	switch (sig) {
case SIGVTALRM: /* �����ʱ�������õļ�ʱ��� */
	//ʱ��Ƭ���꣬��ʼһ�ε���
	scheduler();
	return;
case SIGCHLD: /* �ӽ��̽���ʱ���͸������̵��ź� */
	//�ȴ�����һ���ӽ��̽��������ѽ���״̬������status��
	ret = waitpid(-1,&status,WNOHANG);
	if (ret == 0)
		return;
	//�����������ط�0ֵ
	if(WIFEXITED(status)){
		current->job->state = DONE;
		printf("normal termation, exit status = %d\n",WEXITSTATUS(status));
	}
	//���źŶ�����������
	else if (WIFSIGNALED(status)){
		printf("abnormal termation, signal number = %d\n",WTERMSIG(status));
	}
	//�ӽ�����ͣʱ������
	else if (WIFSTOPPED(status)){
		printf("child stopped, signal number = %d\n",WSTOPSIG(status));
	}
	return;
	default:
		return;
	}
}

void do_enq(struct jobinfo *newjob,struct jobcmd enqcmd)
{
	struct waitqueue *newnode,*p;
	int i=0,pid;
	char *offset,*argvec,*q;
	char **arglist;
	sigset_t zeromask;

	sigemptyset(&zeromask);

	/* ��װjobinfo���ݽṹ */
	newjob = (struct jobinfo *)malloc(sizeof(struct jobinfo));
	newjob->jid = allocjid();
	newjob->defpri = enqcmd.defpri;
	newjob->curpri = enqcmd.defpri;
	newjob->ownerid = enqcmd.owner;
	newjob->state = READY;
	newjob->create_time = time(NULL);
	newjob->wait_time = 0;
	newjob->run_time = 0;
	newjob->queuenumber = enqcmd.defpri;
	arglist = (char**)malloc(sizeof(char*)*(enqcmd.argnum+1));
	newjob->cmdarg = arglist;
	offset = enqcmd.data;
	argvec = enqcmd.data;
	while (i < enqcmd.argnum){
		if(*offset == ':'){
			*offset++ = '\0';
			q = (char*)malloc(offset - argvec);
			strcpy(q,argvec);
			arglist[i++] = q;
			argvec = offset;
		}else
			offset++;
	}

	arglist[i] = NULL;

#ifdef DEBUG

	printf("enqcmd argnum %d\n",enqcmd.argnum);
	for(i = 0;i < enqcmd.argnum; i++)
		printf("parse enqcmd:%s\n",arglist[i]);

#endif

	/*��ȴ������������µ���ҵ*/
	newnode = (struct waitqueue*)malloc(sizeof(struct waitqueue));
	newnode->next =NULL;
	newnode->job=newjob;
/******************************************/
	newinjob = newnode;  //��¼�¸ս�����ҵ�ڵȴ������е�ָ�룬�Ա���ռʱʹ��
/******************************************/
	// if(head)
	// {
	// 	for(p=head;p->next != NULL; p=p->next);
	// 	p->next =newnode;
	// }else
	// 	head=newnode;

/**************************************************************************************************/
	//����ҵ����ӵ�������
	switch(newjob->curpri){
		case 1:
			if(lowhead){
				for(p=lowhead;p->next != NULL; p=p->next);
				p->next =newnode;
			}else
				lowhead=newnode;
			break;
		case 2:
			if(middlehead){
				for(p=middlehead;p->next != NULL; p=p->next);
				p->next =newnode;
			}else
				middlehead=newnode;
			break;
		case 3:
			if(highhead){
				for(p=highhead;p->next != NULL; p=p->next);
				p->next =newnode;
			}else
				highhead=newnode;
			break;
	}
/**************************************************************************************************/
	/*Ϊ��ҵ��������*/
	if((pid=fork())<0)
		error_sys("enq fork failed");

	if(pid==0){
		newjob->pid =getpid();
		/*�����ӽ���,�ȵ�ִ��*/
		//�����Գ�����ָ�����ź�
	/*************************************************/
		kill(getppid(), SIGUSR1);
	/*************************************************/
		raise(SIGSTOP);
#ifdef DEBUG

		printf("begin running\n");
		for(i=0;arglist[i]!=NULL;i++)
			printf("arglist %s\n",arglist[i]);
#endif

		/*�����ļ�����������׼���*/
		//����Ͱѱ�׼���1��ָ�븳����globalfd,1�Ͳ�����
		dup2(globalfd,1);
		/* ִ������ */
		if(execv(arglist[0],arglist)<0)
			printf("exec failed\n");
		exit(1);
	}else{
		newjob->pid=pid;
	}
}

void do_deq(struct jobcmd deqcmd)
{
	int deqid,i;
	struct waitqueue *p,*prev,*select,*selectprev;
	deqid=atoi(deqcmd.data);

#ifdef DEBUG
	printf("deq jid %d\n",deqid);
#endif

	/*current jodid==deqid,��ֹ��ǰ��ҵ*/
	if (current && current->job->jid ==deqid){
		printf("teminate current job\n");
		kill(current->job->pid,SIGKILL);
		for(i=0;(current->job->cmdarg)[i]!=NULL;i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i]=NULL;
		}
		free(current->job->cmdarg);
		free(current->job);
		free(current);
		current=NULL;
	}
	else{ /* �����ڵȴ������в���deqid */
		select=NULL;
		selectprev=NULL;
		// if(head){
		// 	for(prev=head,p=head;p!=NULL;prev=p,p=p->next)
		// 		if(p->job->jid==deqid){
		// 			select=p;
		// 			selectprev=prev;
		// 			break;
		// 		}
		// 		selectprev->next=select->next;
		// 		if(select==selectprev)
		// 			head=NULL;
		// }
/**************************************************************************************************/
		//�����������в���ɾ��
		if(highhead){
			for(prev=highhead,p=highhead;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				selectprev->next=select->next;
				if(select==selectprev && select!=NULL)
					highhead=highhead->next;
		}
		if(select==NULL){
			if(middlehead){
				for(prev=middlehead,p=middlehead;p!=NULL;prev=p,p=p->next)
					if(p->job->jid==deqid){
						select=p;
						selectprev=prev;
						break;
					}
					selectprev->next=select->next;
					if(select==selectprev && select!=NULL)
						middlehead=middlehead->next;
			}
		}
		if(select==NULL){
			if(lowhead){
				for(prev=lowhead,p=lowhead;p!=NULL;prev=p,p=p->next)
					if(p->job->jid==deqid){
						select=p;
						selectprev=prev;
						break;
					}
					selectprev->next=select->next;
					if(select==selectprev && select!=NULL)
						lowhead=lowhead->next;
			}
		}
/**************************************************************************************************/
		if(select){
			for(i=0;(select->job->cmdarg)[i]!=NULL;i++){
				free((select->job->cmdarg)[i]);
				(select->job->cmdarg)[i]=NULL;
			}
			free(select->job->cmdarg);
			free(select->job);
			free(select);
			select=NULL;
		}
	}
}

void do_stat(struct jobcmd statcmd)
{
	struct waitqueue *p;
	char timebuf[BUFLEN];
	/*
	*��ӡ������ҵ��ͳ����Ϣ:
	*1.��ҵID
	*2.����ID
	*3.��ҵ������
	*4.��ҵ����ʱ��
	*5.��ҵ�ȴ�ʱ��
	*6.��ҵ����ʱ��
	*7.��ҵ״̬
	*/

	/* ��ӡ��Ϣͷ�� */
	printf("JOBID\tPID\tOWNER\tRUNTIME\tWAITTIME\tCREATTIME\t\tSTATE\n");
	if(current){
		strcpy(timebuf,ctime(&(current->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			current->job->jid,
			current->job->pid,
			current->job->ownerid,
			current->job->run_time,
			current->job->wait_time,
			timebuf,"RUNNING");
	}

	// for(p=head;p!=NULL;p=p->next){
	// 	strcpy(timebuf,ctime(&(p->job->create_time)));
	// 	timebuf[strlen(timebuf)-1]='\0';
	// 	printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
	// 		p->job->jid,
	// 		p->job->pid,
	// 		p->job->ownerid,
	// 		p->job->run_time,
	// 		p->job->wait_time,
	// 		timebuf,
	// 		"READY");
	// }
/**************************************************************************************************/
	for(p=highhead;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}
	for(p=middlehead;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}
	for(p=lowhead;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}
/**************************************************************************************************/
}

int main()
{
	struct timeval interval;
	struct itimerval new,old;
	struct stat statbuf;
	struct sigaction newact,oldact1,oldact2;

	if(stat("/tmp/server",&statbuf)==0){
		/* ���FIFO�ļ�����,ɾ�� */
		if(remove("/tmp/server")<0)
			error_sys("remove failed");
	}
    //����FIFO�ļ�
	if(mkfifo("/tmp/server",0666)<0)
		error_sys("mkfifo failed");
	/* �ڷ�����ģʽ�´�FIFO */
	if((fifo=open("/tmp/server",O_RDONLY|O_NONBLOCK))<0)
		error_sys("open fifo failed");

	/* �����źŴ����� */

	signal(SIGUSR1, setGoon);

	//ʹ��sig_handler()��������Ӧ��Щ�ź�
	newact.sa_sigaction=sig_handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags=SA_SIGINFO;
	//������ͣ���ж�ʱ����SIGCHLD�ź�
	sigaction(SIGCHLD,&newact,&oldact1);
	//setitimer�������õ�Virtual Interval Timer��ʱ��ʱ�����SIGVTALRM
	sigaction(SIGVTALRM,&newact,&oldact2);
	/*
	ÿ���������ͣ�»�ʱ��Ƭ����ʱ��Ҫ���µ���һ��sig_handler()
	*/


	/* ����ʱ����Ϊ1000���� */
	interval.tv_sec=1;
	interval.tv_usec=0;

	new.it_interval=interval;		//ָ����ʱ��ʱ����
	new.it_value=interval;			//ָ����ʱ����ʼʱ��
	setitimer(ITIMER_VIRTUAL,&new,&old);
	//// ����ִ�е��Ⱥ���ʱ����ʱ�ˣ�����ôִ�е�

	while(siginfo==1);

	close(fifo);
	close(globalfd);
	return 0;
}
