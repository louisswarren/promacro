#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <unistd.h>

/* All I wanted to do was have a button on my 8bitdo that makes my retroarch
 * emulator fast-forward. Apparently this is not possible, because retroarch
 * considers this a "hotkey", and to use a hotkey, you have to press and hold
 * the "hotkey enable" button. You can disable having a "hotkey enable" button,
 * but then you can't have any hotkey cobinations at all (e.g. menu, save
 * states, close emulator etc) which is completely useless for me.
 *
 * My solution: make button 0x108 (left stick) press the 'l' keyboard key.
 * Unmap the "hotkey enable" keyboard key (as opposed to gamepad button).
 *
 */

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
		b_fastforward = {.type = EV_KEY, .code = KEY_L,     .value = 0};

	if (0 > ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY)
	 || 0 > ioctl(uinput_fd, UI_SET_KEYBIT, b_fastforward.code)
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
		case 0x108:
			b_fastforward.value = !!axis;
			send_event(uinput_fd, &b_fastforward);
			continue;
			break;
		default:
			continue;
		}
	}

	close(joyinput.fd);
	ioctl(uinput_fd, UI_DEV_DESTROY);
	close(uinput_fd);
}
