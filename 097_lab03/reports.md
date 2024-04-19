# 实验三 操作系统的引导

### 1.改写bootsect.s并显示正确
引导程序bootsect.s中包含了屏幕输出功能的代码，因此，借助bootsect.s程序原有的框架，更改后的代码的两部分以及注释如下所示：
屏幕输出部分代码：
```
! Print some inane message

	mov	ah,#0x03		 #读光标位置。
	xor	bh,bh            #bh = 显示页面号。返回光标位置值在 dx 中。
	int	0x10
	
	mov	cx,#28          #我显示的字符再包括6个回车换行字符一共28个字符，存放于cx
	mov	bx,#0x000b		! page 0, attribute b (blue)
	mov	bp,#msg1        #bp指向要显示的字符串
	mov	ax,#0x1301		! write string, move cursor
	int	0x10            #写字符串并移动光标到串结尾处。

msg1:
	.byte 13,10         #   #2个字符，回车的作用是使光标移动到本行的首个字符位置
	.ascii "Miyapeng os is running..."      #要打印出的内容
	.byte 13,10,13,10

```
以下是对代码的解释：
&emsp;&emsp;  其中，BIOS 中断 0x10 功能号 ah = 0x03，读光标位置。bh显示页面号，BIOS 中断 0x10 功能号 ah = 0x13，显示字符串。cx = 28表示我显示的字符加回车换行字符个数，mov	bp,#msg1让bp指向要显示的字符串。通过对cx和.ascii部分进行改写，可以在屏幕上的显示 如下图所示：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\1.png"/>
    <!--标题-->
    </center>
</div> 

由上图可见，改写代码后，屏幕中显示了正确的输出，光标的位置在第一个位置处。且由于`l:  jmp l  `的作用，使程序可以停在此处，不继续运行。

### 2.bootsect.s 读入 setup.s
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\2.png"/>
    <!--标题-->
    </center>
</div> 

如上图所示，引导程序在运行时会首先将自己移动到物理地址0x90000开始的位置，然后自己跳转到这个位置继续执行，然后将setup.s（从磁盘的第二个扇区开始的4个扇区）模块加载到bootsect后面位置(物理地址0x90200)。

<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\3.png"/>
    <!--标题-->
    </center>
</div> 

那么可见程序首先需要移动bootsect模块到正确位置，代码如下：
```
#下面这段代码将自身从0x07c0移动到0x9000处
#然后跳转到go后面,就是本程序的下一语句

    mov ax,#BOOTSEG   
    mov ds,ax          #将ds段设置为0x07c0
    mov ax,#INITSEG
    mov es,ax          #将es段设置为0x9000
    mov cx,#256       #设置计数器为256字(512字节)
    sub si,si            #源地址 ds:si = 0x07C0:0x0000         
    sub di,di            #目的地址 es:di = 0x9000:0x0000
    rep                  #重复执行,直到cx=0
    movw                #即 movs 指令。从内存[si]处移动 cx 个字到[di]处。
    jmpi    go,INITSEG   #段间跳转
```

该部分代码的作用是将bootsect.S从0x07C0移动到移动到0x9000，并跳转到移动后的开始位置。需要注意：8086汇编实运行在cpu的实模式下，寻址方式为：物理地址=段基址<<4 + 段内偏移地址。
然后是load setup.s程序，代码如下:
```
load_setup:
    mov dx,#0x0000      #（驱动器和磁头） drive 0, head 0
    mov cx,#0x0002      #sector 2, track 0
    mov bx,#0x0200      #address = 512, in INITSEG 设置读入的内存地址
    mov ax,#0x0200+SETUPLEN ! service 2, nr of sectors
                        #读入4个setup.s扇区
    int 0x13            #read it
    jnc ok_load_setup       #ok - continue
    mov dx,#0x0000      #软驱、软盘有问题才会执行到这里
    mov ax,#0x0000      #reset the diskette
    int 0x13
    j   load_setup

ok_load_setup:
                        #接下来跳转到setup执行
```

