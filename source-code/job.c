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
求出一个next就把它从原队列中删除，等到下次switch()再放回去
*/



/**************************************************************************************************/
int jobk = -2; //用来记录作业开始执行后，暂停次数    
/*
本来准备从0开始计时，不过从jobk赋0到作业启动几乎已经过了1s，
所以启动的作业如果优先级为3的话还没开始就结束了，所以让jobk从-1开始计时，多1s的准备时间
因为scheduler()函数的执行也需要将近1s的时间，所以真正记一次时是2s。
*/
int mark = 0; //用来记录当前有没有作业在执行，0没有，1有
int nowjobqueuenumber = 0; //正在执行作业所在的队列，从低到高 1,2,3
int nowjobpri = 0; //正在执行作业优先级

int goon = 0; //解决进程间通信问题

void setGoon(){
	goon++;
}
/**************************************************************************************************/

/**************************************************************************************************/
struct waitqueue *highhead=NULL;
struct waitqueue *middlehead=NULL;
struct waitqueue *lowhead=NULL;

struct waitqueue *newinjob=NULL; //用于作业抢占
/**************************************************************************************************/


//struct waitqueue *head=NULL;
struct waitqueue *next=NULL,*current =NULL;

/* 调度程序 */
void scheduler()
{
	struct waitqueue *p,*prev,*select,*selectprev;
	struct jobinfo *newjob=NULL;
	struct jobcmd cmd;
	int  count = 0;
	//清除cmd中的数据
	bzero(&cmd,DATALEN);
	//从fifo文件中读取一个作业，放入cmd中
	if((count=read(fifo,&cmd,DATALEN))<0)
		error_sys("read fifo failed");

//打印作业信息
#ifdef DEBUG
	if(count){
		printf("cmd cmdtype\t%d\ncmd defpri\t%d\ncmd data\t%s\n",cmd.type,cmd.defpri,cmd.data);
	}
	else
		printf("no data read\n");
#endif

	//  更新等待队列中的作业的等待时间，优先级和正在运行作业的运行时间 
	 updateall();
	switch(cmd.type){
	case ENQ:
		do_enq(newjob,cmd);
		
/**************************************************************************************************/
		goon = 0;  //只有入队后马上就要执行的子进程才可能还没创建完成
		if(newinjob->job->curpri > nowjobpri){
			//切换作业，更改原作业优先级，队列，和状态
			next = newinjob;
			//将新读取的作业从队列中去掉
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
	//jobk >= 0说明新的作业开始执行了
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





	// /* 选择高优先级作业 */
	// next=jobselect();
	// /* 作业切换 */
	// jobswitch();
}

int allocjid()
{
	return ++jobid;
}

void updateall()
{
	struct waitqueue *p,*prev,*temp;

	/* 更新作业运行时间 */
	if(current)
		current->job->run_time += 1; /* 加1代表1000ms */

	/* 更新作业等待时间及优先级,每过5000ms优先级加1 */
	// for(p = head; p != NULL; p = p->next){
	// 	p->job->wait_time += 1000;
	// 	if(p->job->wait_time >= 5000 && p->job->curpri < 3){
	// 		p->job->curpri++;
	// 		p->job->wait_time = 0;
	// 	}
	// }
/**************************************************************************************************/
		//更新三个队列信息
		for(p = highhead; p != NULL; p = p->next){
			p->job->wait_time += 1000;
		}
		for(prev= middlehead,p = middlehead; p != NULL;){
			p->job->wait_time += 1000; 
			if(p->job->wait_time >= 5000){  //切换队列
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
			if(p->job->wait_time >= 5000){  //切换队列
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
	// 	/* 遍历等待队列中的作业，找到优先级最高的作业 */
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
		mark = 1;  //已经开始执行作业了
	}
/*********************************************************************************/


	if(current && current->job->state == DONE){ /* 当前作业完成 */
		/* 作业完成，删除它 */
		for(i = 0;(current->job->cmdarg)[i] != NULL; i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i] = NULL;
		}
		/* 释放空间 */
		free(current->job->cmdarg);
		free(current->job);
		free(current);
		current = NULL;
	}

	if(next == NULL && current == NULL){ /* 没有作业要运行 */
/*********************************************************************/
		jobk = -2; 
		mark = 0;   //又没有作业了
		nowjobqueuenumber = 0;
/*********************************************************************/
		return;
	}
	else if (next != NULL && current == NULL){ /* 开始新的作业 */

		printf("begin start new job\n");
		current = next;
		next = NULL;
		current->job->state = RUNNING;
/********************************************/
		//记录正在执行作业的优先级和队列
		nowjobqueuenumber = current->job->queuenumber;
		nowjobpri = current->job->curpri;
		while(goon < 1){
			jobk = -1;
		};
/********************************************/
		kill(current->job->pid,SIGCONT);
		return;
	}
	else if (next != NULL && current != NULL){ /* 切换作业 */

		printf("switch to Pid: %d\n",next->job->pid);
/********************************************/
		while(goon < 1){
			jobk = -1;
		};
/********************************************/
		kill(current->job->pid,SIGSTOP);
		//current->job->curpri = current->job->defpri;
/*******************************************************************/
		//更改优先级
		if(current->job->curpri>1){
			current->job->curpri = current->job->curpri-1;
		}
/*******************************************************************/
		current->job->wait_time = 0;
		current->job->state = READY;

		/* 放回等待队列，放到等待队列的最后 */
		// if(head){
		// 	for(p = head; p->next != NULL; p = p->next);
		// 	p->next = current;
		// }else{
		// 	head = current;
		// }
/**************************************************************************************************/
		//把current放回队列中
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
		//记录正在执行作业的优先级和队列
		nowjobqueuenumber = current->job->queuenumber;
		nowjobpri = current->job->curpri;
/*******************************************************************/
		kill(current->job->pid,SIGCONT);
		
		return;
	}else{ /* next == NULL且current != NULL，不切换 */
		return;
	}
}

void sig_handler(int sig,siginfo_t *info,void *notused)
{
	int status;
	int ret;

	switch (sig) {
case SIGVTALRM: /* 到达计时器所设置的计时间隔 */
	//时间片用完，开始一次调度
	scheduler();
	return;
case SIGCHLD: /* 子进程结束时传送给父进程的信号 */
	//等待任意一个子进程结束，并把结束状态保存在status中
	ret = waitpid(-1,&status,WNOHANG);
	if (ret == 0)
		return;
	//正常结束返回非0值
	if(WIFEXITED(status)){
		current->job->state = DONE;
		printf("normal termation, exit status = %d\n",WEXITSTATUS(status));
	}
	//因信号而结束返回真
	else if (WIFSIGNALED(status)){
		printf("abnormal termation, signal number = %d\n",WTERMSIG(status));
	}
	//子进程暂停时返回真
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

	/* 封装jobinfo数据结构 */
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

	/*向等待队列中增加新的作业*/
	newnode = (struct waitqueue*)malloc(sizeof(struct waitqueue));
	newnode->next =NULL;
	newnode->job=newjob;
/******************************************/
	newinjob = newnode;  //记录下刚进来作业在等待队列中的指针，以备抢占时使用
/******************************************/
	// if(head)
	// {
	// 	for(p=head;p->next != NULL; p=p->next);
	// 	p->next =newnode;
	// }else
	// 	head=newnode;

/**************************************************************************************************/
	//新作业都添加到队列中
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
	/*为作业创建进程*/
	if((pid=fork())<0)
		error_sys("enq fork failed");

	if(pid==0){
		newjob->pid =getpid();
		/*阻塞子进程,等等执行*/
		//函数对程序发送指定的信号
	/*************************************************/
		kill(getppid(), SIGUSR1);
	/*************************************************/
		raise(SIGSTOP);
#ifdef DEBUG

		printf("begin running\n");
		for(i=0;arglist[i]!=NULL;i++)
			printf("arglist %s\n",arglist[i]);
#endif

		/*复制文件描述符到标准输出*/
		//这里就把标准输出1的指针赋给了globalfd,1就不用了
		dup2(globalfd,1);
		/* 执行命令 */
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

	/*current jodid==deqid,终止当前作业*/
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
	else{ /* 或者在等待队列中查找deqid */
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
		//在三个队列中查找删除
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
	*打印所有作业的统计信息:
	*1.作业ID
	*2.进程ID
	*3.作业所有者
	*4.作业运行时间
	*5.作业等待时间
	*6.作业创建时间
	*7.作业状态
	*/

	/* 打印信息头部 */
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
		/* 如果FIFO文件存在,删掉 */
		if(remove("/tmp/server")<0)
			error_sys("remove failed");
	}
    //创建FIFO文件
	if(mkfifo("/tmp/server",0666)<0)
		error_sys("mkfifo failed");
	/* 在非阻塞模式下打开FIFO */
	if((fifo=open("/tmp/server",O_RDONLY|O_NONBLOCK))<0)
		error_sys("open fifo failed");

	/* 建立信号处理函数 */

	signal(SIGUSR1, setGoon);

	//使用sig_handler()函数来响应这些信号
	newact.sa_sigaction=sig_handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags=SA_SIGINFO;
	//进程暂停或中断时产生SIGCHLD信号
	sigaction(SIGCHLD,&newact,&oldact1);
	//setitimer函数设置的Virtual Interval Timer超时的时候产生SIGVTALRM
	sigaction(SIGVTALRM,&newact,&oldact2);
	/*
	每当进程因故停下或时间片用完时都要重新调用一次sig_handler()
	*/


	/* 设置时间间隔为1000毫秒 */
	interval.tv_sec=1;
	interval.tv_usec=0;

	new.it_interval=interval;		//指定计时器时间间隔
	new.it_value=interval;			//指定计时器初始时间
	setitimer(ITIMER_VIRTUAL,&new,&old);
	//// 我在执行调度函数时，超时了，他怎么执行的

	while(siginfo==1);

	close(fifo);
	close(globalfd);
	return 0;
}
