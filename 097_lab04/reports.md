# 实验四 系统调用

### 一、将 testlab2.c 在修改过的 Linux 0.11 上编译运行，显示的结果即内核程序的得分
&emsp;&emsp;下面简述完成本实验的过程：
#### （1）添加系统调用号 + 修改系统调用总数
&emsp;&emsp;API 将系统调用号存入 EAX，然后调用中断进入内核态。这里新增了两个系统调用，所以要添加新的系统调用号，还要修改系统调用总数。
进入 linux-0.11/include 目录，打开 unistd.h ，增添新的系统调用编号，如下图所示。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\1.png"/>
    <!--标题-->
    </center>
</div> 

进入 linux-0.11/kernel 目录，打开 system_call.s ，修改系统调用总数。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\2.png"/>
    <!--标题-->
    </center>
</div> 

#### （2）维护系统调用表 + 编写系统调用函数
中断处理函数根据系统调用号，调用对应的内核函数，所以要为新增的系统调用添加系统调用函数名并维护系统调用表。
进入 linux-0.11/include/linux 目录，打开 sys.h ，维护系统调用表：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\3.png"/>
    <!--标题-->
    </center>
</div> 

进入 linux-0.11/kernel 目录，创建一个 **who.c** 文件，为新增的系统调用函数编写代码，即实现 **iam()** 和 **whoami()** 要求的功能。

#### who.c
```
#include <asm/segment.h>
#include <errno.h>
#include <string.h>
 
char _myname[24];
 
int sys_iam(const char *name)
{
	int i;
	char tmp[30];
	for(i=0; i<30; i++)
	{
		tmp[i] = get_fs_byte(name+i);
		if(tmp[i] == '\0') break;  //字符串结束
	}
	i=0;
	while(i<30&&tmp[i]!='\0') i++;
	int len = i;
	if(len > 23)
	{
		return -(EINVAL);  //置errno为EINVAL  返回“­-1”  具体见_syscalln宏展开
	}
	strcpy(_myname,tmp);
	return i;
}
 
int sys_whoami(char *name, unsigned int size)
{
    int length = strlen(_myname);
    //printk("%s\n", _myname);
 
    if (size < length)
    {
        errno = EINVAL;
        length = -1;
    }
    else
    {
        int i = 0;
        for (i = 0; i < length; i++)
        {
            /* copy from kernel mode to user mode */
            put_fs_byte(_myname[i], name + i);
        }
    }
    return length;
}
```
#### （3）修改makefile
要想让添加的 kernel/who.c 和其它 Linux 代码编译链接到一起，必须要修改 Makefile 文件。
在【OBJS】后添加 who.o，并在【Dependencies】后添加 who.s who.o: who.c ../include/linux/kernel.h ../include/unistd.h。具体如下图所示：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\4.png"/>
    <!--标题-->
    </center>
</div>  
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\5.png"/>
    <!--标题-->
    </center>
</div>  
在修改完后进入Linux0.11进行make all

#### （4）编写测试程序
到此为止，系统调用的内核函数已经完成。应用程序想要使用我们新增的系统调用 iam 和 whoami ，还需要添加对应的系统调用 API 。
- 在 oslab 目录下创建一个 iam.c 和 whoami.c 文件

#### iam.c
```
/* iam.c */
#define __LIBRARY__
#include <unistd.h> 
#include <errno.h>
#include <asm/segment.h> 
#include <linux/kernel.h>
 
_syscall1(int, iam, const char*, name);
   
int main(int argc, char *argv[])
{
    /* 调用系统调用iam() */
    iam(argv[1]);
    return 0;
}
```
#### whoami.c
```
/* whoami.c */
#define __LIBRARY__
#include <unistd.h> 
#include <errno.h>
#include <asm/segment.h> 
#include <linux/kernel.h>
#include <stdio.h>
   
_syscall2(int, whoami,char *,name,unsigned int,size);
   
int main(int argc, char *argv[])
{
    char username[64] = {0};
    /* 调用系统调用whoami() */
    whoami(username, 24);
    printf("%s", username);
    return 0;
}
```

