# 4.任务管理
### 4.1 任务管理概述
&emsp;&emsp;任务是处理器可以分派、执行和挂起的一个工作单元。它可以用于执行程序、任务或进程、操作系统服务实用程序、中断或异常处理程序，或内核或执行实用程序。
&emsp;&emsp;8086提供了一种保存任务状态、调度任务进行执行以及从一个任务切换到另一个任务的机制。在受保护模式下操作时，所有处理器执行都从任务内执行。即使是简单的系统，也必须至少定义一个任务。更复杂的系统可以使用处理器的任务管理工具来支持多任务处理应用程序。
&emsp;&emsp;描述符表中与任务相关的描述符有两类：任务状态段描述符和任务门。当执行权限传给任何这一类描述符时，都会造成任务切换。
&emsp;&emsp;任务切换很像过程调用，但任务切换会保存更多的处理器状态信息。任务切换会把控制权完全转移到一个新的执行环境，即新任务的执行环境。这种转移操作要求保存处理器中几乎所有寄存器的当前内容，包括标志寄存器EFLAG和所有的段寄存器。任务切换不会把任何信息压入栈中，处理器的状态信息都被保存在内存中称为任务状态段的数据结构中。
#### 4.1.1 任务的结构
任务由两部分组成：
- 任务执行空间：由一个代码段、一个堆栈段和一个或多个数据段组成（请参见图7-1）。如果操作系统或执行人员使用处理器的特权级保护机制，则任务执行空间还将为每个特权级别提供一个单独的堆栈。
- 任务状态段（TSS）：TSS指定了构成任务执行空间的数据段，并为任务状态信息提供了一个存储位置。在多任务处理系统中，TSS还提供了一种连接任务的机制。
**为什么会有多个特权级栈空间？**
答：如果在同一个栈中容纳所有特权级的的数据时，这种交叉引用会使栈变得非常混乱。 并且，用一个栈容纳多个特权级下的数据，栈容量有限，这很容易溢出。

&emsp;&emsp;一个任务由其TSS的段选择器标识。当任务加载到处理器中进行执行时，TSS的段选择器、基本地址、限制和段描述符属性将被加载到任务寄存器中（TR）。
如果为该任务实现了分页，则该任务所使用的页面目录的基本地址将被加载到控制寄存器CR3中。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\1.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 

#### 4.1.2 任务状态
以下项定义了当前正在执行的任务的状态：
- 任务的当前执行空间，由段寄存器(CS、DS、SS、ES、FS和GS)中的段选择器定义。
- 通用寄存器的状态。
- EFLAGS寄存器的状态。
- EIP寄存器的状态。
- 控制寄存器CR3的状态。
- 任务寄存器的状态。
- LDTR寄存器的状态。
- 输入/输出图的基本地址和输入/输出图（包含在TSS中）。
- 指向特权0、1和2堆栈（包含在TSS中）的栈指针。
- 链接到先前执行的任务（包含在TSS中）。

在调度任务之前，除了任务寄存器的状态外，所有这些项目都包含在任务的TSS中。此外，LDTR寄存器的完整内容不包含在TSS中，只包含了LDT的段选择器。
#### 4.1.3 任务的执行
任务的执行方式有以下几种：
1. 使用call指令对任务进行的显式调用。
2. 使用JMP指令显式跳转到任务。
3. 对中断处理程序任务的隐式调用。
4. 对异常处理程序任务的隐式调用。
5. 当在EFLAGS寄存器中设置NT标志时，一个返回（由IRET指令启动）。

 Linux0.00使用JMP指令显式跳转到任务的任务执行方式。

 &emsp;&emsp;所有这些调度任务执行方法都会使用一个指向任务门或任务TSS段的选择符来确定一个任务。当使用CALL或JMP指令调度一个任务时，指令中的选择符既可以直接选择任务的TSS，也可以选择存放有TSS选择符的任务门。当调度一个任务来处理一个中断或异常时，那么 IDT 中该中断或异常表项必须是一个任务门，并且其中含有终端或异常处理任务的TSS选择符。
 &emsp;&emsp;当调度一个任务执行时，当前正在运行任务和调度任务之前会自动地发生任务切换操作。在任务切换期间，当前运行任务的执行环境(称为任务的状态或上下文)会被保存到它的TSS中并且暂停该任务的执行。此后新调度任务的上下文会被加载进处理器中，并且从加载的EIP指向的指令处开始执行新任务。
 &emsp;&emsp;如果当前执行任务(调用者)调用了被调度的新任务(被调用者)，那么调用者的TSS段选择符会被保存在被调用者的TSS中，从而提供了一个返回调用者的链接。对于所有的80X86处理器，任务是不可递归调用的，即任务不能调用或跳转到自己。
