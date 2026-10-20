#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define MAX_LINES 1024
#define MAX_LINE_LEN 256

static char lines[MAX_LINES][MAX_LINE_LEN];
static int line_count = 0;
static int cursor_row = 0;
static int cursor_col = 0;
static char filename[256];
static int modified = 0;
static struct termios orig_termios;
static int raw_mode_active = 0;

static void disable_raw_mode(void)
{
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = 0;
    }
}

static void enable_raw_mode(void)
{
    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        raw_mode_active = 1;
        atexit(disable_raw_mode);
    }
}

static void load_file(const char *path)
{
    FILE *f = fopen(path, "r");
    line_count = 0;
    if (!f) {
        /* New file, initialize with one empty line */
        lines[0][0] = '\0';
        line_count = 1;
        return;
    }

    char buf[MAX_LINE_LEN];
    while (fgets(buf, sizeof(buf), f) && line_count < MAX_LINES) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        strncpy(lines[line_count], buf, MAX_LINE_LEN - 1);
        lines[line_count][MAX_LINE_LEN - 1] = '\0';
        line_count++;
    }
    fclose(f);
    if (line_count == 0) {
        lines[0][0] = '\0';
        line_count = 1;
    }
}

static int save_file(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < line_count; i++) {
        fputs(lines[i], f);
        fputc('\n', f);
    }
    fclose(f);
    modified = 0;
    return 0;
}

