#include "defaults.h"
#include "pal.h"
#include "ram.h"
#include "disk.h"
#include "globals.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef EMULATOR_OS_DOS
#include <conio.h>
#include <dir.h>
#include <dirent.h>
#endif

#ifdef EMULATOR_OS_WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#endif

#define TO_HEX(x)   (x < 10 ? x + 48 : x + 87)

FILE* pal_fopen_r(uint8_t *filename);
int pal_fseek(FILE *file, long delta, int origin);
long pal_ftell(FILE *file);
long pal_fread(void *buffer, long size, long count, FILE *file);
int pal_fclose(FILE *file);

#ifndef EMULATOR_OS_ARDUINO

uint8_t pal_init() {
    return 1;
}

void pal_digital_set(uint16_t ind, uint16_t state) {
}

void pal_pin_set_mode(uint16_t ind, uint16_t mode) {
}

uint16_t pal_digital_get(uint16_t ind) {
    return 0;
}

void pal_analog_set(uint16_t ind, uint16_t state) {
}

uint16_t pal_analog_get(uint16_t ind) {
    return 0;
}

uint8_t pal_load_file(uint8_t *filename, uint16_t address) {
	long l;
    int i;
	FILE *file = pal_fopen_r(filename);
	if(!file) {
		return 1;
	}
	pal_fseek(file, 0, SEEK_END);
	l = pal_ftell(file);

	pal_fseek(file, 0, SEEK_SET);
	for(i=0; i<l; i++) {
		uint8_t b;
		pal_fread(&b, 1, 1, file); // (todo) This can overwrite past RAM space
		ram_write(address+i,b);
	}

	pal_fclose(file);
	return 0;
}

/* Filesystem (disk) abstraction fuctions */
/*===============================================================================*/

uint8_t pal_file_exists(uint8_t *filename) {
	return(!access((const char*)filename, F_OK));
}

FILE* pal_fopen_r(uint8_t *filename) {
	return(fopen((const char*)filename, "rb"));
}

FILE* pal_fopen_w(uint8_t *filename) {
	return(fopen((const char*)filename, "wb"));
}

FILE* pal_fopen_rw(uint8_t *filename) {
	return(fopen((const char*)filename, "r+b"));
}

FILE* pal_fopen_a(uint8_t *filename) {
	return(fopen((const char*)filename, "a"));
}

int pal_fseek(FILE *file, long delta, int origin) {
	return(fseek(file, delta, origin));
}

long pal_ftell(FILE *file) {
	return(ftell(file));
}

long pal_fread(void *buffer, long size, long count, FILE *file) {
	return(fread(buffer, size, count, file));
}

long pal_fwrite(const void *buffer, long size, long count, FILE *file) {
	return(fwrite(buffer, size, count, file));
}

int pal_feof(FILE *file) {
	return(feof(file));
}

int pal_fclose(FILE *file) {
	return(fclose(file));
}

int pal_remove(uint8_t *filename) {
	return(remove((const char*)filename));
}

int pal_rename(uint8_t *name1, uint8_t *name2) {
	return(rename((const char*)name1, (const char*)name2));
}

int pal_select(uint8_t *disk) {
	struct stat st;
	return((stat((char*)disk, &st) == 0) && ((st.st_mode & S_IFDIR) != 0));
}

long pal_file_size(uint8_t *filename) {
	long l = -1;
	FILE *file = pal_fopen_r(filename);
	if (file != NULL) {
		pal_fseek(file, 0, SEEK_END);
		l = pal_ftell(file);
		pal_fclose(file);
	}
	return(l);
}

int pal_open_file(uint8_t *filename) {
	FILE *file = pal_fopen_r(filename);
	if (file != NULL)
		pal_fclose(file);
	return(file != NULL);
}

int pal_make_file(uint8_t *filename) {
	FILE *file = pal_fopen_a(filename);
	if (file != NULL)
		pal_fclose(file);
	return(file != NULL);
}

int pal_delete_file(uint8_t *filename) {
	return(!pal_remove(filename));
}

int pal_rename_file(uint8_t *filename, uint8_t *newname) {
	return(!pal_rename(&filename[0], &newname[0]));
}

#ifdef DEBUG_LOG
void pal_log_buffer(uint8_t *buffer) {
	FILE *file;
#ifdef DEBUG_LOG_TO_CONSOLE
	puts((char *)buffer);
#else
	uint8_t s = 0;
	while (*(buffer + s))   // Computes buffer size
		s++;
	file = pal_fopen_a((uint8_t*)DEBUG_LOG_PATH);
	pal_fwrite(buffer, 1, s, file);
	pal_fclose(file);
#endif
}
#endif

