#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/string.h>
#include <linux/uaccess.h>
SYSCALL_DEFINE2(revstr, int, len, char __user *, src){
    char str_in[100];
    char reverse[100];
    if( copy_from_user(str_in, src, len)){
        return -EFAULT;
    }
    for(int i=0; i<len; i++){
        reverse[i] = str_in[(len-1) - i];
    }
    str_in[len] = '\0';
    reverse[len] = '\0';
    printk("The origin string: %s\n", str_in);
    printk("The reversed string: %s\n", reverse);
    return 0;
}