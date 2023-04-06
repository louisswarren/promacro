#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <inttypes.h>

#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <unistd.h>

#define die(...) do { \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		exit(1); \
	} while(0)

static const struct input_event
	ev_x_down     = {.type = EV_KEY, .code = KEY_X,     .value = 1},
	ev_x_up       = {.type = EV_KEY, .code = KEY_X,     .value = 0},
	ev_space_down = {.type = EV_KEY, .code = KEY_SPACE, .value = 1},
	ev_space_up   = {.type = EV_KEY, .code = KEY_SPACE, .value = 0},
	ev_syn        = {.type = EV_SYN, .code = SYN_REPORT};

void
send_event(int fd, const struct input_event *e)
{
	write(fd, e, sizeof(*e));
	write(fd, &ev_syn, sizeof(ev_syn));
}

void
send_turbo_button(int fd, int pos)
{
	static int current_pos = 0;
	if (pos < 0) {
		current_pos = !current_pos;
	} else {
		current_pos = pos;
	}

	if (current_pos)
		send_event(fd, &ev_x_down);
	else
		send_event(fd, &ev_x_up);
}

int
main(void)
{
	int turbo_x = 0, turbo_y = 0;

	unsigned char event[8] = {0};

	struct pollfd joyinput;
	joyinput.events = POLLIN;
	joyinput.fd = open("/dev/input/js0", O_RDONLY);
	if (joyinput.fd < 0)
		die("Failed to open /dev/input/js0");

	struct uinput_setup st = {0};
	st.id.bustype = BUS_USB;
	// Just something random
	st.id.vendor = 0x4C53;
	st.id.product = 0x5357;
	strcpy(st.name, "Joy Emulated");

	int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (uinput_fd < 0) {
		die("Could not open /dev/uinput");
	}

	if (-1 == ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY)
	 || -1 == ioctl(uinput_fd, UI_SET_KEYBIT, KEY_X)
	 || -1 == ioctl(uinput_fd, UI_SET_KEYBIT, KEY_SPACE)
	 || -1 == ioctl(uinput_fd, UI_DEV_SETUP, &st)
	 || -1 == ioctl(uinput_fd, UI_DEV_CREATE)) {
		die("ioctl failed");
	}

	while (1) {
		int ready = 0;
		while ((turbo_x || turbo_y) && !ready) {
			/* Poll for event */
			ready = poll(&joyinput, 1, 16);
			/* Toggle the turbo button */
			send_turbo_button(uinput_fd, -1);
		}
		/* If turbo is on, this shouldn't block */
		if (read(joyinput.fd, event, sizeof(event)) < 0) {
			die("Read error");
		}

		uint16_t meta = (event[5] <<  8) | event[4];
		int16_t axis;
		memcpy(&axis, &meta, 2);
		uint16_t button = (event[6] <<  8) | event[7];

		switch (button) {
		case 0x104:
			if (axis)
				send_event(uinput_fd, &ev_space_down);
			else
				send_event(uinput_fd, &ev_space_up);
			break;
		case 0x203:
			if (axis > -0x4000 && axis <= 0x4000) {
				turbo_x = 0;
			} else {
				turbo_x = 1;
			}
			if (!turbo_x && !turbo_y)
				send_turbo_button(uinput_fd, 0);
			break;
		case 0x204:
			if (axis > -0x4000 && axis <= 0x4000) {
				turbo_y = 0;
			} else {
				turbo_y = 1;
			}
			if (!turbo_x && !turbo_y)
				send_turbo_button(uinput_fd, 0);
			break;
		}
	}

	close(joyinput.fd);
	ioctl(uinput_fd, UI_DEV_DESTROY);
	close(uinput_fd);
}
