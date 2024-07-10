#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sound/asound.h>

int main(void)
{
	int fd, ret;
	
	fd = open("/dev/snd/audiosync", O_RDWR | O_CLOEXEC);

	struct snd_userspace_timer timer = {
		.rate = 8000,
		.period = 4410,
		.id = -1,
	};

	ret = ioctl(fd, SNDRV_TIMER_IOCTL_CREATE, &timer);
	printf("Hello, World! %d\n", ret);
	printf("The new timer id is %d\n", timer.id);


	close(fd);
	return 0;
}