&emsp;&emsp;​中断或异常可以通过切换到一个任务来进行处理。在这种情况下，处理器不仅能够执行任务切换来处理中断或异常，而且也会在中断或异常处理任务返回时自动地切换回被中断的任务中去。这种操作方式可以处理在中断任务执行时发生的中断。
​&emsp;&emsp;作为任务切换操作的一部分，处理器也会切换到另一个LDT，从而允许每个任务基于LDT的段具有不同逻辑到物理地址的映射。同时，页目录寄存器CR3也会在切换时被重新加载，因此每个任务可以有自己的一套页表。这些保护措施能够用来隔离各个任务并且防止防止他们相互干扰。

### 4.2 任务的数据结构
处理器定义了5个数据结构来处理与任务相关的活动：
1. **Task-state segment (TSS)**
恢复任务所需的处理器状态信息被保存在一个称为任务状态段（TSS）的系统段中。.图7-2显示了为32位cpu设计的任务的TSS的格式。TSS的字段主要分为两大类：**动态字段和静态字段**。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\2.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 
当任务切换期间挂起时，处理器更新动态字段。以下是动态字段：

- 通用寄存器字段：任务切换前的EAX、ECX、EDX、EBX、ESP、EBP、ESI和EDI寄存器的状态
- 段选择器字段：在任务切换之前，存储在ES、CS、SS、DS、FS和GS寄存器中的段选择器。
- EFLAGS寄存器字段：在任务切换前的EFAGS寄存器的状态。
- EIP（指令指针）字段
- 上一个任务链接字段

处理器读取静态字段，但通常不会更改它们。这些字段是在创建任务时设置的。以下是静态字段：
- LDT段选择器字段:包含任务的LDT的段选择器。
- CR3控制寄存器字段:包含任务要使用的页面目录的基本物理地址。控制寄存器CR3也被称为页面目录基寄存器（PDBR）。
- 特权级别为0、1和2的堆栈指针字段:这些堆栈指针由堆栈段的段选择器（SS0、SS1和SS2）组成的逻辑地址和进入堆栈的偏移量（ESP0、ESP1和ESP2）组成。
- T（调试陷阱）标志（第100字节，第0位）
- 输入地图基本地址字段:包含从TSS的base到I/O权限位图和中断重定向位图的16位偏移量。

2. **TSS描述符**
与所有其他段一样，TSS也由段描述符定义。图7-3显示了TSS描述符的格式。TSS描述符只能放置在GDT中；它们不能放置在LDT或IDT中。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\3.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 

&emsp;&emsp;类型字段中的繁忙标志(B)指示任务是否繁忙。一个繁忙的任务当前正在运行或正在暂停。值为1001B的类型字段表示非活动任务；值为1011B表示繁忙任务。任务不是递归的。处理器使用繁忙标志来检测调用执行已中断的任务的尝试。为了确保只有一个繁忙的标志与一个任务相关联，每个TSS应该只有一个指向它的TSS描述符。当32位TSS的TSS描述符中的G标志为0时，限制字段的值必须等于或大于67H

