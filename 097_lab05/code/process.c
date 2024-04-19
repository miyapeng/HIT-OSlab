#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>

#include <stdlib.h>

#define HZ 100
#define num 10
void cpuio_bound(int last, int cpu_time, int io_time);

int main(int argc, char *argv[])
{
	int i;
	/*10个子进程 PID*/
	pid_t n_process[num]; 
	for (i = 0; i < num; i++)
	{
		n_process[i] = fork();
		/*子进程*/
		if (n_process[i] == 0)
		{
			cpuio_bound(20, 2 * i, 20 - 2 * i); /*每个子进程都占用20s*/
			return 0;							/*执行完cpuio_bound 以后，结束该子进程*/
		}
		/*fork failure*/
		else if (n_process[i] < 0)
		{
			printf("Failed to fork child process %d!\n", i + 1);
			return -1;
		}
		/*父进程继续fork*/
	}
	/*打印所有子进程PID*/
	for (i = 0; i < num; i++)
		printf("Child PID: %d\n", n_process[i]);
	/*等待所有子进程完成*/
	wait(&i); 
	return 0;
}

void cpuio_bound(int last, int cpu_time, int io_time)
{
	struct tms start_time, current_time;
	clock_t utime, stime;
	int sleep_time;
	while (last > 0)
	{
		times(&start_time);
		do
		{
			times(&current_time);
			utime = current_time.tms_utime - start_time.tms_utime;
			stime = current_time.tms_stime - start_time.tms_stime;
		} while (((utime + stime) / HZ) < cpu_time);
		last -= cpu_time;

		if (last <= 0)
			break;

		sleep_time = 0;
		while (sleep_time < io_time)
		{
			sleep(1);
			sleep_time++;
		}
		last -= sleep_time;
	}
}
