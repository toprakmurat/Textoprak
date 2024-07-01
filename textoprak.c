/* includes */

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* defines */

#define KILO_VERSION "0.0.1"

#define KILO_TAB_STOP 8
#define DEFAULT_BUFFER_SIZE 80

#define CTRL_KEY(k) ((k) & 0x1f) 

enum editorKey {
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};


// TODO: Implement a status bar


/* data */

typedef struct erow {
	int size;
	int rsize;
	char *chars;
	char *render;
} erow;

struct editorConfig {
	int cx, cy;  // Cursor x and y positions
	int rx;
	int rowoff;  // Row offset
	int coloff;  // Column offset
	int screenrows;
	int screencols;
	int numrows;
	erow *row;  // array of rows
	char *filename;
	char *username;
	char statusmsg[DEFAULT_BUFFER_SIZE];
	time_t statusmsg_time;
	struct termios orig_termios;
};

struct editorConfig E;

/* terminal */

void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	
	perror(s);
	exit(1);
}

void disableRawMode(void) {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) != 0)
		die("tcsetattr");
}

void enableRawMode(void) {
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) != 0)
		die("tcgetattr");

	atexit(disableRawMode);

	// Copy original terminal attributes
	struct termios raw = E.orig_termios;

	// Change some of the flags
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
		die("tcsetattr");
}

int editorReadKey(void) {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}
	if (c == '\x1b') {
		char seq[3];

		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
		
		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';

				if (seq[2] == '~') {
					switch (seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
		} else if (seq[0] == '0') {
			switch (seq[1]) {
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}
		return '\x1b';

	} else {
		return c;
	}
}

int getCursorPosition(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;
	// Query the terminal for status information to get cursor position
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		return -1;

	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}
	buf[i] = '\0';  // null character

	if (buf[0] != '\x1b' || buf[1] != '[') 
		return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
		return -1;

	return 0;
}

int getWindowSize(int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;

		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/* row operations */

int editorRowCxToRx(erow *row, int cx) {
	int rx = 0;
	int j;
	for (j = 0; j < cx; j++) {
		if (row->chars[j] == '\t')
			rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
		rx++;
	}
	return rx;
}

void editorUpdateRow(erow *row) {
	int tabs = 0;
	int j;
	for (j = 0; j < row->size; j++)
		if (row->chars[j] == '\t') tabs++;

	free(row->render);
	row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);  // change here step 83

	// Copy the content of row to render
	int idx = 0;
	for (j = 0; j < row->size; j++) {
		if (row->chars[j] == '\t') {
			row->render[idx++] = ' ';
			while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
		} else {
		row->render[idx++] = row->chars[j];  // change here later
		}
	}
	row->render[idx] = '\0';  // null terminator
	row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

	int at = E.numrows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';  // null terminator
	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	editorUpdateRow(&E.row[at]);

	E.numrows++;
}

/* file i/o */

void editorOpen(char *filename) {
	free(E.filename);
	E.filename = strdup(filename);

	FILE *fp = fopen(filename, "r");
	if (!fp) die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;

	while((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen - 1] == '\n' ||
							   line[linelen - 1] == '\r'))
			linelen--;
		editorAppendRow(line, linelen);
	}
	free(line);
	fclose(fp);
}

/* append buffer */

struct abuf {
	char *b;
	int len;
};

// Append buffer initialization
#define ABUF_INIT {NULL, 0}  // define it with a init funtion later

void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL)
		return;

	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab) {
	free(ab->b);
}

/* output */

void editorScroll(void) {
	E.rx = 0;
	if (E.cy < E.numrows) {
		E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
	}

	if (E.cy < E.rowoff) {
		E.rowoff = E.cy;
	}
	if (E.cy >= E.rowoff + E.screenrows) {
		E.rowoff = E.cy - E.screenrows + 1;
	}
	if (E.rx < E.coloff) {
		E.coloff = E.rx;
	}
	if (E.rx >= E.coloff + E.screencols) {
		E.coloff = E.rx - E.screencols + 1;
	}
}

void editorDrawRows(struct abuf *ab) {
	int y = 0;
	for (y = 0; y < E.screenrows; y++) {
		int filerow = y + E.rowoff;
		if (filerow >= E.numrows) {
			if (E.numrows == 0 && y == E.screenrows / 3) {
				char welcome[DEFAULT_BUFFER_SIZE];
				int welcomelen = snprintf(welcome, sizeof(welcome),
					"Kilo editor -- version %s", KILO_VERSION);
				if (welcomelen > E.screencols)
					welcomelen = E.screencols;

				int padding = (E.screencols - welcomelen) / 2;
				if (padding) {
					abAppend(ab, "~", 1);
					padding--;
				}
				while (padding--) 
					abAppend(ab, " ", 1);

				abAppend(ab, welcome, welcomelen);

			} else {
				abAppend(ab, "~", 1);
			}
		} else {
			int len = E.row[filerow].rsize - E.coloff;
			if (len < 0) len = 0;
			if (len > E.screencols) len = E.screencols;
			abAppend(ab, &E.row[filerow].render[E.coloff], len);
		}

		abAppend(ab, "\x1b[K", 3);
		abAppend(ab, "\r\n", 2);
	}
}