然后是在setup.S中写入显示到屏幕字符的代码，代码如下：
```
entry _start
_start:
	mov ah,#0x03	! 读光标位置
	xor bh,bh	
	int 0x10
	mov cx,#25	! 设置25个字节
	mov bx,#0x0007	!默认颜色
	mov bp,#msg2	!调用打印函数
	mov ax,cs	! 这里的cs其实就是这段代码的段地址
	mov es,ax
	mov ax,#0x1301
	int 0x10
inf_loop:
	jmp inf_loop
msg2:
	.byte	13,10
	.ascii	"Now we are in SETUP"
	.byte	13,10,13,10
.org 510
boot_flag:
	.word	0xAA55

```
代码逻辑和前面一致，编译运行时在`make BootImage`时会报错，这是因为 make 根据 Makefile 的指引执行了 tools/build.c，它是为生成整个内核的镜像文件而设计的，没考虑我们只需要 bootsect.s 和 setup.s 的情况。它在向我们要 “系统” 的核心代码，所以我们要修改build.c,下面是修改的步骤。
#### 修改 build.c
&emsp;&emsp;build.c 从命令行参数得到 bootsect、setup 和 system 内核的文件名，将三者做简单的整理后一起写入 Image。其中 system 是第三个参数（argv[3]）。当 “make all” 或者 “makeall” 的时候，这个参数传过来的是正确的文件名，build.c 会打开它，将内容写入 Image。而 “make BootImage” 时，传过来的是字符串 "none"。所以，改造 build.c 的思路就是当 argv[3] 是"none"的时候，只写 bootsect 和 setup，忽略所有与 system 有关的工作，或者在该写 system 的位置都写上 “0”。

代码如下：
```    
if(strcmp(argv[3], "none") != 0){       //使用strcmp,如果匹配到none,则直接返回
        if ((id=open(argv[3],O_RDONLY,0))<0)
                die("Unable to open 'system'");
        //  if (read(id,buf,GCC_HEADER) != GCC_HEADER)
        //      die("Unable to read header of 'system'");
        //  if (((long *) buf)[5] != 0)
        //      die("Non-GCC header of 'system'");
            for (i=0 ; (c=read(id,buf,sizeof buf))>0 ; i+=c )
                if (write(1,buf,c)!=c)
                    die("Write call failed");
            close(id);
            fprintf(stderr,"System is %d bytes.\n",i);
            if (i > SYS_SIZE*16)
                die("System is too big");
    }
    return(0);
```

然后再次编译，得到结果：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\4.png"/>
    <!--标题-->
    </center>
</div> 

### 3.setup获取硬件参数正确
&emsp;&emsp;setup.s 是一个操作系统加载程序，它的主要作用是利用 ROM BIOS 中断读取机器系统数据，并将这些数据保存到 0x90000 开始的位置（覆盖掉了 bootsect 程序所在的地方）。所取得的参数和保留的内存位置如下表所示：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\5.png"/>
    <!--标题-->
    </center>
</div> 

<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\6.png"/>
    <!--标题-->
    </center>
</div> 

根据要求，需要获取光标位置，内存大小，硬盘的柱面数，磁头数和每磁道扇区数信息。首先是获取光标位置和内存大小，并放在放在内存0x90000处，代码和解释如下所示：
```
! ok, the read went well so we get current cursor position and save it for
! posterity.
! 获取光标位置
#使用 BIOS 中断 0x10 功能 0x03 取屏幕当前光标位置，并保存在内存 0x90000 处（2 字节）。
#控制台初始化程序 console.c 会到此处读取该值。
#BIOS 中断 0x10 功能号 ah = 0x03，读光标位置。
#输入：bh = 页号
#返回：ch = 扫描开始线；cl = 扫描结束线；dh = 行号(0x00 顶端)；dl = 列号(0x00 最左边)。
	mov	ax,#INITSEG	! this is done in bootsect already, but...
	mov	ds,ax
	mov	ah,#0x03	! 读光标位置
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches
	mov	[0],dx		! 来自0x90000，将它存储在dx里

! Get memory size (extended mem, kB)
！ 获取内存大小
	mov	ah,#0x88
	int	0x15
	mov	[2],ax		!存储在ax里

! Get hd0 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]		! 把0x0000:0x0104单元存放的值（表示硬盘参数表阵列的首地址的偏移地址）赋给si寄存器
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0004	! 重复16次，即传送16B
	mov	cx,#0x10
	rep
	movsb

```

