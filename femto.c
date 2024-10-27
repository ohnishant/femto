// for getline()
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _BSD_SOURCE

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "femto.h"

#define KILO_VERSION "0.1.0"

struct EditorConfig E;

int abAppend(struct AppendBuffer* ab, const char* s, int len) {
    char* new = realloc(ab->b, ab->len + len);
    if (new == NULL) {
        return 1;
    }
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
    return 0;
}

void abFree(struct AppendBuffer* ab) {
    free(ab->b);
    // I dont know why we dont set the length to 0
}

void die(const char* s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

int getScreenSize(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {

        fprintf(stderr, "[WARN]:\tFailed to get window size... Running Fallback\r\n");

        if (write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            fprintf(stderr, "[FATAL]:\tFailed to get window size... Fallback failed\r\n");
            return -1;
        }
        getCursorPosition(rows, cols);
        fprintf(stderr, "[INFO]:\tFallabck Successful, Rows: %d, Cols: %d\r\n", *rows, *cols);
        return 0;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        fprintf(stderr, "[INFO]:\tGot Window size successfully, Rows: %d, Cols: %d\r\n", *rows, *cols);
        return 0;
    }
}

void disableRawMode() {
    fprintf(stderr, "[INFO]:\tDisabling Raw Mode\r\n");
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    fprintf(stderr, "[INFO]:\tEnabling Raw Mode\r\n");
    if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1) {
        die("tcgetattr");
    }
    atexit(disableRawMode);

    struct termios raw = E.original_termios;

    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // 0 characters needed before read() can return
    raw.c_cc[VMIN] = 0;
    // 1/10 seconds needed before read() can return
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

int getCursorPosition(int* rows, int* cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }
    printf("\r\n");

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        ++i;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }

    editorReadKey();
    return 0;
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {
            switch (seq[1]) {
            case 'A':
                return KEY_MOVE_UP;
            case 'B':
                return KEY_MOVE_DOWN;
            case 'C':
                return KEY_MOVE_RIGHT;
            case 'D':
                return KEY_MOVE_LEFT;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

void editorAppendRow(char* s, size_t len) {
    E.rows = realloc(E.rows, sizeof(editorRow) * (E.numRows + 1));

    int at = E.numRows;

    E.rows[at].size = len;
    E.rows[at].chars = malloc(len + 1);
    memcpy(E.rows[at].chars, s, len);
    E.rows[at].chars[len] = '\0';

    ++E.numRows;
}

void editorOpen(char* filename) {
    FILE* f;
    f = fopen(filename, "r");

    if (f == NULL) {
        fprintf(stderr, "[ERROR]:\tFailed to open file");
        die("fopen");
    }

    char* line = NULL;
    size_t lineCap = 0;
    ssize_t lineLen = 0;

    while (((lineLen = getline(&line, &lineCap, f)) != -1)) {
        while (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r')) {
            lineLen--;
        }

        editorAppendRow(line, lineLen);
    }
    free(line);
    fclose(f);
}

void editorDrawRows(struct AppendBuffer* abuf) {
    for (int i = 0; i < E.screenRows - 1; ++i) {

        if (i >= E.numRows) {
            if (i == E.screenRows / 3 && E.numRows == 0) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo Editor -- Version %s", KILO_VERSION);
                if (welcomelen > E.screenCols) {
                    welcomelen = E.screenCols;
                }
                int padding = (E.screenCols - welcomelen) / 2;
                if (padding) {
                    abAppend(abuf, "~", 1);
                    --padding;
                }
                while (padding--) {
                    abAppend(abuf, " ", 1);
                }
                abAppend(abuf, welcome, welcomelen);
            } else {
                abAppend(abuf, "~", 1);
            }
        } else {
            /*fprintf(stderr, "[INFO]:\tWriting row - \t %s\r\n", E.rows[i].chars);*/
            int len = E.rows[i].size;
            if (len > E.screenCols) {
                len = E.screenCols;
            }

            abAppend(abuf, E.rows[i].chars, len);
        }

        abAppend(abuf, "\x1b[K", 3);
        if (i < E.screenRows - 1) {
            abAppend(abuf, "\r\n", 2);
        }
    }
}

void editorDrawStatusBar(struct AppendBuffer* abuf) {
    abAppend(abuf, "\x1b[7m", 4);
    switch (E.editorMode) {
    case EDITOR_MODE_NORMAL:
        abAppend(abuf, "--NORMAL--", 10);
        break;
    case EDITOR_MODE_INSERT:
        abAppend(abuf, "--INSERT--", 10);
        break;
    case EDITOR_MODE_VISUAL:
        abAppend(abuf, "--VISUAL--", 10);
        break;
    case EDITOR_MODE_COMMAND:
        abAppend(abuf, "--COMMAND--", 11);
        break;
    default:
        abAppend(abuf, "--UNKNOWN EDITOR STATE--", 24);
        break;
    }
    abAppend(abuf, "\x1b[27m", 5);
}

void editorMoveCursor(char key) {
    switch (key) {
    case KEY_MOVE_LEFT:
        if (E.cursorX <= 0) {
            break;
        }
        E.cursorX--;
        break;
    case KEY_MOVE_DOWN:
        // Extra line for statusbar
        if (E.cursorY >= E.screenRows - 2) {
            break;
        }
        E.cursorY++;
        break;
    case KEY_MOVE_RIGHT:
        if (E.cursorX >= E.screenCols - 1) {
            break;
        }
        E.cursorX++;
        break;
    case KEY_MOVE_UP:
        if (E.cursorY <= 0) {
            break;
        }
        E.cursorY--;
        break;
    }
}

void editorProcessKeypress() {
    char c = editorReadKey();

    // TODO: Refactor to handle different modes in a more elegant way
    if (E.editorMode == EDITOR_MODE_NORMAL) {
        switch (c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
        case 'i':
            fprintf(stderr, "[INFO]:\tEntering Insert Mode\r\n");
            E.editorMode = EDITOR_MODE_INSERT;
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            editorMoveCursor(c);
            break;
        }
    } else if (E.editorMode == EDITOR_MODE_INSERT) {
        switch (c) {
        // TODO: Peek next character to make sure ESC is not a part of a sequence
        case '\x1b':
            fprintf(stderr, "[INFO]:\tEntering Normal Mode\r\n");
            E.editorMode = EDITOR_MODE_NORMAL;
            break;
        }
    }
}

void editorRefreshScreen() {
    struct AppendBuffer abuf = ABUF_INIT;

    abAppend(&abuf, "\x1b[?25l", 6);
    abAppend(&abuf, "\x1b[H", 3);

    editorDrawRows(&abuf);
    editorDrawStatusBar(&abuf);

    char buf[32];
    snprintf(buf, 32, "\x1b[%d;%dH", E.cursorY + 1, E.cursorX + 1);
    abAppend(&abuf, buf, strlen(buf));

    abAppend(&abuf, "\x1b[?25h", 6);

    write(STDOUT_FILENO, abuf.b, abuf.len);
    abFree(&abuf);
}

void initEditor() {
    E.cursorX = 10;
    E.cursorY = 0;
    E.editorMode = EDITOR_MODE_NORMAL;
    E.numRows = 0;
    E.rows = NULL;

    if (getScreenSize(&E.screenRows, &E.screenCols) == -1) {
        die("getScreenSize");
    }
}

int main(int argc, char* argv[]) {
    fprintf(stderr, "-----------------------------------------\r\n");
    fprintf(stderr, "\n\n[INFO]:\tKilo Editor Version %s\r\n", KILO_VERSION);
    enableRawMode();
    initEditor();

    if (argc >= 2) {
        fprintf(stderr, "[INFO]:\tOpening File - %s\r\n", argv[1]);
        editorOpen(argv[1]);
    }

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
