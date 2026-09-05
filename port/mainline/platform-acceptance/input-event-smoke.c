#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/input/event0";
	struct pollfd pfd;
	int pressed = 0;
	int released = 0;
	int fd;

	fd = open(path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	pfd.fd = fd;
	pfd.events = POLLIN;
	printf("INPUT_EVENT_SMOKE waiting path=%s code=%u timeout=30s\n",
	       path, KEY_RESTART);
	fflush(stdout);

	for (int remaining = 30000; remaining > 0 && !released;) {
		int ret = poll(&pfd, 1, remaining);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			close(fd);
			return 1;
		}
		if (!ret)
			break;

		struct input_event events[16];
		ssize_t bytes = read(fd, events, sizeof(events));

		if (bytes < 0) {
			if (errno == EAGAIN)
				continue;
			perror("read");
			close(fd);
			return 1;
		}
		for (size_t i = 0; i < (size_t)bytes / sizeof(events[0]); i++) {
			if (events[i].type != EV_KEY ||
			    events[i].code != KEY_RESTART)
				continue;
			printf("INPUT type=%u code=%u value=%d\n",
			       events[i].type, events[i].code, events[i].value);
			if (events[i].value == 1)
				pressed = 1;
			if (pressed && events[i].value == 0)
				released = 1;
		}
	}
	close(fd);
	if (pressed && released) {
		puts("INPUT_EVENT_SMOKE PASS");
		return 0;
	}
	printf("INPUT_EVENT_SMOKE FAIL pressed=%d released=%d\n",
	       pressed, released);
	return 1;
}
