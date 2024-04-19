## 漫谈hello world!
&emsp;&emsp;众所众知，hello world是计算机学子的第一站，那么在学习了操作系统后，我们又会对hello world有什么新的认识呢？本报告将以操作系统的角度来分析hello world运行过程中的点点滴滴，报告中不免会有错误，还望老师指正。
&emsp;&emsp;分析hello时肯定是先在bochs里运行hello，幸运的是linux0.11中有hello.c和编译好的hello，所以直接打开并运行。运行的结果如下图所示：
![img1](images\img1.png)
接下来将从进程管理，内存管理，IO控制等方面进行分析，让我们开始吧。
### 1.输入./hello前的工作
本报告基于Linux0.11完成，加载shell界面的工作在第三个实验中已有涉及，这里进行简要介绍。
在我们运行./run打开bochs的shell加载Linux0.11时，系统已经做了很多工作。概括起来就是两点：**进入内核前的准备工作**和**初始化**。
#### 进入内核前的准备工作
当按下开机键的那一刻，在主板上提前写死的固件程序 BIOS 会将硬盘中启动区的 512 字节的数据，原封不动复制到内存中的 0x7c00 这个位置，并跳转到那个位置进行执行，其执行过程如下图所示：
![img2](images\img2.png)
接下来就是将setup.s、head.s编译并加载进入内存，完成将硬盘中的内容全部加载到内存上的工作，下面这张图可以很全面的说明这个过程：
![img3](images\img3.png)
然后就是逐渐进入保护模式，并设置分段、分页、中断等机制。具体来说，就是设置GDT和IDT，设置分页，总之，在系统的一番设置后，内存布局就变成了下面这个样子：
![img4](images\img4.png)
至此，操作系统进入内核，开始执行各种_init函数进行初始化。
#### 初始化
进入内核后执行的主要函数是main.c，完成初始化的核心代码就下面几行：
```
void main(void) {
    ...
    mem_init(main_memory_start,memory_end);
    trap_init();
    blk_dev_init();
    chr_dev_init();
    tty_init();
    time_init();
    sched_init();
    buffer_init(buffer_memory_end);
    hd_init();
    floppy_init();
    sti();
    move_to_user_mode();
    if (!fork()) {init();}
    for(;;) pause();
}
```
&emsp;&emsp;这段代码包括内存初始化 mem_init，中断初始化 trap_init、进程调度初始化 sched_init 等等，调用相关的函数分别完成相应的初始化工作，然后由move_to_user_mode()切换至用户态模式，并在一个新的进程中做一个最终的初始化 init。这个 init 函数里会创建出一个进程，设置终端的标准 IO，并且再创建出一个执行 shell 程序的进程用来接受用户的命令，到这里我们的shell就出现在我们的面前了。我们便可以在界面中输入指令。
&emsp;&emsp;同时在网上学习时找到一张说明Linux0.11整体流程的说明图，私以为非常简洁明了，放置在下面，方便理解。
![img5](images\img5.png)
### 2.Hello World进程管理
&emsp;&emsp;在学过操作系统后都知道，进程在操作系统中扮演着关键的角色，操作系统每一次任务的执行都离不开它。那么我们运行hello world肯定也离不开它。从某种程度上来说，操作系统是一个靠中断运行的死循环，其在main函数中也能体现出来，进入shell后的整个操作系统的大核心便是下面这几句代码：
```
void main(void) {
    ...
    move_to_user_mode();
    if (!fork()) {
        // 创建了新进程
        init();
    }
    // 死循环，操作系统怠速状态
    for(;;) pause();
}
```
&emsp;&emsp;这么一看代码还挺简单的，但就是这没有多少个字母的四行代码，是整个操作系统的精髓所在，也是最难的四行代码。我们在实验5中了解学习了fork函数和进程1的创建工作流程，还打印了log文件，同样可以把相关的知识和技巧用在本次helloworld的分析上。

让我们挑一些重点的代码来看，

move_to_user_mode()将当前代码的特权级，从内核态变为用户态。一旦转变为了用户态，那么之后的代码将一直处于用户态的模式，除非发生了中断，比如用户发出了系统调用的中断指令，那么此时将会从用户态陷入内核态，不过当中断处理程序执行完之后，又会通过中断返回指令从内核态回到用户态。所以Hello World在执行的过程中除了我们输入相关指令和输出等中断时，其一直在用户状态下执行。这一点可以从cs代码寄存器段选择子的CPL来看，11就表示是当前处理器处于用户态这个特权级。