void editorDrawStatusBar(struct abuf *ab) {
	// Switch to inverted color mode
	abAppend(ab, "\x1b[7m", 4);
	
	char status[DEFAULT_BUFFER_SIZE];
	char rstatus[DEFAULT_BUFFER_SIZE];
	
	int len = snprintf(status, sizeof(status), "%.20s - %d lines",
		E.filename ? E.filename : "[No Name]", E.numrows);
	
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
		E.cy + 1, E.numrows);

	if (len > E.screencols) len = E.screencols;
	abAppend(ab, status, len);
	
	while (len < E.screencols) {
		if (E.screencols - len == rlen) {
			abAppend(ab, rstatus, rlen);
			break;
		} else {
			abAppend(ab, " ", 1);
			len++;
		}
	}
	// Switch back to normal mode
	abAppend(ab, "\x1b[m", 3);
	// Make room for the second line of status bar
	abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
	abAppend(ab, "\x1b[7m", 4);
	
	abAppend(ab, "\x1b[K", 3);

	int msglen = strlen(E.statusmsg);
	if (msglen > E.screencols) msglen = E.screencols;

	if (msglen && time(NULL) - E.statusmsg_time < 5) {
		abAppend(ab, E.statusmsg, msglen);	
	} 
	// Let's make a little fun
	else {
		msglen = strlen(E.username);
		abAppend(ab, E.username, msglen);
	}

	for (int i = 0; i < E.screencols - msglen; i++) {
		abAppend(ab, " ", 1);
	}

	abAppend(ab, "\x1b[m", 3);
}

void editorRefreshScreen(void) {
	editorScroll();

	struct abuf ab = ABUF_INIT;

	// Escape character is x1b: 27 in decimal
	// Escape sequences start with escape character and followed by '['
	
	// Hide the cursor 
	abAppend(&ab, "\x1b[?25l", 6);

	// Cursor left at the bottom of the screen
	// Reposition the cursor at the beginning of the screen
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);
	editorDrawStatusBar(&ab);
	editorDrawMessageBar(&ab);
	
	// Moving the cursor
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, 
											  (E.rx - E.coloff) + 1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);  // print out to STDOUT
	abFree(&ab);  // free resources
}

/* Footer */

void editorSetStatusMessage(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vsnprintf(E.statusmsg, sizeof(E.statusmsg), format, ap);
	va_end(ap);
	E.statusmsg_time = time(NULL);
}

char* editorGetUsername(void) {
	char *username = getlogin();
	if (username == NULL) {
		die("getlogin");
	}

	return username;
}

// Only Dune fans could use this editor
void editorSetUsername(const char *username) {
	if (username == NULL) {
		char *tempUsername = "Unknown";
		E.username = strdup(tempUsername);
		return;
	}

	free(E.username);
	E.username = strdup(username);

	char *suffix_msg = " Atreides";
	int len_suffix_msg = strlen(suffix_msg);
	int len_username = strlen(username);

	E.username = realloc(E.username, len_suffix_msg + len_username + 1);  // + 1 null character
	strcat(E.username, suffix_msg);
}

/* input */

void editorMoveCursor(int key) {
	erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

	switch (key) {
		case ARROW_LEFT:
			if (E.cx != 0) {
				E.cx--;
			} else if (E.cx == 0 && E.cy > 0) {
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_RIGHT:
			if (row && E.cx < row->size) {
				E.cx++;
			} else if (row && E.cx == row->size){
				E.cy++;
				E.cx = 0;
			}
			break;
		case ARROW_UP:
			if (E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if (E.cy < E.numrows) {
				E.cy++;
			}
			break;
	}

	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row ? row->size : 0;
	if (E.cx > rowlen) {
		E.cx = rowlen;
	}
}

void editorProcessKeypress(void) {
	int c = editorReadKey();

	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
		case HOME_KEY:
			E.cx = 0;
			break;

		case END_KEY:
			if (E.cy < E.numrows)
				E.cx = E.row[E.cy].size;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				if (c == PAGE_UP) {
					E.cy = E.rowoff;
				} else if (c == PAGE_DOWN) {
					E.cy = E.rowoff + E.screenrows - 1;
					if (E.cy > E.numrows) E.cy = E.numrows;
				}
				int times  = E.screenrows;
				while (times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}

/* init */

void initEditor(void) {
	E.cx = 0;
	E.cy = 0;
	E.rx = 0;
	E.rowoff = 0;
	E.numrows = 0;
	E.row = NULL;
	E.filename = NULL;
	E.username = NULL;
	E.statusmsg[0] = '\0';  // null terminator
	E.statusmsg_time = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1)
		die("getWindowSize");

	E.screenrows -= 2;  // Reserved for status bar
}

int main(int argc, char *argv[]) {
	enableRawMode();
	initEditor();
	if (argc >= 2) {
		editorOpen(argv[1]);
	}

	editorSetStatusMessage("HELP: Ctrl-Q = quit");
	const char *username = editorGetUsername();
	editorSetUsername(username);

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}
