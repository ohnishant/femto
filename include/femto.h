#ifndef FEMTO_H
#define FEMTO_H 1

#include <termios.h>

enum EditorModes {
    EDITOR_MODE_NORMAL,
    EDITOR_MODE_INSERT,
    EDITOR_MODE_VISUAL,
    EDITOR_MODE_COMMAND,
};

typedef struct editorRow {
    int size;
    char* chars;
} editorRow;

struct EditorConfig {
    struct termios original_termios;
    enum EditorModes editorMode;
    int screenRows;
    int screenCols;
    int cursorX;
    int cursorY;
    int numRows;
    editorRow* rows;
};

struct AppendBuffer {
    char* b;
    int len;
};

enum editorKeys {
    KEY_MOVE_LEFT = 'h',
    KEY_MOVE_DOWN = 'j',
    KEY_MOVE_UP = 'k',
    KEY_MOVE_RIGHT = 'l',
};

void die(const char* s);

// Terminal
int getScreenSize(int* rows, int* cols);
void disableRawMode(void);
void enableRawMode(void);
int getCursorPosition(int* rows, int* cols);
int editorReadKey(void);

// Input
void editorDrawRows(struct AppendBuffer* abuf);
void editorDrawStatusBar(struct AppendBuffer* abuf);
void editorProcessKeypress(void);

// Output
void editorRefreshScreen(void);
void initEditor(void);

// Files I/O
void editorOpen();

#define CTRL_KEY(k) ((k) & 0x1f)

#define ABUF_INIT { NULL, 0 }
int abAppend(struct AppendBuffer* ab, const char* s, int len);

#endif // !FEMTO_H
