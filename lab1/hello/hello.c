#include <linux/kernel.h>
#include <linux/syscalls.h>
SYSCALL_DEFINE0(hello){
    printk("Hello world!\n");
    printk("311552067\n");
    return 0;
}