那为什么说操作系统跳不出用户态呢，因为代码跳转只能同特权级，数据访问只能高特权级访问低特权级，除了中断和中断返回等方法。

接下来就涉及到进程了，HelloWorld所在的进程可以被操作系统管理并不出错要归功于为进程设计的数据结构，还有TSS。TSS中保存了一个进程的上下文相关信息，CPU 规定，如果 ljmp 指令后面跟的是一个 tss 段，那么，会由硬件将当前各个寄存器的值保存在当前进程的 tss 中，并将新进程的 tss 信息加载到各个寄存器。
![img6](images\img6.png)
我们可以在oslab/linux-0.11/include/linux/sched.h找到相关的结构体定义如下：
![img8](images\img8.png)
在这里可以看到一些用于进程调度的元素，如state表示进程的状态，counter用于进行进程调度等，那么我们就要回答一个问题，Linux0.11中是如何进行进程调度的呢？
在schedule(void)中我们可以得知该调度算法的机制：
1. 拿到剩余时间片（counter的值）最大且在 runnable 状态（state = 0）的进程号 next。
2. 如果所有 runnable 进程时间片都为 0，则将所有进程（注意不仅仅是 runnable 的进程）的 counter 重新赋值（counter = counter/2 + priority），然后再次执行步骤 1。
3. 最后拿到了一个进程号 next，调用了 switch_to(next) 这个方法，就切换到了这个进程去执行了。

好了现在我们最后再分析一下fork函数，fork 触发系统调用中断，最终调用到了 sys_fork 函数。fork的大致流程如下：
![img9](images\img9.png)
就这样，新的进程就创建成功了，这里有一段内存分配和管理的操作，我们放在内存管理那里说明。
这里写入一个log文件来说明HelloWorld中的进程活动,利用实验5中的fprintk函数，对进程进行打印输出。在fork.c和sched.c中增加代码如下：
![img20](images\img20.png)
新增代码主要是在运行时打印进程的pid，father，jiffies等信息，并将这些信息写入hello.log中进行一个对比分析。加入后打开界面运行如下：
![img21](images\img21.png)
从其中我们可以看到hello程序运行在pid为6和7的进程上，由nr得知该进程为进程4.并且其由pid=4的进程创建，我们打开hello.log仔细看一下：
![img22](images\img22.png)
可以见得是一致的，而且还发生了两次调度，一次sleep_on。
### 3.Hello World内存管理
这里的内存管理我打算从main中的内存设置讲起，main中刚开始有一段内存代码如下：
```
void main(void) {
    ROOT_DEV = ORIG_ROOT_DEV;
    drive_info = DRIVE_INFO;
    memory_end = (1<<20) + (EXT_MEM_K<<10);
    memory_end &= 0xfffff000;
    if (memory_end > 16*1024*1024)
        memory_end = 16*1024*1024;
    if (memory_end > 12*1024*1024) 
        buffer_memory_end = 4*1024*1024;
    else if (memory_end > 6*1024*1024)
        buffer_memory_end = 2*1024*1024;
    else
        buffer_memory_end = 1*1024*1024;
    main_memory_start = buffer_memory_end;
    ...
}
```
这段代码是再在存管理前设置3个边界值：main_memory_start、memory_end、buffer_memory_end，划分后的内存结构如下：
![img10](images\img10.png)
那么主内存是如何管理的？哈哈，答案就是操作系统用一张大表管理内存。我们来细细说道。
我们可以在oslab/linux-0.11/mm/memory中找到核心方法mem_init：
![img12](images\img12.png)
就是给一个 mem_map 数组的各个位置上赋了值，而且显示全部赋值为 USED 也就是 100，然后对其中一部分又赋值为了0。可以用下面一张表来表示：
![img11](images\img11.png)
可以看出，初始化完成后，其实就是 mem_map 这个数组的每个元素都代表一个 4K 内存是否空闲（准确说是使用次数）。
4K 内存通常叫做 1 页内存，而这种管理方式叫分页管理，就是把内存分成一页一页（4K）的单位去管理。
1M 以下的内存这个数组干脆没有记录，这里的内存是无需管理的，或者换个说法是无权管理的，也就是没有权利申请和释放，因为这个区域是内核代码所在的地方，不能被“污染”。
1M 到 2M 这个区间是缓冲区，2M 是缓冲区的末端，缓冲区的开始在哪里之后再说，这些地方不是主内存区域，因此直接标记为 USED，产生的效果就是无法再被分配了。
2M 以上的空间是主内存区域，而主内存目前没有任何程序申请，所以初始化时统统都是零，未来等着应用程序去申请和释放这里的内存资源。
那么在申请内存的过程中，是如何使用 mem_map 这个结构的呢。在 memory.c 文件中有个函数 get_free_page()，用于在主内存区中申请一页空闲内存页，并返回物理内存页的起始地址。比如我们在 fork 子进程的时候，会调用 copy_process 函数来复制进程的结构信息，其中有一个步骤就是要申请一页内存，用于存放进程结构信息 task_struct。
fork 函数为新的进程（进程 1）申请了槽位，并把全部 task_struct 结构的值都从进程零复制了过来。代码可在fork.c中的copy_mem中找到，这里附上这段代码：
![img23](images\img23.png)
其就是对新进程 LDT 表项赋值，以及页表的拷贝
过程如下：
![img14](images\img14.png)
所以我们通过查看LDT表来看看这个过程，打开GDT表
![img24](images\img24.png)
可以看到在第四个LDT的地址为0xFCB2D0，我们查找其物理地址得：
![img25](images\img25.png)
至此，段表便查询到了。然后我们改写memory.c代码，为其加入地址的打印信息，将其打印到hello.log中。代码如下：
![img26](images\img26.png)
运行后找到pid=6的那里的地址，得到如下的log文件记录：
![img27](images\img27.png)
可读出页表地址为0xf91007，发生了缺页中断，新分配的页地址为0xfc7007。
### 4.缺页
当触发 Page-Fault 中断后，就会进入 Linux 0.11 源码中的 page_fault 方法
，核心函数在do_no_page中，我们可以看看这段代码：
![img19](images\img19.png)
先计算出 address 所在的页，其实就是完成一次 4KB 的对齐，这个地址是整个线性地址空间的地址，但对于进程 2 自己来说，需要计算出相对于进程 2 的偏移地址，也就是去掉进程 2 的段基址部分。然后使用get_free_page 去 mem_map[] 中寻找一个值为 0 的位置，这就表示找到了空闲内存。找到一页物理内存后，便把硬盘中的数据加载进来，最后一步完成页表的映射。由于 Linux 0.11 使用的是二级页表，所以实际上就是写入页目录项和页表项的过程。
可以看到上面代码中已写入一段代码，在hello.log中打印是否发生缺页已经缺页的第一个块。而且可以发现出现了多次缺页，系统为其重新分配新的一页。
![img28](images\img28.png)
所以总的来说，在HelloWorld运行中，execve 函数返回后，CPU 就跳转到hello程序的第一行开始执行，但由于跳转到的线性地址不存在，所以引发了上述的缺页中断，把硬盘里hello所需要的内容加载到了内存，此时缺页中断返回。
返回后，CPU 会再次尝试跳转到这个线性地址，此时由于缺页中断的处理结果，使得该线性地址已有对应的页表进行映射，所以顺利地映射到了物理地址，也就是hello的代码部分（从硬盘加载过来的），那接下来就终于可以执行hello程序。

