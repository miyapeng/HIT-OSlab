# 5.进程运行轨迹的跟踪与统计


## 5.1. 实验过程

### 编写`process.c`

根据给出的process.c模板，通过对其进行修改，使用fork()建立若干个并行的子进程，父进程等待所有子进程退出后才退出。代码如下所示：

```c
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
	/*5个子进程 PID*/
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

```

### 修改init/main.c

根据实验指导，为了能尽早开始记录，应当在内核启动时就打开 `log` 文件。 内核的入口是 `init/main.c` 中的 `main()`，因此修改如下：

![img22](images\img22.png)

### 修改kernel/printk.c

`log` 文件将被用来记录进程的状态转移轨迹。所有的状态转移都是在内核进行的。 在内核状态下， `write()` 功能失效，其原理等同于《系统调用》实验中不能在内核状态调用 `printf()` ， 只能调用 `printk()` 。 编写可在内核调用的 `write()` 的难度较大，所以这里直接给出源码。 它主要参考了 `printk()` 和 `sys_write()` 而写成的：

```c
static char logbuf[1024];
int fprintk(int fd, const char *fmt, ...)
{
    va_list args;
    int count;
    struct file * file;
    struct m_inode * inode;

    va_start(args, fmt);
    count=vsprintf(logbuf, fmt, args);
    va_end(args);

    if (fd < 3)    /* 如果输出到stdout或stderr，直接调用sys_write即可 */
    {
        __asm__("push %%fs\n\t"
            "push %%ds\n\t"
            "pop %%fs\n\t"
            "pushl %0\n\t"
            "pushl $logbuf\n\t" /* 注意对于Windows环境来说，是_logbuf,下同 */
            "pushl %1\n\t"
            "call sys_write\n\t" /* 注意对于Windows环境来说，是_sys_write,下同 */
            "addl $8,%%esp\n\t"
            "popl %0\n\t"
            "pop %%fs"
            ::"r" (count),"r" (fd):"ax","cx","dx");
    }
    else    /* 假定>=3的描述符都与文件关联。事实上，还存在很多其它情况，这里并没有考虑。*/
    {
        // if (!(file=task[0]->filp[fd]))    /* 从进程0的文件描述符表中得到文件句柄 */
        //    return 0;
        
        // 修改为如下：
        // task[1]->filp is not ready or f_inode->i_dev is not ready
        if (!(file=task[1]->filp[fd]) || !task[1]->filp[fd]->f_inode->i_dev) {   /* 从进程1的文件描述符表中得到文件句柄 */
            return 0;
        }
        inode=file->f_inode;

        __asm__("push %%fs\n\t"
            "push %%ds\n\t"
            "pop %%fs\n\t"
            "pushl %0\n\t"
            "pushl $logbuf\n\t"
            "pushl %1\n\t"
            "pushl %2\n\t"
            "call file_write\n\t"
            "addl $12,%%esp\n\t"
            "popl %0\n\t"
            "pop %%fs"
            ::"r" (count),"r" (file),"r" (inode):"ax","cx","dx");
    }
    return count;
}
```

可以将此函数放入到 `kernel/printk.c` 的尾部，并按照下面方式进行调用

```c
fprintk(1, "The ID of running process is %ld", current->pid); //向stdout打印正在运行的进程的ID
fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'R', jiffies); //向log文件输出
```

### 跟踪进程运行轨迹

欲记录进程的切换，需要找到所有发生进程状态切换的代码点，并在这些点添加适当的代码，来输出进程状态变化的情况到 `log` 文件中。其中涉及到fork.c、sched.c、exit.c文件。

首先，需要找到进程的状态保存的位置，其位于`sched.h` 文件中。

![img1](images\img1.png)

为了找到全部状态切换点，由于我使用的是vscode 通过ssh远程连接虚拟机，因此可以在vscode使用`ctrl+shift+f`进行全局搜索，例如TASK_RUNNING，涉及到fork.c和shed.c文件，接下来对其进行修改。

![img2](images\img2.png)

#### 修改fork.c

`jiffies` 在 `kernel/sched.c` 文件中定义为一个全局变量,它记录了从开机到当前时间的时钟中断发生次数。在 `kernel/sched.c` 文件中的 `sched_init()` 函数中， 时钟中断处理函数被设置为：

```
set_intr_gate(0x20,&timer_interrupt);
```

而在 `kernel/system_call.s` 文件中将 `timer_interrupt` 定义为：

```
timer_interrupt:

    incl jiffies     #增加jiffies计数值
```

这说明 `jiffies` 表示从开机时到现在发生的时钟中断次数，这个数也被称为 **滴答数**。