uint8_t pal_read_seq(uint8_t *filename, long fpos) {
	uint8_t result = 0xff;
	uint8_t bytesread;
	uint8_t dmabuf[128];
	uint8_t i;

	FILE *file = pal_fopen_r(&filename[0]);
	if (file != NULL) {
		if (!pal_fseek(file, fpos, 0)) {
			for (i = 0; i < 128; i++)
				dmabuf[i] = 0x1a;
			bytesread = (uint8_t)pal_fread(&dmabuf[0], 1, 128, file);
			if (bytesread) {
				for (i = 0; i < 128; i++)
					ram_write(glb_dma_addr + i, dmabuf[i]);
			}
			result = bytesread ? 0x00 : 0x01;
		} else {
			result = 0x01;
		}
		pal_fclose(file);
	} else {
		result = 0x10;
	}

	return(result);
}

uint8_t pal_write_seq(uint8_t *filename, long fpos) {
	uint8_t result = 0xff;
    int i;

	FILE *file = pal_fopen_rw(&filename[0]);
	if (file != NULL) {
		if (!pal_fseek(file, fpos, 0)) {
			for(i=0; i<128; i++) {
				int8_t b=ram_read(glb_dma_addr+i);
				if (pal_fwrite(&b, 1, 1, file))
					result = 0x00;
				else {
					result = 0xFF;
					break;
				}
			}
		} else {
			result = 0x01;
		}
		pal_fclose(file);
	} else {
		result = 0x10;
	}

	return(result);
}

uint8_t pal_read_rand(uint8_t *filename, long fpos) {
	uint8_t result = 0xff;
	uint8_t bytesread;
	uint8_t dmabuf[128];

	FILE *file = pal_fopen_r(&filename[0]);
	if (file != NULL) {
		if (!pal_fseek(file, fpos, 0)) {
            uint8_t i;
			for (i = 0; i < 128; i++)
				dmabuf[i] = 0x1a;
			bytesread = (uint8_t)pal_fread(&dmabuf[0], 1, 128, file);
			if (bytesread) {
				for (i = 0; i < 128; i++)
					ram_write(glb_dma_addr + i, dmabuf[i]);
			}
			result = bytesread ? 0x00 : 0x01;
		} else {
			result = 0x06;
		}
		pal_fclose(file);
	} else {
		result = 0x10;
	}

	return(result);
}

uint8_t pal_write_rand(uint8_t *filename, long fpos) {
	uint8_t result = 0xff;

	FILE *file = pal_fopen_rw(&filename[0]);
	if (file != NULL) {
		if (!pal_fseek(file, fpos, 0)) {
            uint8_t i;
			for(i=0; i<128; i++) {
				int8_t b=ram_read(glb_dma_addr+i);
				if (pal_fwrite(&b, 1, 1, file))
					result = 0x00;
				else {
					result = 0xFF;
					break;
				}
			}
		} else {
			result = 0x06;
		}
		pal_fclose(file);
	} else {
		result = 0x10;
	}

	return(result);
}

#ifdef EMULATOR_OS_WIN32
uint8_t pal_truncate(char *fn, uint8_t rc) {
	uint8_t result = 0x00;
	LARGE_INTEGER fp;
	fp.QuadPart = rc * 128;
	wchar_t filename[15];
	MultiByteToWideChar(CP_ACP, 0, fn, -1, filename, 4096);
	HANDLE fh = CreateFileW(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		result = 0xff;
	} else {
		if (SetFilePointerEx(fh, fp, NULL, FILE_BEGIN) == 0 || SetEndOfFile(fh) == 0)
			result = 0xff;
	}
	CloseHandle(fh);
	return(result);
}
#endif

#if defined(EMULATOR_OS_POSIX) || defined(EMULATOR_OS_DOS)
uint8_t pal_truncate(char *fn, uint8_t rc) {
	uint8_t result = 0x00;
	if (truncate(fn, rc * 128))
		result = 0xff;
	return(result);
}
#endif

