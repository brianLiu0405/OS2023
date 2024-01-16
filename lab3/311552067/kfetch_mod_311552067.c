#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/ioctl.h>
#include <asm/processor.h>
#include <asm/cpufeature.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>  // For si_meminfo()
#include <linux/mmzone.h> // For PAGE_SIZE

MODULE_LICENSE("Dual BSD/GPL");


#define DEVICE_NAME "kfetch" /* Dev name as it appears in /proc/devices   */ 
#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)
#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1)
#define BUF_LEN 80 /* Max length of the message from the device */

enum { 
    CDEV_NOT_USED = 0, 
    CDEV_EXCLUSIVE_OPEN = 1, 
}; 
 
/* Is device open? Used to prevent multiple access to device */ 
static int major; /* major number assigned to our device driver */ 
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);
static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */
static struct class *cls; 

const int arr[5];
char cpu_model[48];
char kernel_release[48];
unsigned int online_cpus;
unsigned int total_cpus;
unsigned int process_num;
unsigned long total_mem_mb, free_mem_mb;
unsigned long uptime_seconds;

char message[832];

static int kfetch_open(struct inode *inode, struct file *file) 
{ 
    static int counter = 0; 
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) 
        return -EBUSY; 
 
    sprintf(msg, "I already told you %d times Hello world!\n", counter++); 
    try_module_get(THIS_MODULE); 
 
    return 0; 
} 
 
/* Called when a process closes the device file. */ 
static int kfetch_release(struct inode *inode, struct file *file) 
{ 
    /* We're now ready for our next caller */ 
    atomic_set(&already_open, CDEV_NOT_USED); 
    /* Decrement the usage count, or else once you opened the file, you will 
     * never get rid of the module. 
     */ 
    module_put(THIS_MODULE); 
    return 0; 
} 

void get_cpu_model(void){
	unsigned int *v;
    int i, j, cpuid_level;
    cpuid_level = cpuid_eax(0x80000000);
    if (cpuid_level >= 0x80000004) {
        v = (unsigned int *)cpu_model;
        for (i = 0; i < 3; i++) {
            cpuid(0x80000002 + i, &v[4 * i], &v[4 * i + 1], &v[4 * i + 2], &v[4 * i + 3]);
        }
        // Replace non-printable characters with spaces
        for (j = 0; j < sizeof(cpu_model); j++) {
            if (cpu_model[j] < 32 || cpu_model[j] >= 127) {
                cpu_model[j] = 0;
            }
        }
        printk("CPU Model: %s\n", cpu_model);
    } else {
        printk("Unable to get CPU Model: CPUID level too low\n");
    }
}

void get_cpu_num(void)
{
    online_cpus = num_online_cpus();    // Get the number of online CPUs
    total_cpus = num_present_cpus();   // Get the total number of CPUs
    printk("CPUs: %u / %u\n", online_cpus, total_cpus);
}

void get_kernel_release(void){
    struct new_utsname *uts;
    uts = utsname(); // Get the kernel information
	for(int i=0; i<48; i++){
		kernel_release[i] = uts->release[i];
	}
    printk("Kernel Release: %s\n", kernel_release);
}

void get_process_num(void){
	struct task_struct *task;
    process_num = 0;
    // Iterate over the task list
    for_each_process(task) {
        process_num++;
    }
    printk("Number of processes: %d\n", process_num);
}

void get_mem(void){
	struct sysinfo si;
    si_meminfo(&si);
    // Convert memory units from kilobytes to megabytes
    total_mem_mb = (si.totalram * PAGE_SIZE) >> 20;  // totalram is in pages
    free_mem_mb = (si.freeram * PAGE_SIZE) >> 20;    // freeram is in pages
    printk("Memory Information: %lu MB / %lu MB (Free/Total)\n", free_mem_mb, total_mem_mb);
}

void get_uptime(void){
	struct timespec64 ts;
    ktime_get_boottime_ts64(&ts);
    uptime_seconds = ts.tv_sec / 60; // min
    printk("up : %lu\n", uptime_seconds);
}

void print_func(int mask_info, char* message){
    int count = 0;
    char info[6][128];
    char sep_line[65];
	struct new_utsname *uts;
    for(int i=0; i<6; i++){
        for(int j=0; j<128; j++){
            info[i][j] = 0;
        }
    }
    if(mask_info & KFETCH_RELEASE){
        get_kernel_release();
        sprintf(info[count++], "Kernel:  %s", kernel_release);
    }
    if(mask_info & KFETCH_CPU_MODEL){
        get_cpu_model();
        sprintf(info[count++], "CPU:     %s", cpu_model);
    }
    if(mask_info & KFETCH_NUM_CPUS){
        get_cpu_num();
        sprintf(info[count++], "CPUs:    %d / %d", online_cpus, total_cpus);
    }
    if(mask_info & KFETCH_MEM){
        get_mem();
        sprintf(info[count++], "Mem:     %ld MB / %ld MB", free_mem_mb, total_mem_mb);
    }
    if(mask_info & KFETCH_NUM_PROCS){
        get_process_num();
        sprintf(info[count++], "Procs:   %d", process_num);
    }
    if(mask_info & KFETCH_UPTIME){
        get_uptime();
        sprintf(info[count++], "Uptime:  %ld mins", uptime_seconds);
    }

    uts = utsname();
    for(int i=0; i<65; i++){
        if(uts->nodename[i] > 32 && uts->nodename[i] < 127)
            sep_line[i] = '-';
        else
            sep_line[i] = 0;
    }

	sprintf(message,    "                  %s\n"
                        "       .-.        %s\n"
                        "      (.. |       %s\n"
                        "      <>  |       %s\n"
                        "     / --- \\      %s\n"
                        "    ( |   | |     %s\n"
                        "  |\\\\_)___/\\)/\\   %s\n"
                        " <__)------(__/   %s\n",
                 uts->nodename, sep_line, info[0], info[1], info[2], info[3], info[4], info[5]);

	printk(KERN_INFO "%d\n",mask_info);
}

static ssize_t kfetch_read(struct file *filp,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset)
{
    if(message[0] == 0){
        print_func(KFETCH_FULL_INFO, message);
    }
    /* fetching the information */
    if (copy_to_user(buffer, message, 832)) {
        pr_alert("Failed to copy data to user");
        return 0;
    }
	return 832;
}

static ssize_t kfetch_write(struct file *filp,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset)
{
    int mask_info;
	for(int i=0; i<832; i++){
		message[i] = 0;
	}
    if (copy_from_user(&mask_info, buffer, length)) {
        pr_alert("Failed to copy data from user");
        return 0;
    }
    print_func(mask_info, message);
	return 832;
}

const static struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .read    = kfetch_read,
    .write   = kfetch_write,
    .open    = kfetch_open,
    .release = kfetch_release,
};

static __init int kfetch_init(void)
{
    for(int i=0; i<832; i++){
		message[i] = 0;
	}
	major = register_chrdev(0, DEVICE_NAME, &kfetch_ops); 
    if (major < 0) { 
        pr_alert("Registering char device failed with %d\n", major); 
        return major; 
    }
    pr_info("I was assigned major number %d.\n", major);
    cls = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    pr_info("Device created on /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit kfetch_exit(void)
{
	device_destroy(cls, MKDEV(major, 0)); 
    class_destroy(cls); 
    unregister_chrdev(major, DEVICE_NAME); 
}

module_init(kfetch_init);
module_exit(kfetch_exit);