static void render_screen(const char *status_msg)
{
    if (!isatty(STDOUT_FILENO)) return;

    /* Clear screen and go home */
    printf("\033[2J\033[H");

    for (int i = 0; i < 23 && i < line_count; i++) {
        printf("%s\r\n", lines[i]);
    }
    for (int i = line_count; i < 23; i++) {
        printf("~\r\n");
    }

    /* Status bar */
    printf("\033[7m\"%s\" %dL %s\033[0m\r\n",
           filename, line_count, status_msg ? status_msg : (modified ? "[Modified]" : ""));

    /* Move cursor to row, col */
    printf("\033[%d;%dH", cursor_row + 1, cursor_col + 1);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *cmd_arg = NULL;
    filename[0] = '\0';

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cmd_arg = argv[++i];
        } else if (filename[0] == '\0') {
            strncpy(filename, argv[i], sizeof(filename) - 1);
        }
    }

    if (filename[0] == '\0') {
        strcpy(filename, "untitled.txt");
    }

    load_file(filename);

    /* Batch/Command execution mode (e.g. vi -c "wq" file.txt or scripted commands) */
    if (cmd_arg) {
        if (strstr(cmd_arg, "w")) {
            save_file(filename);
        }
        return 0;
    }

    /* If not interactive terminal, support piped/batch edit or saving directly */
    if (!isatty(STDIN_FILENO)) {
        char buf[MAX_LINE_LEN];
        line_count = 0;
        while (fgets(buf, sizeof(buf), stdin) && line_count < MAX_LINES) {
            size_t len = strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
            strncpy(lines[line_count++], buf, MAX_LINE_LEN - 1);
        }
        save_file(filename);
        return 0;
    }

    enable_raw_mode();
    render_screen("AuroraOS vi - Normal Mode. Press 'i' to insert, ':wq' to save and exit");

    int mode = 0; /* 0 = Normal, 1 = Insert, 2 = Command */
    char cmd_buf[64];
    int cmd_pos = 0;

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;

        if (mode == 0) {
            /* Normal Mode */
            if (c == 'q') {
                break;
            } else if (c == 'i') {
                mode = 1;
                render_screen("-- INSERT --");
            } else if (c == 'a') {
                if (cursor_col < (int)strlen(lines[cursor_row])) cursor_col++;
                mode = 1;
                render_screen("-- INSERT --");
            } else if (c == 'h') {
                if (cursor_col > 0) cursor_col--;
                render_screen(NULL);
            } else if (c == 'l') {
                if (cursor_col < (int)strlen(lines[cursor_row])) cursor_col++;
                render_screen(NULL);
            } else if (c == 'k') {
                if (cursor_row > 0) cursor_row--;
                if (cursor_col > (int)strlen(lines[cursor_row])) cursor_col = strlen(lines[cursor_row]);
                render_screen(NULL);
            } else if (c == 'j') {
                if (cursor_row < line_count - 1) cursor_row++;
                if (cursor_col > (int)strlen(lines[cursor_row])) cursor_col = strlen(lines[cursor_row]);
                render_screen(NULL);
            } else if (c == 'x') {
                int len = strlen(lines[cursor_row]);
                if (len > 0 && cursor_col < len) {
                    memmove(&lines[cursor_row][cursor_col],
                            &lines[cursor_row][cursor_col + 1],
                            len - cursor_col);
                    modified = 1;
                    render_screen(NULL);
                }
            } else if (c == 'o') {
                if (line_count < MAX_LINES) {
                    for (int i = line_count; i > cursor_row + 1; i--) {
                        strcpy(lines[i], lines[i - 1]);
                    }
                    cursor_row++;
                    lines[cursor_row][0] = '\0';
                    line_count++;
                    cursor_col = 0;
                    modified = 1;
                    mode = 1;
                    render_screen("-- INSERT --");
                }
            } else if (c == ':') {
                mode = 2;
                cmd_pos = 0;
                cmd_buf[0] = '\0';
                render_screen(":");
            }
        } else if (mode == 1) {
            /* Insert Mode */
            if (c == 27) { /* ESC */
                mode = 0;
                if (cursor_col > 0) cursor_col--;
                render_screen(NULL);
            } else if (c == 127 || c == 8) { /* Backspace */
                if (cursor_col > 0) {
                    int len = strlen(lines[cursor_row]);
                    memmove(&lines[cursor_row][cursor_col - 1],
                            &lines[cursor_row][cursor_col],
                            len - cursor_col + 1);
                    cursor_col--;
                    modified = 1;
                    render_screen("-- INSERT --");
                }
            } else if (c == '\r' || c == '\n') {
                if (line_count < MAX_LINES) {
                    for (int i = line_count; i > cursor_row + 1; i--) {
                        strcpy(lines[i], lines[i - 1]);
                    }
                    int cur_len = strlen(lines[cursor_row]);
                    if (cursor_col > cur_len) cursor_col = cur_len;
                    int rem_len = strlen(&lines[cursor_row][cursor_col]);
                    strcpy(lines[cursor_row + 1], &lines[cursor_row][cursor_col]);
                    lines[cursor_row][cursor_col] = '\0';
                    cursor_row++;
                    cursor_col = 0;
                    line_count++;
                    modified = 1;
                    (void)rem_len;
                    render_screen("-- INSERT --");
                }
            } else if (c >= 32 && c <= 126) {
                int len = strlen(lines[cursor_row]);
                if (cursor_col > len) cursor_col = len;
                if (len < MAX_LINE_LEN - 2) {
                    memmove(&lines[cursor_row][cursor_col + 1],
                            &lines[cursor_row][cursor_col],
                            len - cursor_col + 1);
                    lines[cursor_row][cursor_col] = c;
                    cursor_col++;
                    modified = 1;
                    render_screen("-- INSERT --");
                }
            }
        } else if (mode == 2) {
            /* Command Mode */
            if (c == '\r' || c == '\n') {
                cmd_buf[cmd_pos] = '\0';
                if (strcmp(cmd_buf, "w") == 0) {
                    save_file(filename);
                    mode = 0;
                    render_screen("Written");
                } else if (strcmp(cmd_buf, "q") == 0) {
                    break;
                } else if (strcmp(cmd_buf, "wq") == 0 || strcmp(cmd_buf, "x") == 0) {
                    save_file(filename);
                    break;
                } else if (strcmp(cmd_buf, "q!") == 0) {
                    break;
                } else {
                    mode = 0;
                    render_screen("Unknown command");
                }
            } else if (c == 27) {
                mode = 0;
                render_screen(NULL);
            } else if (c == 127 || c == 8) {
                if (cmd_pos > 0) cmd_buf[--cmd_pos] = '\0';
                char msg[80];
                snprintf(msg, sizeof(msg), ":%s", cmd_buf);
                render_screen(msg);
            } else if (cmd_pos < (int)sizeof(cmd_buf) - 1) {
                cmd_buf[cmd_pos++] = c;
                cmd_buf[cmd_pos] = '\0';
                char msg[80];
                snprintf(msg, sizeof(msg), ":%s", cmd_buf);
                render_screen(msg);
            }
        }
    }

    disable_raw_mode();
    printf("\033[2J\033[H");
    return 0;
}