&emsp;&emsp;任何具有访问TSS描述符的程序或过程（即其CPL在数值上等于或小于TSS描述符的DPL）都可以通过调用或跳转来分派任务。
在大多数系统中，TSS描述符的DPL被设置为小于3，因此只有特权软件才能执行任务切换。但是，在多任务处理应用程序中，一些TSS描述符的DPL可以设置为3，以允许在应用程序（或用户）权限级别上进行任务切换。

3. **任务寄存器**
任务寄存器为当前任务的TSS的16位段选择器和整个段描述符.此信息从当前GDT中的任务的TSS描述符复制。图7-5显示了处理器用于访问TSS的路径（使用任务寄存器中的信息）。

<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\4.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 
&emsp;&emsp;任务寄存器具有可见部分（可由软件读取和更改）和不可见部分（由处理器维护，软件无法访问）。可见部分中的段选择器指向GDT中的TSS描述符。处理器使用任务寄存器的不可见部分来缓存TSS的段描述符。在寄存器中缓存这些值可以使任务的执行效率更高。LTR（加载任务寄存器）和STR（存储任务寄存器）指令加载并读取任务寄存器的可见部分：

&emsp;&emsp;LTR指令将一个段选择器（源操作数）加载到任务寄存器中，该任务寄存器指向GDT中的一个TSS描述符。然后，它用来自TSS描述符的信息加载任务寄存器的不可见部分。LTR是一种特权指令，仅在CPL为0时才能执行。在系统初始化期间，它被用于在任务寄存器中放置一个初始值。然后，当任务切换发生时，将隐式地更改任务寄存器的内容。
&emsp;&emsp;STR（存储任务寄存器）指令将任务寄存器的可见部分存储在通用寄存器或存储器中。此指令可以通过在任何特权级别上运行的代码来执行，以识别当前正在运行的任务。**但是，它通常只被操作系统软件使用。**当处理器通电或复位时，段选择器和基本地址设置为默认值0；限制设置为FFFFH。

4. **任务门描述符 Task-Gate Descriptor**
任务门描述符提供了对任务的间接的、受保护的引用（请参见图7-6）。它可以被放置在GDT、LDT或IDT中。任务门描述符中的TSS段选择器字段指向GDT中的TSS描述符。不使用此线段选择器中的RPL。

<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\5.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 

&emsp;&emsp;当程序或过程通过任务门进行调用或跳转到任务时，指向任务门的门选择器的CPL和RPL字段必须小于或等于任务门描述符的DPL。请注意，当使用任务门时，将不使用目标TSS描述符的DPL。
&emsp;&emsp;任务可以通过任务门描述符或TSS描述符来访问。图7-7说明了LDT中的任务门、GDT中的任务门以及IDT中的任务门如何都可以指向同一任务。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\6.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 

### 4.3 任务切换
处理器在四种情况下将执行转移到另一个任务：
- 当前的程序、任务或过程对GDT中的TSS描述符执行JMP或CALL指令。
- 当前程序、任务或过程对GDT或当前LDT中的任务门描述符执行JMP或CALL指令。
- 一个中断或异常向量指向IDT中的一个任务门描述符。
- 当设置了EFLAGS寄存器中的NT标志时，当前任务将执行一个IRET。

