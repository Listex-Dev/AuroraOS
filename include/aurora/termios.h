#ifndef _AURORA_TERMIOS_H
#define _AURORA_TERMIOS_H

#include <aurora/types.h>

typedef unsigned int   tcflag_t;
typedef unsigned char  cc_t;
typedef unsigned int   speed_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;      /* input modes */
    tcflag_t c_oflag;      /* output modes */
    tcflag_t c_cflag;      /* control modes */
    tcflag_t c_lflag;      /* local modes */
    cc_t     c_line;       /* line discipline */
    cc_t     c_cc[NCCS];   /* control characters */
    speed_t  c_ispeed;     /* input speed */
    speed_t  c_ospeed;     /* output speed */
};

/* c_cc indices */
#define VINTR    0   /* Interrupt (Ctrl-C, default 0x03) */
#define VQUIT    1   /* Quit (Ctrl-\, default 0x1C) */
#define VERASE   2   /* Erase (Backspace, default 0x7F / 0x08) */
#define VKILL    3   /* Kill line (Ctrl-U, default 0x15) */
#define VEOF     4   /* End-of-file (Ctrl-D, default 0x04) */
#define VTIME    5   /* Non-canonical read timeout */
#define VMIN     6   /* Non-canonical read minimum characters */
#define VSTART   8   /* Start flow (Ctrl-Q, default 0x11) */
#define VSTOP    9   /* Stop flow (Ctrl-S, default 0x13) */
#define VSUSP    10  /* Suspend (Ctrl-Z, default 0x1A) */
#define VEOL     11  /* Additional end-of-line */
#define VREPRINT 12  /* Reprint unread chars (Ctrl-R) */
#define VWERASE  14  /* Word erase (Ctrl-W, default 0x17) */

/* c_iflag bits */
#define IGNBRK   0000001
#define BRKINT   0000002
#define IGNPAR   0000004
#define PARMRK   0000010
#define INPCK    0000020
#define ISTRIP   0000040
#define INLCR    0000100
#define IGNCR    0000200
#define ICRNL    0000400
#define IXON     0002000
#define IXOFF    0010000

/* c_oflag bits */
#define OPOST    0000001
#define ONLCR    0000004
#define OCRNL    0000010
#define ONOCR    0000020
#define ONLRET   0000040

/* c_cflag bits */
#define CSIZE    0000060
#define CS8      0000060
#define CSTOPB   0000100
#define CREAD    0000200
#define PARENB   0000400
#define PARODD   0001000
#define HUPCL    0002000
#define CLOCAL   0004000

/* c_lflag bits */
#define ISIG     0000001
#define ICANON   0000002
#define ECHO     0000010
#define ECHOE    0000020
#define ECHOK    0000040
#define ECHONL   0000100
#define NOFLSH   0000200
#define TOSTOP   0000400
#define IEXTEN   0100000

/* Window size structure */
struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/* Terminal ioctl commands (POSIX / Linux compatible) */
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404
#define TCGETA      0x5405
#define TCSETA      0x5406
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414
#define TIOCGPGRP   0x540F
#define TIOCSPGRP   0x5410
#define TIOCSCTTY   0x540E
#define TIOCNOTTY   0x5422
#define FIONREAD    0x541B
#define TIOCGPTN    0x80045430
#define TIOCSPTLCK  0x40045431

/* Virtual Terminal ioctl commands */
#define VT_OPENQRY   0x5600
#define VT_GETMODE   0x5601
#define VT_SETMODE   0x5602
#define VT_GETSTATE  0x5603
#define VT_ACTIVATE  0x5606
#define VT_WAITACTIVE 0x5607

#endif /* _AURORA_TERMIOS_H */