真正实现进程创建的函数是 `copy_process()`

```c
int copy_process(int nr, /* ……*/)
{
    struct task_struct *p;
    // ……
    p = (struct task_struct *) get_free_page();  //获得一个task_struct结构体空间
    // ……
    p->pid = last_pid;
    // ……
    p->start_time = jiffies;    //设置start_time为jiffies
    // ……
    p->state = TASK_RUNNING;    //设置进程状态为就绪。所有就绪进程的状态都是
                                //TASK_RUNNING(0），被全局变量current指向的
                                //是正在运行的进程。
    return last_pid;
}
```

因此要完成进程运行轨迹的记录就要在 `copy_process()` 中添加输出语句。这里要输出两种状态，分别是 `N` （新建） 和 `J` （就绪） 。

**根据上述提示**

需要在` p->start_time = jiffies; `和`p->state = TASK_RUNNING`部分进行修改

这两部分分别涉及到进程的新建和就绪，需要进行记录，改动如下所示：

（1）进程新建

![img3](images\img3.png)

（2）进程就绪

![img4](images\img4.png)

### 修改sched.c

##### 修改schedule

根据上文已经找到的TASK_RUNNING部分

(1)就绪

![img5](images\img5.png)

（2）就绪且运行

![img6](images\img6.png)

##### 修改sys_pause

- 进入阻塞态

![img7](images\img7.png)

修改sleep_on

分别切换到阻塞态和就绪态

![img8](images\img8.png)

##### 修改interruptible_sleep_on

根据state切换的值改动代码

![img9](images\img9.png)

##### 修改wake_up

同理

![img10](images\img10.png)

#### 修改exit.c

同理，主要修改`do_exit()`和`sys_waitpid()`这两个函数。

`do_exit()` 函数实现了进程的实际终止操作。当一个进程调用 exit() 函数或者异常终止时，内核会调用 do_exit() 函数。这个函数首先释放该进程占用的内存页面、打开的文件描述符和信号等资源，然后使用 exit_code 参数来设置进程的退出状态码，并将进程状态设置为 ZOMBIE（僵尸）状态，表示该进程已经终止但还未被父进程回收。最后，do_exit() 函数通过调用 schedule() 函数来让 CPU 执行下一个可运行的进程。

`sys_waitpid()`函数则是用于父进程等待子进程结束并获取其退出状态码的系统调用。在调用 waitpid() 函数时，父进程会阻塞自己，直到被等待的子进程处于终止状态。如果子进程已经终止，则 waitpid() 函数会立即返回，否则它会等待子进程终止。在成功等待到子进程终止时，waitpid() 函数返回子进程的 PID 和其退出状态码。如果指定的子进程不存在或者不是当前进程的子进程，则 waitpid() 函数会返回错误。

改动如下：

![img11](images\img11.png)

![img12](images\img12.png)

### 生成日志
使用boch2.7会导致系统试图休眠task[0]的sync报错！怀疑可能原因是：正常电脑程序所有操作都是task0的子进程，而若把该行指令代码加在init函数之前，函数尚未执行第一次fork，那么将会是task0去执行该条程序（事实上，这样的操作是应当被禁止的，task0只负责fork子进程），bochs可能对该类对task0的操作有非法检验，所以出现报错。
使用bochs2.4继续进行实验，在一切文件修改之后，进入linux-0.11文件夹中进行编译，然后将process.c通过`sudo ./mount-hdc`命令拷贝到bochs中，然后启动bochs，对process.c进行编译并运行，如下所示：
![img23](images\img23.png)

![img13](images\img13.png)

成功运行，然后sync将log文件保存，然后退出虚拟机，并将log文件拷贝到oslab文件夹下，内容如下所示：

![img14](images\img14.png)

从中可以看出，能向日志文件输出信息且5种状态都能输出。

### 统计进程时间

根据老师所提供的程序，只要给 `stat_log.py` 加上执行权限，就可以直接运行它。在ubuntu上安装python，使用python2对脚本进行执行，由于它们均在oslab目录下，执行结果如下所示

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process.log 
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      0         3718         0           0           0
      2           25         4          20           0
      1           25         0           1           0
      3         3003         0           4        2998
      4         9009        81          64        8863
      5            3         1           2           0
      6            7         1           6           0
      7            4         0           3           0
      8          282         2          11         268
      9           58         0          57           0
     10          108         0         107           0
     11           26         0          25           0
     12           80         0          79           0
     13            3         1           2           0
     14         2251         0          10        2240
     15         2247       142           0        2105
     16         3477      1686         200        1590
     17         3553      3046         400         107
     18         3552      3132         420           0
     19         3537      3116         420           0
     20         3521      3101         420           0
     21         3503      3083         420           0
     22         3488      3067         420           0
     23         3472      3052         420           0
     24         3456      3036         420           0
     25            4         0           3           0
     26           46        45           0           0