#ifdef EMULATOR_USER_SUPPORT
void pal_make_user_dir() {
	uint8_t d_folder = glb_c_drive + 'A';
	uint8_t u_folder = toupper(TO_HEX(glb_user_code));

	uint8_t path[4] = { d_folder, GLB_FOLDER_SEP, u_folder, 0 };
#ifdef EMULATOR_OS_DOS
	mkdir((char*)path, S_IRUSR | S_IWUSR | S_IXUSR);
#endif
#ifdef EMULATOR_OS_WIN32
	mkdir((char*)path);
#endif
#ifdef EMULATOR_OS_POSIX
	mkdir((char*)path, S_IRUSR | S_IWUSR | S_IXUSR);
#endif
}
#endif


#ifdef EMULATOR_OS_DOS 

void pal_console_init(void) {
}

void pal_console_reset(void) {

}

int pal_kbhit(void) {
    return kbhit();
}

uint8_t pal_getch(void) {
	return getch();
}

uint8_t pal_getche(void) {
	return getche();
}

void pal_clrscr(void) {
    clrscr();
}

void pal_putch(uint8_t ch) {
	putchar(ch);
}

struct ffblk fnd;

uint8_t pal_find_first(uint8_t isdir) {
	uint8_t result = 0xff;
	uint8_t found;

	fcb_hostname_to_fcbname(glb_file_name, glb_pattern);
	found = findfirst((char*)glb_file_name, &fnd, 0);
	if (found == 0) {
		if (isdir) {
			fcb_hostname_to_fcb(glb_dma_addr, (uint8_t*)fnd.ff_name);
			ram_write(glb_dma_addr, 0);	// Sets the user of the requested file correctly on DIR entry
		}
		ram_write(GLB_TMP_FCB_ADDR, glb_file_name[0] - '@');
		fcb_hostname_to_fcb(GLB_TMP_FCB_ADDR, (uint8_t*)fnd.ff_name);
		result = 0x00;
	}
	return(result);
}

uint8_t pal_find_next(uint8_t isdir) {
	uint8_t result = 0xff;
	uint8_t more;

	fcb_hostname_to_fcbname((uint8_t*)fnd.ff_name, glb_fcb_name);
	more = findnext(&fnd);
	if (more == 0) {
		if (isdir) {
			fcb_hostname_to_fcb(glb_dma_addr, (uint8_t*)fnd.ff_name);
			ram_write(glb_dma_addr, 0);	// Sets the user of the requested file correctly on DIR entry
		}
		ram_write(GLB_TMP_FCB_ADDR, glb_file_name[0] - '@');
		fcb_hostname_to_fcb(GLB_TMP_FCB_ADDR, (uint8_t*)fnd.ff_name);
		result = 0x00;
	}
	return(result);
}

#endif

/* Console abstraction functions */
/*===============================================================================*/

#ifdef EMULATOR_OS_WIN32

BOOL _signal_handler(DWORD signal) {
	BOOL result = FALSE;
	if (signal == CTRL_C_EVENT) {
		_ungetch(3);
		result = TRUE;
	}
	return(result);
}

void pal_console_init(void) {
	HANDLE hConsoleHandle = GetStdHandle(STD_INPUT_HANDLE);
	DWORD dwMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;

	SetConsoleMode(hConsoleHandle, dwMode);
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)_signal_handler, TRUE);
}

void pal_console_reset(void) {

}

int pal_kbhit(void) {
    return _kbhit();
}

uint8_t pal_getch(void) {
	return _getch();
}

uint8_t pal_getche(void) {
	return _getche();
}

void pal_clrscr(void) {
    system("cls");
}

void pal_putch(uint8_t ch) {
	_putch(ch);
}


int dir_pos;
WIN32_FIND_DATA find_file_data;
HANDLE h_find;

uint8_t pal_find_next(uint8_t isdir) {
	uint8_t result = 0xff;
	uint8_t found = 0;
	uint8_t more = 1;

	if (dir_pos == 0) {
		h_find = FindFirstFile((LPCSTR)glb_file_name, &find_file_data);
	} else {
		more = FindNextFile(h_find, &find_file_data);
	}

	while (h_find != INVALID_HANDLE_VALUE && more) {	// Skips folders and long file names
		if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			more = FindNextFile(h_find, &find_file_data);
			continue;
		}
		if (find_file_data.cAlternateFileName[0] != 0) {
			if (find_file_data.cFileName[0] != '.')	// Keeps files that are extension only
			{
				more = FindNextFile(h_find, &find_file_data);
				continue;
			}
		}
		found++; dir_pos++;
		break;
	}
	if (found) {
		if (isdir) {
			fcb_hostname_to_fcb(glb_dma_addr, (uint8_t*)&find_file_data.cFileName[0]); // Create fake DIR entry
			ram_write(glb_dma_addr, 0);	// Sets the user of the requested file correctly on DIR entry
		}
		ram_write(GLB_TMP_FCB_ADDR, glb_file_name[0] - '@');							// Set the requested drive onto the tmp FCB
		fcb_hostname_to_fcb(GLB_TMP_FCB_ADDR, (uint8_t*)&find_file_data.cFileName[0]); // Set the file name onto the tmp FCB
		result = 0x00;
	} else {
		FindClose(h_find);
	}
	return(result);
}