### 4.setup正确显示硬件参数
接下来介绍如何将读入的二进制数据转化为十六进制并输出到屏幕上。
每4位二进制数可以对应1位十六进制，需要将内存中的数按4位依次读取出来，然后将其转换为ASCII码送到显示器。ASCII码与十六进制数字的对应关系为：0x30~0x39对应数字 0 ~ 9，0x41 ~ 0x46对应数字a ~ f。
为使一个十六进制数能按高位到低位依次显示，实际编程中，需对bx中的数每次循环左移一组（4位二进制）， 然后屏蔽掉当前高12位，对当前余下的4位（即1位十六进制数）求其ASCII码， 要判断它是0 ~ 9还是a ~ f，是前者则加0x30得对应的ASCII码，后者则要加0x37才行，最后送显示器输出。 以上步骤重复4次，就可以完成bx中数以4位十六进制的形式显示出来。
代码如下所示：
```
! 打印前的准备
	mov ax,cs
 	mov es,ax
 	mov ax,#INITSEG
  	mov ds,ax

! 打印"Cursor position:"
	mov ah,#0x03
 	xor bh,bh
 	int 0x10
	mov cx,#18
	mov bx,#0x0007
	mov bp,#msg_cursor
	mov ax,#0x1301
	int 0x10

! 打印光标位置
	mov dx,[0]
	call	print_hex

! 打印"Memory Size:"
	mov ah,#0x03
	xor bh,bh
	int 0x10
	mov cx,#14
	mov bx,#0x0007
	mov bp,#msg_memory
	mov ax,#0x1301
	int 0x10

! 打印内存大小
	mov dx,[2]
	call	print_hex

! 打印"KB"
	mov ah,#0x03
 	xor bh,bh
 	int 0x10
	mov cx,#2
 	mov bx,#0x0007
	mov bp,#msg_kb
	mov ax,#0x1301
	int 0x10

! 打印"Cyls:" 
 	mov ah,#0x03
	xor bh,bh
	int 0x10
	mov cx,#7
	mov bx,#0x0007
	mov bp,#msg_cyles
	mov ax,#0x1301
	int 0x10

! 打印柱面数   
	mov dx,[4]
 	call	print_hex

! 打印"Heads:"
	mov ah,#0x03
	xor bh,bh
	int 0x10
	mov cx,#8
	mov bx,#0x0007
	mov bp,#msg_heads
	mov ax,#0x1301
	int 0x10

! 打印磁头数
	mov dx,[6]
	call    print_hex

! 打印"Sectors:"
	mov ah,#0x03
	xor bh,bh
	int 0x10
	mov cx,#10
	mov bx,#0x0007
	mov bp,#msg_sectors
	mov ax,#0x1301
	int 0x10
	mov dx,[18]
	call	print_hex

inf_loop:
	jmp	inf_loop

```
经过调试后，可以得到如下结果：
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\7.png"/>
    <!--标题-->
    </center>
</div> 

从图中得知Memory为0x3C00KB,刚好是 15MB（扩展内存），加上 1MB 正好是 16MB,Heads也正确对应。查看bochs/bochsrc.bxrc如下：

<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\8.png"/>
    <!--标题-->
    </center>
</div> 

### 5.实验报告问题讨论

- **有时，继承传统意味着别手蹩脚。 `x86` 计算机为了向下兼容，导致启动过程比较复杂。 请找出 `x86` 计算机启动过程中，被硬件强制，软件必须遵守的两个“多此一举”的步骤（多找几个也无妨），说说它们为什么多此一举，并设计更简洁的替代方案。**


我认为最多余的步骤就是在运行 32/64 位系统时，一开始 CPU 处于 16 位模式，需要程序自己将其切换到 32/64 位模式。而现代系统大多都运行在 32/64 位下，甚至 Windows11 已经放弃了对 32 位硬件的支持，只有 64 位版本了。在个人 PC 已经几乎完全淘汰了 16 位系统的情况下，让每一颗 CPU 都要兼容16位模式，使得软件需要专门切换，这是毫无必要的。
1. A20地址线问题

Intel 80286 CPU 具有 24 根地址线，为了兼容 20 根地址线的Intel 8088 CPU 的环绕机制，A20 信号被发明出来从而控制内存寻址位数。而芯片集的不同导致机器启动时很难用统一的方法开启 A20 地址线。

解决方案是：较新的 PC 调用 BIOS 中断 int 0x15, ax=0x2401(被称为 Fast A20) 就可以直接实现 A20 的功能。

2. system 模块二次移动问题

上面提到，bootsect 模块先将 system 模块加载到内存 0x10000 位置，等到 setup 模块运行时又将 system 模块移动到 0x0000位置。二次转移显得多此一举。

这样做是因为：计算机刚启动时处于实模式，内存 0x00000 开头一部分是留给 BIOS 存储中断向量表信息的，执行 bootsect 和 setup 模块时都需要用到 BIOS 中断，因此不能破坏这段信息，system 模块必须先加载到 0x10000 处。当setup.s 建立全局描述符表、中断描述符表，进入保护模式后，最开始的中断向量表就没用了，这时便可将 system 模块移动到 0x00000 处了。

解决方案是：BIOS 将中断向量表放到其他实模式下 BIOS 可以访问到的内存空间处，避免和 system 模块发生冲突，以便 system 模块能直接载入 0x00000 内存处。
Intel 提出的白皮书展示了 Intel 在 x86 架构受到来自 Arm 芯片的冲击之后的大胆改变。如果 x86-S 架构真的投入使用，不仅能简化启动过程，还能简化硬件电路等，提高性能、减少功耗。这也是我除了龙芯的 LoongArch 架构之外，最期待的 CPU 架构。