当切换到新任务时，处理器会执行以下操作：
1. 从任务门或前一个任务链接字段获取新任务的TSS段选择器，作为JMP或CALL指令的操作数。
2. 检查是否允许当前（旧的）任务切换到新任务。
3. 检查新任务的TSS描述符是否被标记为存在，并具有一个有效的限制（大于或等于67H）。
4. 检查新任务是否可用（调用、跳转、异常或中断）或是否繁忙(IRET返回)。
5. 检查当前（旧的）TSS、新的TSS以及在任务开关中使用的所有段描述符是否已被分页到系统内存中。
6. 如果任务切换使用JMP或IRET指令启动，处理器将清除当前（旧）任务的TSS描述符中的繁忙(B)标志；如果使用CALL指令、异常或中断启动繁忙(B)标志：将设置繁忙(B)标志。
7. 如果任务切换是用IRET指令启动的，则处理器将清除临时保存的EFLAGS寄存器映像中的NT标志；如果使用CALL或JMP指令、异常或中断启动，则已保存的EFLAGS映像中的NT标志保持不变。
8. 在当前任务的TSS中保存当前（旧的）任务的状态
9. 如果任务切换由CALL指令、异常或中断启动，处理器将在从新任务加载的EFLAGS中设置NT标志。如果使用IRET指令或JMP指令启动，NT标志将反映从新任务加载的EFLAGS中的NT的状态
10. 如果任务切换由CALL指令、JMP指令、异常或中断启动，处理器将在新任务的TSS描述符中设置繁忙(B)标志；如果使用IRET指令启动，则将设置繁忙(B)标志。
11. 使用新任务的TSS的段选择器和描述符加载任务寄存器。
12. TSS状态将被加载到处理器中。
13. 与线段选择器相关联的描述符将被加载并进行限定。
14. 
中断或异常本身并不直接引发任务切换，但操作系统可以根据需要在中断处理程序中执行任务切换，以满足特定的需求和策略。

### 4.4 任务链
Q：如何判断任务是否嵌套？
A：EFLAGS.NT = 1时代表当前正在执行的任务嵌套在另一个任务的执行中。

Q：什么情况会发生任务嵌套？
A：当CALL指令、中断或异常导致任务切换时：处理器将当前TSS的段选择器复制到新任务的TSS的前一个任务链接字段；然后设置EFLAGS.NT = 1.如果软件使用IRET指令暂停新任务，处理器将检查EFLAGS.NT = 1；然后，它使用前一个任务链接字段中的值返回到上一个任务。见图7-8。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\7.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 

当JMP指令导致任务切换时，新任务不嵌套.

任务嵌套时修改了哪些标志位？
Answer:在任务嵌套时，下面列出了在x86体系结构的任务切换中修改的关键标志位和字段:
- EFLAGS.NT (Nested Task) 标志位： 当一个任务嵌套在另一个任务内部时，EFLAGS 寄存器的 NT 标志被设置为 1。这表明当前执行的任务是在另一个任务的内部执行。这个标志位用于指示任务嵌套状态。
- TSS 中的 Previous Task Link Field（也称为“backlink”）： TSS（任务状态段）中的前一个任务链接字段用于指向前一个任务的TSS。当通过CALL指令、中断或异常引发任务切换时，处理器将当前TSS的段选择器复制到新任务的TSS的前一个任务链接字段中。
- TSS 中的 Busy Flag（B标志）： TSS的段描述符中包含一个Busy Flag，用于防止递归任务切换。当任务被分派（dispatched）时，处理器设置新任务的Busy Flag。如果当前任务在嵌套链中，Busy Flag将保持设置。当切换到新任务时，如果新任务的Busy Flag已经设置，处理器会引发通用保护异常（#GP）。在使用IRET指令进行任务切换时，不会引发异常。

Q：任务嵌套时，如何返回前一任务？
A：任务嵌套时返回前一任务的过程如下：
1. 当通过CALL指令、中断或异常引发任务切换时，处理器会将前一个任务的TSS段选择器复制到新任务的TSS的前一个任务链接字段中。
2. 在EFLAGS寄存器中，EFLAGS.NT (Nested Task) 标志被设置为1，以指示当前执行的任务是在另一个任务的内部执行，即任务嵌套状态。
3. 当使用IRET指令返回到前一任务时，处理器会检查EFLAGS.NT标志是否为1。如果EFLAGS.NT为1，处理器会使用前一个任务链接字段中的TSS段选择器来返回到前一任务。
在任务嵌套的情况下，通过设置EFLAGS.NT标志和维护前一个任务链接字段，处理器可以正确返回到前一任务。
### 4.5 任务地址空间
Q:什么是任务地址空间？
A:任务地址空间是任务可以访问的段的集合，这些段包括在TSS中引用的代码、数据、堆栈和系统段，以及任务代码访问的任何其他段。这些段被映射到处理器的线性地址空间，然后又被映射到处理器的物理地址空间（直接或通过分页）

