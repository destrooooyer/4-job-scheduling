#include "time.h"
#include <stdio.h>

void main()
{
	time_t timer, timerc;
	int count = 1;
	struct tm *timeinfo;

	/*time(&timer);//系统开始的时间*/
	while (1)
	{
// 		time(&timerc);
// 		if ((timerc - timer) >= 1)//每过1秒打印
// 		{
// 			printf("程序经过%d秒\n", count++);
// 			//timer = timerc;
// 			//切换完，等于完之后，俩都变成timerc，然而是切换前的时间，和系统时间依然差1s+，就会连续输出2次
// 
// 			//改成都变成系统时间
// 			time(&timer);
// 			time(&timerc);
// 		}
// 
// 		//什么鬼？while(1)里不sleep是在逗我么？
// 		usleep(1000);
		int i;
		sleep(1);
		printf("程序经过%d秒\n", count++);
	}

}