### 5.I/O控制
在运行HelloWorld时，主要的I/O控制来自于输入命令和打印结果，当我们在命令行中输入./hello时，发生了什么呢？
```
// console.c
void con_init(void) {
    ...
    set_trap_gate(0x21,&keyboard_interrupt);
    ...
}
```
上面的代码成功将键盘中断绑定在了 keyboard_interrupt 这个中断处理函数上，也就是说当我们按下键盘 'c' 时，CPU 的中断机制将会被触发，最终执行到这个 keyboard_interrupt 函数中。
keyboard_interrupt首先通过 IO 端口操作，从键盘中读取了刚刚产生的键盘扫描码，就是刚刚按下 '.' 的时候产生的键盘扫描码。随后在 key_table 中寻找不同按键对应的不同处理函数，比如普通的一个字母对应的字符 'c' 的处理函数为 do_self，该函数会将扫描码转换为 ASCII 字符码，并将自己放入一个队列里。然后调用do_tty_interrupt函数，实现只要读队列 read_q 不为空，且辅助队列 secondary 没有满，就不断从 read_q 中取出字符，经过一大坨的处理，写入 secondary 队列里。该过程可以简化如下图所示：
![img15](images\img15.png)
 read_q 是键盘按下按键后，进入到键盘中断处理程序 keyboard_interrupt 里，最终通过 put_queue 函数字符放入 read_q 这个队列。