Average:     1942.89    985.04
Throughout: 0.30/s

```
![img24](images\img24.png)


### 调度算法时间片修改

根据实验提示， `0.11` 的调度算法是选取 `counter` 值最大的就绪进程进行调度。 其中运行态进程（即 `current` ）的 `counter` 数值会随着时钟中断而不断减 `1` （时钟中断 `10ms` 一次）， 所以是一种比较典型的时间片轮转调度算法。

此处要求实验者对现有的调度算法进行时间片大小的修改，并进行实验验证。

为完成此工作，我们需要知道两件事情：

1. 进程 `counter` 是如何初始化的？
2. 当进程的时间片用完时，被重新赋成何值？

首先回答第一个问题，显然这个值是在 `fork()` 中设定的。 `Linux 0.11` 的 `fork()` 会调用 `copy_process()` 来完成从父进程信息拷贝（所以才称其为 `fork` ）， 看看 `copy_process()` 的实现（也在 `kernel/fork.c` 文件中），会发现其中有下面两条语句：

```
*p = *current;             //用来复制父进程的PCB数据信息，包括priority和counter
p->counter = p->priority;  //初始化counter
```

因为父进程的 `counter` 数值已发生变化，而 `priority` 不会， 所以上面的第二句代码将 `p->counter` 设置成 `p->priority` 。 每个进程的 `priority` 都是继承自父亲进程的，除非它自己改变优先级。 查找所有的代码，只有一个地方修改过 `priority` ，那就是 `nice` 系统调用：

```
int sys_nice(long increment)
{
    if (current->priority-increment>0)
        current->priority -= increment;
    return 0;
}
```

本实验假定没有人调用过 `nice` 系统调用，时间片的初值就是进程 `0` 的 `priority` ，即宏 `INIT_TASK` 中定义的：

```
#define INIT_TASK \
  { 0,15,15, //分别对应state;counter;和priority;
```

接下来回答第二个问题，当就绪进程的 `counter` 为 `0` 时，不会被调度（ `schedule` 要选取 `counter` 最大的，大于 `0` 的进程），而当所有的就绪态进程的 `counter` 都变成 `0` 时，会执行下面的语句：

```
(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
```

显然算出的新的 `counter` 值也等于 `priority` ，即初始时间片的大小。

因此通过更改counter的值，即可修改初始时间片的大小。如下所示，将15改为30

![img16](images\img16.png)

再次进行编译，然后重复生成log文件过程，并对新的log文件进行分析：

对应进程9-13

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process_10_30.log 7 8 9 10 11 12 13 14 15 16 
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      7         2248       142           0        2105
      8         3800      1687         200        1913
      9         3890      3060         400         430
     10         3890      3424         465           0
     11         3874      3409         465           0
     12         3858      3392         465           0
     13         3842      3376         465           0
     14         3826      3361         465           0
     15         3811      3345         465           0
     16         3795      3329         465           0
Average:     3683.40   2852.50
Throughout: 0.26/s
```

## 5.4.实验报告

### **结合自己的体会，谈谈从程序设计者的角度看，单进程编程和多进程编程最大的区别是什么？**

单进程编程和多进程编程最大的区别在于它们如何管理计算机系统中的资源。

单进程编程的所有代码都运行在同一个进程中，从而导致不同模块的代码可能会出现竞争关系，因此程序员需要确保不同模块之间不会相互影响，需要考虑线程安全等问题。单进程编程适用于单个任务情况。

多进程编程中，每个进程都是独立的，它们有自己的地址空间和资源，可以并行运行。因此，多进程编程更容易出现并发操作，程序员需要考虑不同进程之间的同步和通信问题，以及共享内存和死锁等问题。

### **你是如何修改时间片的？仅针对样本程序建立的进程，在修改时间片前后， `log` 文件的统计结果（不包括Graphic）都是什么样？结合你的修改分析一下为什么会这样变化，或者为什么没变化？**

最初生成的进程为5个，但是基本没有区别：

| 时间片长度 | 平均周转时间 | 平均等待时间 | 吞吐量 |
| ---------- | ------------ | ------------ | ------ |
| 1          | 1858.20      | 828.4        | 0.22/s |
| 5          | 1858.60      | 828.60       | 0.22/s |
| 10         | 1857.60      | 828.00       | 0.22/s |
| 15         | 1858.20      | 828.40       | 0.22/s |
| 30         | 1858.20      | 828.60       | 0.22/s |

于是更改`process.c`，将进程设为10个，统计结果如下所示。

将时间片分别设置为1,5,10,15,30,60，统计结果如下所示：

#### 设置为15

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process_10_15_2.log 7 8 9 10 11 12 13 14 15 16 
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      7         2247       142           0        2105
      8         3373      1686         200        1486
      9         3458      3058         400           0
     10         3448      3042         405           0
     11         3431      3025         405           0
     12         3415      3010         405           0
     13         3399      2994         405           0
     14         3384      2978         405           0
     15         3368      2963         405           0
     16         3352      2947         405           0
Average:     3287.50   2584.50
Throughout: 0.29/s
```

#### 设置为1

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process_10_1.log 7 8 9 10 11 12 13 14 15 16 17
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      7         2248       142           0        2105
      8         3788      1686         200        1901
      9         4508      3048         400        1060
     10         4508      3952         555           0
     11         4492      3936         555           0
     12         4476      3921         555           0
     13         4461      3905         555           0
     14         4445      3889         555           0
     15         4429      3874         555           0
     16         4413      3857         555           0
     17            4         0           3           0
Average:     3797.45   2928.18
Throughout: 0.24/s
```

#### 设置为5

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process_10_5_2.log 7 8 9 10 11 12 13 14 15 16 
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      7         2248       143           0        2105
      8         3684      1687         200        1796
      9         3774      3058         400         315
     10         3772      3322         450           0
     11         3756      3306         450           0
     12         3741      3290         450           0
     13         3725      3275         450           0
     14         3709      3259         450           0
     15         3694      3243         450           0
     16         3678      3228         450           0
Average:     3578.10   2781.10
Throughout: 0.26/s
```

#### 设置为10

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process_10_10.log 7 8 9 10 11 12 13 14 15 16 
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      7         2248       142           0        2105
      8         3683      1687         200        1796
      9         3774      3057         400         316
     10         3773      3323         450           0
     11         3756      3306         450           0
     12         3741      3290         450           0
     13         3725      3274         450           0
     14         3709      3259         450           0
     15         3694      3243         450           0
     16         3678      3227         450           0
Average:     3578.10   2780.80
Throughout: 0.26/s
```

#### 设置为30

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process_10_30.log 7 8 9 10 11 12 13 14 15 16 
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      7         2248       142           0        2105
      8         3800      1687         200        1913
      9         3890      3060         400         430
     10         3890      3424         465           0
     11         3874      3409         465           0
     12         3858      3392         465           0
     13         3842      3376         465           0
     14         3826      3361         465           0
     15         3811      3345         465           0
     16         3795      3329         465           0
Average:     3683.40   2852.50
Throughout: 0.26/s
```

#### 设置为60

```sh
miyapeng@miyapeng-virtual-machine:~/oslab5$ python2 stat_log.py process_10_60.log 7 8 9 10 11 12 13 14 15 16 
(Unit: tick)
Process   Turnaround   Waiting   CPU Burst   I/O Burst
      7         2248       142           0        2105
      8         3803      1687         200        1916
      9         3878      3057         400         421
     10         3878      3412         465           0
     11         3862      3397         465           0
     12         3846      3381         465           0
     13         3830      3364         465           0
     14         3814      3349         465           0
     15         3798      3333         465           0
     16         3783      3317         465           0
Average:     3674.00   2843.90
Throughout: 0.26/s
```

| 时间片长度 | 平均周转时间 | 平均等待时间 | 吞吐量(/s) |
| ---------- | ------------ | ------------ | ---------- |
| 1          | 3797.45      | 2928.18      | 0.24       |
| 5          | 3578.10      | 2781.10      | 0.26       |
| 10         | 3578.10      | 2780.80      | 0.26       |
| 15         | 3287.50      | 2584.50      | 0.30       |
| 30         | 3683.40      | 2852.50      | 0.26       |
| 60         | 3674.00      | 2843.90      | 0.26       |

可以看到，通过更改时间片的长度，统计结果没有太明显的差距。但也有一些变化

- 时间片从15->10、5、1，waiting时间加长，这是因为进程因时间片不断切换产生的进程调度次数变多，该进程等待时间越长。
- 时间片增大为30,60，waiting时间也加长，且平均周转时间加长，吞吐量降低，这是因为随着时间片增大，可能会出现时间片大小过大的情况，可能会出现某些进程长时间占用CPU资源的情况。这会导致其它进程无法及时获得CPU的运行权限，从而降低了系统的响应速度和吞吐量，也会使得平均等待时间变长。

- 因此要合理设计时间片的大小