#### （5）拷贝 iam.c,whoami.c,testlab2.c,testlab2.sh到 Linux 0.11 目录
&emsp;&emsp; 现在还不能直接编译运行，因为编写的 iam.c 和 whoami.c 还位于宿主机的 oslab 目录下，Linux 0.11 虚拟机目录下没有这两个文件，所以无法直接编译和运行。
&emsp;&emsp; 采用 `挂载` 的方式实现宿主机与虚拟机操作系统的文件共享。在 oslab 目录下执行`sudo ./mount-hdc`命令挂载 hdc 目录到虚拟机操作系统上.

在 oslab 目录下执行以下命令将上述两个文件拷贝到虚拟机 Linux 0.11 操作系统 /usr/root/ 目录下：
&emsp;&emsp; `cp iam.c whoami.c hdc/usr/root`
同时将在oslab/files目录下将testlab2.c,testlab2.sh也拷贝到目录下：
&emsp;&emsp; `cp testlab2.c testlab2.sh ../hdc/usr/root`
从下图可以看出拷贝成功：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\6.png"/>
    <!--标题-->
    </center>
</div> 

#### （6）启动虚拟机进行测试
编译并测试：
```
[/usr/root]# gcc -o iam iam.c
[/usr/root]# gcc -o whoami whoami.c
[/usr/root]# ./iam miyapeng
[/usr/root]# ./whoami
```
结果如下：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\7.png"/>
    <!--标题-->
    </center>
</div> 

对testlab2.c进行编译并运行如下图所示：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\8.png"/>
    <!--标题-->
    </center>
</div> 
可见通过所有测试。

### 二、将脚本testlab2.sh在修改过的 Linux 0.11 上运行，显示的结果即应用程序的得分。
Linux的一大特色是可以编写功能强大的 shell 脚本，提高工作效率。 本实验的部分评分工作由脚本 testlab2.sh 完成。它的功能是测试 iam.c 和 whoami.c 。
在上面实验的基础上直接运行testlab2.sh查看结果，得到结果如下：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\9.png"/>
    <!--标题-->
    </center>
</div> 
可见三个测试均通过。

### 三、实验报告
##### 从 Linux 0.11 现在的机制看，它的系统调用最多能传递几个参数？
答：直接能传递的参数至多有 3 个。因为在 Linux-0.11 中，程序使用 ebx、ecx、edx 这三个通用寄存器保存参数，可以直接向系统调用服务过程传递至多三个参数 (不包括放在 eax 寄存器中的系统调用号)。

##### 你能想出办法来扩大这个限制吗？
经查阅资料，有以下方法来扩大这个限制。
（1）增加传参所用的寄存器，但现实不太适用；
（2）用这3个寄存器循环传值；
（3）将寄存器拆分为高位和低位传递一直比较小的参数；
（4）利用堆栈传递参数。
（5）使用指向用户数据空间的指针，将指针信息通过寄存器传递给系统调用服务，然后系统调用就可以通过该指针访问用户数据空间中预置的更多数据

##### 用文字简要描述向 Linux 0.11 添加一个系统调用 foo() 的步骤。
和添加who()一样，添加的步骤如下：
（1）在include/unistd.h中定义宏__NR_foo，并添加函数声明和函数调用。
（2）在kernel/system_call.s修改总调用数，使nr_system_calls数值加1。
（3）在include/linux/sys.h中添加函数声明extern void sys_foo()，在系统调用入口表fn_ptr末端添加元素sys_foo；
（4）在内核文件中添加kernel/foo.c文件，实现sys_foo()函数。
（5）修改kernel/Makefile，在OBJS中加入foo.o，并添加生成foo.s、foo.o的依赖规则。
（6）退回linux-0.11目录下，运行make all指令编译内核，这样就能把foo.c加入到内核中。