uint8_t pal_find_first(uint8_t isdir) {

	dir_pos = 0;
	return(pal_find_next(isdir));
}

#endif

#ifdef EMULATOR_OS_POSIX

#include <ncurses.h>
#include <poll.h>
#include <termios.h>
#include <term.h>

static struct termios _old_term;
static struct termios _new_term;

void pal_console_init(void) {
	tcgetattr(0, &_old_term);

	_new_term = _old_term;

	_new_term.c_lflag &= ~ICANON; /* Input available immediately (no EOL needed) */
	_new_term.c_lflag &= ~ECHO; /* Do not echo input characters */
	_new_term.c_lflag &= ~ISIG; /* ^C and ^Z do not generate signals */
	_new_term.c_iflag &= INLCR; /* Translate NL to CR on input */

	tcsetattr(0, TCSANOW, &_new_term); /* Apply changes immediately */

	setvbuf(stdout, (char *)NULL, _IONBF, 0); /* Disable stdout buffering */
}

void pal_console_reset(void) {
	tcsetattr(0, TCSANOW, &_old_term);
}


int pal_kbhit(void) {
	struct pollfd pfds[1];

	pfds[0].fd = STDIN_FILENO;
	pfds[0].events = POLLIN | POLLPRI | POLLRDBAND | POLLRDNORM;

	return (poll(pfds, 1, 0) == 1) && (pfds[0].revents & (POLLIN | POLLPRI | POLLRDBAND | POLLRDNORM));
}

uint8_t pal_getch(void) {
	return getchar();
}


void pal_putch(uint8_t ch) {
	putchar(ch);
}

uint8_t pal_getche(void) {
	uint8_t ch = pal_getch();

	pal_putch(ch);

	return ch;
}

void pal_clrscr(void) {
	int result;
	setupterm( NULL, STDOUT_FILENO, &result );
	if (result <= 0) return;

	putp(tigetstr( "clear" ) );
}

#include <glob.h>

glob_t pglob;
int dir_pos;

uint8_t pal_find_next(uint8_t isdir)
{
	uint8_t result = 0xff;
#ifdef EMULATOR_USER_SUPPORT
	char dir[6] = { '?', GLB_FOLDER_SEP, 0, GLB_FOLDER_SEP, '*', 0 };
#else
	char dir[4] = { '?', GLB_FOLDER_SEP, '*', 0 };
#endif
	char* dirname;
	int i;
	struct stat st;

	dir[0] = glb_file_name[0];
#ifdef EMULATOR_USER_SUPPORT
	dir[2] = glb_file_name[2];
#endif
	if (!glob(dir, 0, NULL, &pglob)) {
		for (i = dir_pos; i < pglob.gl_pathc; i++) {
			dir_pos++;
			dirname = pglob.gl_pathv[i];
			fcb_hostname_to_fcbname((uint8_t*)dirname, glb_fcb_name);
			if (pal_file_match(glb_fcb_name, glb_pattern) && (stat(dirname, &st) == 0) && ((st.st_mode & S_IFREG) != 0)) {
				if (isdir) {
					fcb_hostname_to_fcb(glb_dma_addr, (uint8_t*)dirname);
					ram_write(glb_dma_addr, 0x00);
				}
				ram_write(GLB_TMP_FCB_ADDR, glb_file_name[0] - '@');
				fcb_hostname_to_fcb(GLB_TMP_FCB_ADDR, (uint8_t*)dirname);
				result = 0x00;
				break;
			}
		}
		globfree(&pglob);
	}

	return(result);
}

uint8_t pal_find_first(uint8_t isdir) {
	dir_pos = 0;    // Set directory search to start from the first position
	fcb_hostname_to_fcbname(glb_file_name, glb_pattern);
	return(pal_find_next(isdir));
}

#endif

#endif
