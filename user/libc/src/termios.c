#include <termios.h>
#include <sys/ioctl.h>

int tcgetattr(int fd, struct termios *termios_p)
{
    return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
    unsigned long req = TCSETS;
    if (optional_actions == TCSADRAIN) req = TCSETSW;
    else if (optional_actions == TCSAFLUSH) req = TCSETSF;
    return ioctl(fd, req, (void *)termios_p);
}

int cfsetispeed(struct termios *termios_p, speed_t speed)
{
    if (termios_p) {
        termios_p->c_ispeed = speed;
        return 0;
    }
    return -1;
}

int cfsetospeed(struct termios *termios_p, speed_t speed)
{
    if (termios_p) {
        termios_p->c_ospeed = speed;
        return 0;
    }
    return -1;
}
