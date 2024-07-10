#include <linux/module.h>
#include <linux/init.h>
#include <sound/timer.h>
#include <linux/debugfs.h>
#include <sound/minors.h>
#include <sound/core.h>
#include <linux/platform_device.h>

#define NANO_SEC 1000000000UL

static bool enabled;

struct utimer_i {
	struct snd_timer *timer;
	struct list_head node;
	int id;
};

static struct snd_card *card;

static LIST_HEAD(timers);

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


static int timer_id;
static int create_timer(struct snd_userspace_timer __user *_utimer)
{
	struct snd_userspace_timer *utimer;
	struct snd_timer *timer;
	struct snd_timer_id tid;
	struct utimer_i *utimeri;
	int err;
	char timer_name[20];

	sprintf(timer_name, "timer%d", timer_id);

	utimer = memdup_user(_utimer, sizeof(*utimer));
	if (IS_ERR(utimer)) {
		pr_err("Can't get utimer struct from userspace\n");
		return -1;
	}

	timer = kmalloc(sizeof(*timer), GFP_KERNEL);
	if (!timer) {
		pr_err("Can't allocate timer\n");
		err = -ENOMEM;
		goto err_timer_alloc;
	}

	utimer->id = timer_id++;
	tid.dev_sclass = SNDRV_TIMER_SCLASS_APPLICATION;
	tid.dev_class = SNDRV_TIMER_CLASS_CARD;
	tid.card = card->number;
	tid.device = utimer->id;
	tid.subdevice = -1;

	err = snd_timer_new(card, timer_name, &tid, &timer);
	if (err < 0) {
		pr_err("Can't create global timer\n");
		err = -EINVAL;
		goto err_timer_new;
	}

	timer->module = THIS_MODULE;
	timer->hw = timer_hw;
	timer->hw.resolution = NANO_SEC / utimer->rate * utimer->period;
	timer->hw.ticks = 1;
	timer->max_instances = 1000;

	err = snd_device_register(card, timer);
	if (err < 0) {
		pr_err("Failed to register a timer\n");
		snd_device_free(card, timer);
		return err;
	}
	err = copy_to_user(_utimer, utimer, sizeof(*utimer));
	if (err) {
		pr_err("Failed to copy to userspace\n");
		return -EFAULT;
	}

	utimeri = kzalloc(sizeof(*utimeri), GFP_KERNEL);
	if (!utimeri) {
		pr_err("Failed to alloc utimeri\n");
		return -ENOMEM;
	}

	utimeri->timer = timer;
	utimeri->id = utimer->id;
	list_add_tail(&utimeri->node, &timers);
	kfree(utimer);

	return 0;

err_timer_new:
	kfree(timer);
err_timer_alloc:
	kfree(utimer);

	return err;
}

static int audiosync_probe(struct platform_device *pdev)
{
	int err;

	err = snd_devm_card_new(&pdev->dev, -1, "audiosync", THIS_MODULE, 0, &card);
	if (err < 0)
		return err;

	err = snd_card_register(card);
	if (err < 0)
		return err;

	return 0;
}

static void audiosync_remove(struct platform_device *pdev)
{

}

static void audiosync_pdev_release(struct device *dev)
{

}

static struct platform_device audiosync_pdev = {
	.name = "audiosync",
	.dev.release = audiosync_pdev_release,
};

static struct platform_driver audiosync_pdrv = {
	.probe = audiosync_probe,
	.remove_new = audiosync_remove,
	.driver = {
		.name = "audiosync",
	},
};

static int fire_timer(struct snd_userspace_timer __user *_utimer)
{
	struct snd_userspace_timer *utimer;
	struct utimer_i *timer;

	utimer = memdup_user(_utimer, sizeof(*utimer));
	if (IS_ERR(utimer)) {
		pr_err("Fire: failed to get utimer from userspace\n");
		return -EFAULT;
	}

	list_for_each_entry(timer, &timers, node) {
		if (timer->id == utimer->id)
			break;
	}

	snd_timer_interrupt(timer->timer, timer->timer->sticks);

	kfree(utimer);
	return 0;
}

static long audiosync_ioctl(struct file *filp, unsigned int ioctl, unsigned long arg)
{
	int r = -EINVAL;

	switch (ioctl) {
	case SNDRV_TIMER_IOCTL_CREATE:
		return create_timer((struct snd_userspace_timer *)arg);
	case SNDRV_TIMER_IOCTL_FIRE:
		return fire_timer((struct snd_userspace_timer *)arg);
	}

	return r;
}

static const struct file_operations audiosync_fops = {
	.owner = THIS_MODULE,
	.llseek = noop_llseek,
	.unlocked_ioctl = audiosync_ioctl,
};

static struct device *audiosync_dev;
static int __init mod_init(void)
{
	int err;

	err = snd_device_alloc(&audiosync_dev, NULL);
	if (err < 0) {
		pr_info("Can't allocate device\n");
		return err;
	}
	dev_set_name(audiosync_dev, "audiosync");

	err = snd_register_device(SNDRV_DEVICE_TYPE_UTIMER, NULL, 0, &audiosync_fops, NULL, audiosync_dev);
	if (err) {
		pr_err("Failed to register the device: %d\n", err);
		return err;
	}

	platform_device_register(&audiosync_pdev);
	platform_driver_register(&audiosync_pdrv);


	return 0;
}

static void clear_timers(void)
{
	struct utimer_i *cur, *next;
	list_for_each_entry_safe(cur, next, &timers, node) {
		list_del(&cur->node);
		// Right way?
		kfree(cur->timer);
	}

}

static void __exit mod_exit(void)
{
	snd_unregister_device(audiosync_dev);
	platform_driver_unregister(&audiosync_pdrv);
	platform_device_unregister(&audiosync_pdev);
	clear_timers();
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Orlov");
module_init(mod_init);
module_exit(mod_exit);
