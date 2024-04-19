#include <asm/segment.h>
#include <errno.h>
#include <string.h>
 
char _myname[24];
 
int sys_iam(const char *name)
{
	int i;
	//临时存储 输入字符串 操作失败时不影响msg
	char tmp[30];
	for(i=0; i<30; i++)
	{
		//从用户态内存取得数据
		tmp[i] = get_fs_byte(name+i);
		if(tmp[i] == '\0') break;  //字符串结束
	}
	//printk(tmp);
	i=0;
	while(i<30&&tmp[i]!='\0') i++;
	int len = i;
	// int len = strlen(tmp);
	//字符长度大于23个
	if(len > 23)
	{
		// printk("String too long!\n");
		return -(EINVAL);  //置errno为EINVAL  返回“­-1”  具体见_syscalln宏展开
	}
	strcpy(_myname,tmp);
	//printk(tmp);
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