secondary 是 read_q 队列里的未处理字符，通过 copy_to_cooked 函数，经过一定的 termios 规范处理后，将处理过后的字符放入 secondary。
然后，进程通过 tty_read 从 secondary 里读字符，通过 tty_write 将字符写入 write_q，最终 write_q 中的字符可以通过 con_write 这个控制台写函数，将字符打印在显示器上。
这就完成了从键盘输入到显示器输出的一个循环。
然后shell程序会获取我们输入的指令，shell 程序将通过上层的 read 函数调用，来读取这些字符。其中，shell 程序会通过 getcmd 函数最终调用到 read 函数一个字符一个字符读入，直到读到了换行符（\n 或 \r）的时候，才返回。读入的字符在 buf 里，遇到换行符后，这些字符将作为一个完整的命令，传入给 runcmd 函数，真正执行这个命令。我们可以看下这个read函数：
```
// read_write.c
// fd = 0, count = 1
int sys_read(unsigned int fd,char * buf,int count) {
    struct file * file = current->filp[fd];
    // 校验 buf 区域的内存限制
    verify_area(buf,count);
    struct m_inode * inode = file->f_inode;
    // 管道文件
    if (inode->i_pipe)
        return (file->f_mode&1)?read_pipe(inode,buf,count):-EIO;
    // 字符设备文件
    if (S_ISCHR(inode->i_mode))
        return rw_char(READ,inode->i_zone[0],buf,count,&file->f_pos);
    // 块设备文件
    if (S_ISBLK(inode->i_mode))
        return block_read(inode->i_zone[0],&file->f_pos,buf,count);
    // 目录文件或普通文件
    if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
        if (count+file->f_pos > inode->i_size)
            count = inode->i_size - file->f_pos;
        if (count<=0)
            return 0;
        return file_read(inode,file,buf,count);
    }
    // 不是以上几种，就报错
    printk("(Read)inode->i_mode=%06o\n\r",inode->i_mode);
    return -EINVAL;
}
```
很明显，可以看出./hello会被解析为目录文件，然后被读取执行。总体流程如下：
![img16](images\img16.png)

### 6.文件系统
我们都知道，在Linux中一切皆文件。在Linux中，文件管理的起点便是sys_setup中的mount_root，其为加载根文件系统，有了它之后，操作系统才能从一个根儿开始找到所有存储在硬盘中的文件，所以它是文件系统的基石。

该函数就是要把硬盘中的数据，以文件系统的格式进行解读，加载到内存中设计好的数据结构，这样操作系统就可以通过内存中的数据，以文件系统的方式访问硬盘中的一个个文件了。图示如下：
![img17](images\img17.png)
有了这个功能后，下一行 open 函数可以通过文件路径，从硬盘中把一个文件的信息方便地拿到。
```
void init(void) {
    setup((void *) &drive_info);
    (void) open("/dev/tty0",O_RDWR,0);
    (void) dup(0);
    (void) dup(0);
}
```
接下来这个 open 函数要打开文件 /dev/tty0，还有后面的两个dup将file[0]中的内容复制到file[1]和file[2]。open 函数会触发 0x80 中断，最终调用到 sys_open 这个系统调用函数,sys_open实现的功能如下：
![img18](images\img18.png)
进程 1 刚刚创建的时候，是 fork 的进程 0，所以也不具备这样的能力，而通过 setup 加载根文件系统，open 打开 tty0 设备文件等代码，使得进程 1 具备了与外设交互的能力，同时也使得之后从进程 1 fork 出来的进程 2 也天生拥有和进程 1 同样的与外设交互的能力。所以后续的各个进程都在被文件管理。

### 7.总结
总的来说，HelloWorld在操作系统上的过程如下：
1. 用户通过命令行或双击打开的方式告知操作系统执行helloworld程序。
2. 操作系统分析程序
3. 可执行文件的装载：创建一个新的进程，并将helloworld可执行文件映射到该进程结构，表示由该进程执行helloworld程序。
4. cpu上下文切换，操作系统为helloworld程序设置CPU上下文环境，并跳到程序起始指令。
5. 执行helloworld程序的第一条指令，发生缺页异常。
6. 操作系统分配一页物理内存，并将代码从磁盘读入内存，然后继续执行helloworld程序。
7. helloworld程序执行puts函数（系统调用），在显示器上写一字符串。
8. 操作系统找到要将字符串送往的显示设备，通常设备是由一个进程控制的，所以，操作系统将要写的字符串送给该进程。
9. 控制设备的进程告诉设备的窗口系统它要显示字符串，窗口系统确定这是一个合法的操作，然后将字符串转换成像素，将像素写入设备的存储映像区。视频硬件将像素转换成显示器可接收的一组控制/数据信号，显示器解释信号，激发液晶屏。
至此，我们的分析就结束啦。