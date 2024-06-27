#include <linux/module.h>
#include <linux/init.h>
#include <sound/timer.h>
#include <linux/debugfs.h>

#define NANO_SEC 1000000000UL
#define SNDRV_AUDIOSYNC_GLOBAL_TIMER 0x228

static struct snd_timer *timer;
static bool enabled;

static int snd_audiosync_start(struct snd_timer *t)
{
	enabled = true;
	return 0;
}

static int snd_audiosync_stop(struct snd_timer *t)
{
	enabled = false;
	return 0;
}

static int snd_audiosync_open(struct snd_timer *t)
{
	return 0;
}

static int snd_audiosync_close(struct snd_timer *t)
{
	return 0;
}

static const struct snd_timer_hardware timer_hw = {
	.flags = SNDRV_TIMER_HW_AUTO | SNDRV_TIMER_HW_WORK,
	.open = snd_audiosync_open,
	.close = snd_audiosync_close,
	.start = snd_audiosync_start,
	.stop = snd_audiosync_stop,
};

static ssize_t audiosync_trigger_read(struct file *file, char __user *buf,
				      size_t count, loff_t *ppos)
{
	return 0;
}

static void audiosync_trigger_timer(void)
{
	pr_info("Triggering...\n");
	snd_timer_interrupt(timer, timer->sticks);
}

static ssize_t audiosync_trigger_write(struct file *file, const char __user *buf,
				       size_t count, loff_t *ppos)
{
	bool triggered;
	int err;

	err = kstrtobool_from_user(buf, count, &triggered);
	if (err)
		return err;

	if (!enabled) {
		pr_err("Timer is not enabled!\n");
		return count;
	}

	if (triggered)
		audiosync_trigger_timer();
	
	return count;
}

static const struct file_operations audiosync_trigger_fops = {
	.open = simple_open,
	.llseek = default_llseek,
	.read = audiosync_trigger_read,
	.write = audiosync_trigger_write,
};

static struct dentry *audiosync_dir;
static void init_debugfs_entries(void)
{

	audiosync_dir = debugfs_create_dir("audiosync", NULL);
	debugfs_create_file("trigger", S_IRUGO | S_IWUSR, audiosync_dir, NULL,
			    &audiosync_trigger_fops);
}

static int __init mod_init(void)
{
	int err;

	timer = kmalloc(sizeof(*timer), GFP_KERNEL);
	if (!timer) {
		pr_err("Can't allocate timer\n");
		return -1;
	}

	err = snd_timer_global_new("audiosync", SNDRV_AUDIOSYNC_GLOBAL_TIMER, &timer);
	if (err < 0) {
		pr_err("Can't create global timer\n");
		return -1;
	}

	timer->module = THIS_MODULE;
	timer->hw = timer_hw;
	timer->hw.resolution = LOW_RES_NSEC;
	timer->hw.ticks = NANO_SEC / LOW_RES_NSEC;
	timer->max_instances = 100;

	err = snd_timer_global_register(timer);
	if (err < 0) {
		pr_err("Failed to register a timer\n");
		snd_timer_global_free(timer);
		return err;
	}

	init_debugfs_entries();

	return 0;
}

static void __exit mod_exit(void)
{
	if (timer) {
		snd_timer_global_free(timer);
		timer = NULL;
	}
	debugfs_remove_recursive(audiosync_dir);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Orlov");
module_init(mod_init);
module_exit(mod_exit);