Q:任务地址空间包括什么？
A：任务地址空间包括以下部分：
- 代码段：这是任务的程序代码存储的地方。
- 数据段：这是任务的变量和数据结构存储的地方。
- 堆栈段：这是任务的堆栈存储的地方，用于支持函数调用、局部变量存储等。
- 系统段：这些是由操作系统使用的特殊段，例如任务状态段（TSS）和局部描述符表（LDT）。
此外，任务地址空间还可能包括其他由任务代码访问的段。所有这些段都被映射到处理器的线性地址空间，然后又被映射到处理器的物理地址空间（直接或通过分页）。
如果启用了分页，每个任务可以有自己的页表集，用于将线性地址映射到物理地址。或者，几个任务可以共享同一组页表。这种映射方式可以通过使用不同的页目录为每个任务实现，因为在任务切换时会加载PDBR（控制寄存器CR3），所以每个任务可能有一个不同的页目录。

Q：了解把任务映射到线性和物理地址空间的方法？
A：任务可以通过以下两种方式映射到线性地址空间和物理地址空间：
1. 所有任务共享一个线性到物理地址空间的映射：当分页未启用时，这是唯一的选择。没有分页，所有线性地址都映射到相同的物理地址。当启用分页时，通过为所有任务使用一个页目录，可以获得这种线性到物理地址空间的映射。如果支持需求分页虚拟内存，线性地址空间可能超过可用的物理空间。
2. 每个任务都有自己的线性地址空间，该空间映射到物理地址空间：通过为每个任务使用不同的页目录来实现这种映射。因为在任务切换时会加载PDBR（控制寄存器CR3），所以每个任务可能有一个不同的页目录。不同任务的线性地址空间可能映射到完全不同的物理地址。如果不同页目录的条目指向不同的页表，并且页表指向物理内存的不同页面，那么任务就不会共享物理地址。
   
无论采用哪种方法映射任务线性地址空间，所有任务的TSS必须位于物理空间的共享区域，所有任务都可以访问。这种映射是必需的，以便在处理器在任务切换期间读取和更新TSS时，TSS地址的映射不会改变。由GDT映射的线性地址空间也应该映射到物理空间的共享区域；否则，GDT的目标就会被无效。
图7-9显示了两个任务的线性地址空间如何通过共享页表在物理空间中重叠。
<div>			<!--块级封装-->
    <center>	<!--将图片和文字居中-->
    <img src=".\images\8.png"
         alt="图片失踪了"
    />
    <!--标题-->
    </center>
</div> 

Q：了解任务逻辑地址空间，及如何在任务之间共享数据的方法？
A：任务的逻辑地址空间是由任务可以访问的所有段组成的，这些段包括代码段、数据段、堆栈段和系统段。这些段都被映射到处理器的线性地址空间，然后又被映射到处理器的物理地址空间。
在任务之间共享数据的方法有几种：
1. **使用全局描述符表（GDT）**：所有任务都可以访问GDT，因此可以通过在此表中的段描述符创建共享段。
2. **使用相同的局部描述符表（LDT）**：几个任务可以使用相同的LDT。这是一种内存高效的方式，可以让特定的任务相互通信或控制，而不会降低整个系统的保护屏障。
3. **使用相同的页表**：如果启用了分页，那么几个任务可以共享同一组页表。这样，这些任务就可以共享相同的线性到物理地址映射。
4. **使用共享内存区域**：在物理内存中创建一个区域，所有任务都可以访问。这需要操作系统提供某种机制来管理对共享内存区域的访问，以防止并发访问问题。
   
请注意，虽然这些方法可以在任务之间共享数据，但必须谨慎使用，以防止数据竞争和其他并发问题。在多任务环境中共享数据通常需要某种形式的同步机制，如互斥锁或信号量。

