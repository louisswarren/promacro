#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <unistd.h>

void
send_event(int fd, const struct input_event *e)
{
	static const struct input_event syn = {
		.type = EV_SYN,
		.code = SYN_REPORT};

	write(fd, e, sizeof(*e));
	write(fd, &syn, sizeof(syn));
}

int
main(void)
{
	int turbo_x = 0, turbo_y = 0;

	/* Set up virtual keyboard */
	struct uinput_setup st = {
		.name = "Joy Emulated",
		.id.bustype = BUS_USB,
		/* My initials - doesn't matter */
		.id.vendor = 0x4C53,
		.id.product = 0x5757};

	int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (uinput_fd < 0) {
		perror("Could not open /dev/uinput");
		return 1;
	}

	static struct input_event
		b_space = {.type = EV_KEY, .code = KEY_SPACE, .value = 0},
		b_x     = {.type = EV_KEY, .code = KEY_X,     .value = 0};

	if (0 > ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY)
	 || 0 > ioctl(uinput_fd, UI_SET_KEYBIT, b_space.code)
	 || 0 > ioctl(uinput_fd, UI_SET_KEYBIT, b_x.code)
	 || 0 > ioctl(uinput_fd, UI_DEV_SETUP, &st)
	 || 0 > ioctl(uinput_fd, UI_DEV_CREATE)) {
		perror("Failed to setup uinput (ioctl failed)");
		return 1;
	}

	struct pollfd joyinput = {.events = POLLIN, .fd = -1};

retry:
	/* Wait for joystick to be plugged in */
	puts("Waiting for joystick ...");
	while (1) {
		joyinput.fd = open("/dev/input/js0", O_RDONLY);
		if (joyinput.fd >= 0)
			break;
		if (errno != ENOENT) {
			perror("Failed to open /dev/input/js0");
			return 1;
		}
		sleep(1);
	}
	puts("Ready.");

	while (1) {
		int ready = 0;
		while ((turbo_x || turbo_y) && !ready) {
			/* Poll for event */
			ready = poll(&joyinput, 1, 1000 / 30);
			/* Toggle the turbo button */
			b_x.value ^= 1;
			send_event(uinput_fd, &b_x);
		}
		/* If turbo is on, this shouldn't block */
		unsigned char event[8] = {0};
		if (read(joyinput.fd, event, sizeof(event)) < 0) {
			if (errno == ENODEV)
				goto retry;
			perror("Read error");
			return 1;
		}

		/* 16 bit unsigned big-endian */
		unsigned button = (event[6] << 8) | event[7];

		/* 16 bit two's complement little-endian */
		long axis = (event[5] << 8) | event[4];
		if (axis >= 0x8000)
			axis -= 0x10000l;

		switch (button) {
		case 0x104:
			b_space.value = !!axis;
			send_event(uinput_fd, &b_space);
			continue;
			break;
		case 0x203:
			turbo_x = axis <= -0x4000 || axis > 0x4000;
			break;
		case 0x204:
			turbo_y = axis <= -0x4000 || axis > 0x4000;
			break;
		default:
			continue;
		}
		if (!turbo_x && !turbo_y) {
			b_x.value = 0;
			send_event(uinput_fd, &b_x);
		}
	}

	close(joyinput.fd);
	ioctl(uinput_fd, UI_DEV_DESTROY);
	close(uinput_fd);
}
