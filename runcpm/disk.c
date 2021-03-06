#include "defaults.h"
#include "globals.h"
#include "cpu.h"
#include "ram.h"
#include "pal.h"
#include "disk.h"

#include <ctype.h>

#define TO_HEX(x)   (x < 10 ? x + 48 : x + 87)

#define CPM_FCB_DR(addr) (addr)
#define CPM_FCB_FN(addr) (addr+1)
#define CPM_FCB_TP(addr) (CPM_FCB_FN(addr)+8)
#define CPM_FCB_EX(addr) (CPM_FCB_TP(addr)+3)
#define CPM_FCB_S1(addr) (CPM_FCB_EX(addr)+1)
#define CPM_FCB_S2(addr) (CPM_FCB_S1(addr)+1)
#define CPM_FCB_RC(addr) (CPM_FCB_S2(addr)+1)
#define CPM_FCB_AL(addr) (CPM_FCB_RC(addr)+1)
#define CPM_FCB_CR(addr) (CPM_FCB_AL(addr)+16)
#define CPM_FCB_R0(addr) (CPM_FCB_CR(addr)+1)
#define CPM_FCB_R1(addr) (CPM_FCB_R0(addr)+1)
#define CPM_FCB_R2(addr) (CPM_FCB_R1(addr)+1)

#define DISK_ERR_WRITE_PROTECT 1
#define DISK_ERR_SELECT 2

#define IS_RW(fcbaddr)  (glb_ro_vector & (1 << ram_read(CPM_FCB_DR(fcbaddr))))

#define DISK_BLK_SZ 128 // CP/M block size
#define DISK_BLK_EX 128 // Number of blocks on an extension
#define DISK_BLK_S2 4096    // Number of blocks on a S2 (module)
#define DISK_MAX_CR 128 // Maximum value the CR field can take
#define DISK_MAX_RC 127 // Maximum value the RC field can take
#define DISK_MAX_EX 31  // Maximum value the EX field can take
#define DISK_MAX_S2 15  // Maximum value the S2 (modules) field can take - Can be set to 63 to emulate CP/M Plus

static void _error(uint8_t error) {
	pal_puts("\r\nBDOS Error on ");
	pal_put_con('A' + glb_c_drive);
	pal_puts(" : ");
	switch (error) {
	case DISK_ERR_WRITE_PROTECT:
		pal_puts("R/O");
		break;
	case DISK_ERR_SELECT:
		pal_puts("Select");
		break;
	default:
		pal_puts("??");
		break;
	}
	pal_puts("\r\n");
	glb_c_drive = glb_o_drive;
	ram_write(0x0004, (ram_read(0x0004) & 0xf0) | glb_o_drive);
	cpu_status = 2;
}

int disk_select_disk(uint8_t dr) {
	uint8_t result = 0xff;
	uint8_t disk[2] = "A";

	if (!dr) {
		dr = glb_c_drive; // This will set dr to defDisk in case no disk is passed
	} else {
		dr--;       // Called from BDOS, set dr back to 0=A: format
	}

	disk[0] += dr;
	if (pal_select(&disk[0])) {
		glb_login_vector = glb_login_vector | (1 << (disk[0] - 'A'));
		result = 0x00;
	} else {
		_error(DISK_ERR_SELECT);
	}

	return (result);
}

uint8_t fcb_to_hostname(uint16_t fcbaddr, uint8_t *file_name) {
	uint8_t add_dot = 1;
	uint8_t i = 0;
	uint8_t unique = 1;

	uint8_t dr = ram_read(CPM_FCB_DR(fcbaddr));
	if (dr) {
		*(file_name++) = (dr - 1) + 'A';
	} else {
		*(file_name++) = glb_c_drive + 'A';
	}
	*(file_name++) = GLB_FOLDER_SEP;

#ifdef EMULATOR_USER_SUPPORT
	*(file_name++) = toupper(TO_HEX(glb_user_code));
	*(file_name++) = GLB_FOLDER_SEP;
#endif

	while (i < 8) {
		uint8_t fn = ram_read(CPM_FCB_FN(fcbaddr) + i);
		if (fn > 32)
			*(file_name++) = toupper(fn);
		if (fn == '?')
			unique = 0;
		i++;
	}
	i = 0;
	while (i < 3) {
		uint8_t tp = ram_read(CPM_FCB_TP(fcbaddr) + i);
		if (tp > 32) {
			if (add_dot) {
				add_dot = 0;
				*(file_name++) = '.'; // Only add the dot if there's an extension
			}
			*(file_name++) = toupper(tp);
		}
		if (tp == '?')
			unique = 0;
		i++;
	}
	*file_name = 0x00;

	return (unique);
}

void fcb_hostname_to_fcb(uint16_t fcbaddr, uint8_t *file_name) {
	uint8_t i = 0;

	file_name++;
	if (*file_name == GLB_FOLDER_SEP) { // Skips the drive and / if needed
#ifdef EMULATOR_USER_SUPPORT
		file_name += 3;
#else
		file_name++;
#endif
	} else {
		file_name--;
	}

	while (*file_name != 0 && *file_name != '.') {
		ram_write(CPM_FCB_FN(fcbaddr) + i, toupper(*file_name));
		file_name++;
		i++;
	}
	while (i < 8) {
		ram_write(CPM_FCB_FN(fcbaddr) + i, ' ');
		i++;
	}
	if (*file_name == '.') {
		file_name++;
	}
	i = 0;
	while (*file_name != 0) {
		ram_write(CPM_FCB_TP(fcbaddr) + i, toupper(*file_name));
		file_name++;
		i++;
	}
	while (i < 3) {
		ram_write(CPM_FCB_TP(fcbaddr) + i, ' ');
		i++;
	}
}

void fcb_hostname_to_fcbname(uint8_t *from, uint8_t *to) {  // Converts a string name (AB.TXT) to FCB name (AB      TXT)
	int i = 0;

	from++;
	if (*from == GLB_FOLDER_SEP) {  // Skips the drive and / if needed
#ifdef EMULATOR_USER_SUPPORT
		from += 3;
#else
		from++;
#endif
	} else {
		from--;
	}

	while (*from != 0 && *from != '.') {
		*to = toupper(*from);
		to++; from++; i++;
	}
	while (i < 8) {
		*to = ' ';
		to++;  i++;
	}
	if (*from == '.') {
		from++;
	}
	i = 0;
	while (*from != 0) {
		*to = toupper(*from);
		to++; from++; i++;
	}
	while (i < 3) {
		*to = ' ';
		to++;  i++;
	}
	*to = 0;
}


static long disk_file_size(uint16_t fcbaddr) {
	long r, l = -1;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		fcb_to_hostname(fcbaddr, &glb_file_name[0]);
		l = pal_file_size(glb_file_name);
		r = l % DISK_BLK_SZ;
		if (r)
			l = l + DISK_BLK_SZ - r;
	}
	return (l);
}

uint8_t disk_open_file(uint16_t fcbaddr) {
	uint8_t result = 0xff;
	long len;
	int32_t i;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		fcb_to_hostname(fcbaddr, &glb_file_name[0]);
		if (pal_open_file(&glb_file_name[0])) {

			len = disk_file_size(fcbaddr) / 128; // Compute the len on the file in blocks

			ram_write(CPM_FCB_RC(fcbaddr), len > DISK_MAX_RC ? 0x80 : (uint8_t)len);
			for (i = 0; i < 16; i++) {
				ram_write(CPM_FCB_AL(fcbaddr) + i, 0x00);
			}

			result = 0x00;
		}
	}
	return (result);
}

uint8_t disk_close_file(uint16_t fcbaddr) {
	uint8_t result = 0xff;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		if (!IS_RW(fcbaddr)) {
			fcb_to_hostname(fcbaddr, &glb_file_name[0]);
			if (fcbaddr == GLB_BATCH_FCB_ADDR) {
				pal_truncate((char*)glb_file_name, ram_read(CPM_FCB_RC(fcbaddr)));
				// Truncate $$$.SUB to F->rc CP/M records so SUBMIT.COM can work
			}
			result = 0x00;
		} else {
			_error(DISK_ERR_WRITE_PROTECT);
		}
	}
	return (result);
}

uint8_t disk_make_file(uint16_t fcbaddr) {
	uint8_t result = 0xff;
	uint8_t i;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		if (!IS_RW(fcbaddr)) {
			fcb_to_hostname(fcbaddr, &glb_file_name[0]);
			if (pal_make_file(&glb_file_name[0])) {
				ram_write(CPM_FCB_EX(fcbaddr), 0x00);
				ram_write(CPM_FCB_S1(fcbaddr), 0x00);
				ram_write(CPM_FCB_S2(fcbaddr), 0x00);
				ram_write(CPM_FCB_RC(fcbaddr), 0x00);
				for (i = 0; i < 16; i++) {
					ram_write(CPM_FCB_AL(fcbaddr) + i, 0x00);
				}
				ram_write(CPM_FCB_CR(fcbaddr), 0x00);
				result = 0x00;
			}
		} else {
			_error(DISK_ERR_WRITE_PROTECT);
		}
	}
	return (result);
}

uint8_t disk_search_first(uint16_t fcbaddr, uint8_t isdir) {
	uint8_t result = 0xff;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		fcb_to_hostname(fcbaddr, &glb_file_name[0]);
		result = pal_find_first(isdir);
	}
	return (result);
}

uint8_t disk_search_next(uint16_t fcbaddr, uint8_t isdir) {
	uint8_t result = 0xff;

	if (!disk_select_disk(0))
		result = pal_find_next(isdir);
	return (result);
}

uint8_t disk_delete_file(uint16_t fcbaddr) {
	uint8_t result = 0xff;
	uint8_t deleted = 0xff;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		if (!IS_RW(fcbaddr)) {
			result = disk_search_first(fcbaddr, 0); // 0 = Does not create a fake dir entry when finding the file
			while (result != 0xff) {
				fcb_to_hostname(GLB_TMP_FCB_ADDR, &glb_file_name[0]);
				if (pal_delete_file(&glb_file_name[0])) {
					deleted = 0x00;
				}
				result = disk_search_first(fcbaddr, 0); // 0 = Does not create a fake dir entry when finding the file
			}
		} else {
			_error(DISK_ERR_WRITE_PROTECT);
		}
	}
	return (deleted);
}

uint8_t disk_rename_file(uint16_t fcbaddr) {
	uint8_t result = 0xff;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		if (!IS_RW(fcbaddr)) {
			ram_write(CPM_FCB_AL(fcbaddr), ram_read(CPM_FCB_DR(fcbaddr)));
			fcb_to_hostname(CPM_FCB_AL(fcbaddr), &glb_new_name[0]);
			fcb_to_hostname(fcbaddr, &glb_file_name[0]);
			if (pal_rename_file(&glb_file_name[0], &glb_new_name[0]))
				result = 0x00;
		} else {
			_error(DISK_ERR_WRITE_PROTECT);
		}
	}
	return (result);
}

uint8_t disk_read_seq(uint16_t fcbaddr) {
	uint8_t result = 0xff;

	long fpos =   ((ram_read(CPM_FCB_S2(fcbaddr)) & DISK_MAX_S2) * DISK_BLK_S2 * DISK_BLK_SZ) +
	            (ram_read(CPM_FCB_EX(fcbaddr)) * DISK_BLK_EX * DISK_BLK_SZ) +
	            (ram_read(CPM_FCB_CR(fcbaddr)) * DISK_BLK_SZ);

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		fcb_to_hostname(fcbaddr, &glb_file_name[0]);
		result = pal_read_seq(&glb_file_name[0], fpos);
		if (!result) { // Read succeeded, adjust FCB
			ram_write(CPM_FCB_CR(fcbaddr), ram_read(CPM_FCB_CR(fcbaddr)) + 1);
			if (ram_read(CPM_FCB_CR(fcbaddr)) > DISK_MAX_CR) {
				ram_write(CPM_FCB_CR(fcbaddr), 0x01);
				ram_write(CPM_FCB_EX(fcbaddr), ram_read(CPM_FCB_EX(fcbaddr)) + 1);
			}
			if (ram_read(CPM_FCB_EX(fcbaddr)) > DISK_MAX_EX) {
				ram_write(CPM_FCB_EX(fcbaddr), 0x00);
				ram_write(CPM_FCB_S2(fcbaddr), ram_read(CPM_FCB_S2(fcbaddr)) + 1);
			}
			if (ram_read(CPM_FCB_S2(fcbaddr)) > DISK_MAX_S2)
				result = 0xfe; // (todo) not sure what to do
		}
	}
	return (result);
}

uint8_t disk_write_seq(uint16_t fcbaddr) {
	uint8_t result = 0xff;

	long fpos =   ((ram_read(CPM_FCB_S2(fcbaddr)) & DISK_MAX_S2) * DISK_BLK_S2 * DISK_BLK_SZ) +
	            (ram_read(CPM_FCB_EX(fcbaddr)) * DISK_BLK_EX * DISK_BLK_SZ) +
	            (ram_read(CPM_FCB_CR(fcbaddr)) * DISK_BLK_SZ);
	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		if (!IS_RW(fcbaddr)) {
			fcb_to_hostname(fcbaddr, &glb_file_name[0]);
			result = pal_write_seq(&glb_file_name[0], fpos);
			if (!result) { // Write succeeded, adjust FCB
				ram_write(CPM_FCB_CR(fcbaddr), ram_read(CPM_FCB_CR(fcbaddr)) + 1);
				if (ram_read(CPM_FCB_CR(fcbaddr)) > DISK_MAX_CR) {
					ram_write(CPM_FCB_CR(fcbaddr), 0x01);
					ram_write(CPM_FCB_EX(fcbaddr), ram_read(CPM_FCB_EX(fcbaddr)) + 1);
				}
				if (ram_read(CPM_FCB_EX(fcbaddr)) > DISK_MAX_EX) {
					ram_write(CPM_FCB_EX(fcbaddr), 0x00);
					ram_write(CPM_FCB_S2(fcbaddr), ram_read(CPM_FCB_S2(fcbaddr)) + 1);
				}
				if (ram_read(CPM_FCB_S2(fcbaddr)) > DISK_MAX_S2)
					result = 0xfe; // (todo) not sure what to do
			}
		} else {
			_error(DISK_ERR_WRITE_PROTECT);
		}
	}
	return (result);
}

uint8_t disk_read_rand(uint16_t fcbaddr) {
	uint8_t result = 0xff;
	int32_t record = (ram_read(CPM_FCB_R2(fcbaddr)) << 16) |
	                 (ram_read(CPM_FCB_R1(fcbaddr)) << 8) | ram_read(CPM_FCB_R0(fcbaddr));
	long fpos = record * DISK_BLK_SZ;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		fcb_to_hostname(fcbaddr, &glb_file_name[0]);
		result = pal_read_rand(&glb_file_name[0], fpos);
		if (!result) { // Read succeeded, adjust FCB
			ram_write(CPM_FCB_CR(fcbaddr), record & 0x7f);
			ram_write(CPM_FCB_EX(fcbaddr), (record >> 7) & 0x1f);
			ram_write(CPM_FCB_S2(fcbaddr), (record >> 12) & 0xff);
		}
	}
	return (result);
}

uint8_t disk_write_rand(uint16_t fcbaddr) {
	uint8_t result = 0xff;
	int32_t record = (ram_read(CPM_FCB_R2(fcbaddr)) << 16) |
	                 (ram_read(CPM_FCB_R1(fcbaddr)) << 8) | ram_read(CPM_FCB_R0(fcbaddr));
	long fpos = record * DISK_BLK_SZ;

	if (!disk_select_disk(ram_read(CPM_FCB_DR(fcbaddr)))) {
		if (!IS_RW(fcbaddr)) {
			fcb_to_hostname(fcbaddr, &glb_file_name[0]);
			result = pal_write_rand(&glb_file_name[0], fpos);
			if (!result) { // Write succeeded, adjust FCB
				ram_write(CPM_FCB_CR(fcbaddr), record & 0x7f);
				ram_write(CPM_FCB_EX(fcbaddr), (record >> 7) & 0x1f);
				ram_write(CPM_FCB_S2(fcbaddr), (record >> 12) & 0xff);
			}
		} else {
			_error(DISK_ERR_WRITE_PROTECT);
		}
	}
	return (result);
}

uint8_t disk_get_file_size(uint16_t fcbaddr) {
	uint8_t result = 0xff;

	int32_t count = disk_file_size(cpu_regs.de) >> 7;

	if (count != -1) {
		ram_write(CPM_FCB_R0(fcbaddr), count & 0xff);
		ram_write(CPM_FCB_R1(fcbaddr), (count >> 8) & 0xff);
		ram_write(CPM_FCB_R2(fcbaddr), (count >> 16) & 0xff);
		result = 0x00;
	}
	return (result);
}

uint8_t disk_set_random(uint16_t fcbaddr) {
	uint8_t result = 0x00;

	int32_t count = ram_read(CPM_FCB_CR(fcbaddr)) & 0x7f;
	count += (ram_read(CPM_FCB_EX(fcbaddr)) & 0x1f) << 7;
	count += ram_read(CPM_FCB_S2(fcbaddr)) << 12;

	ram_write(CPM_FCB_R0(fcbaddr), count & 0xff);
	ram_write(CPM_FCB_R1(fcbaddr), (count >> 8) & 0xff);
	ram_write(CPM_FCB_R2(fcbaddr), (count >> 16) & 0xff);
	return (result);
}

void disk_set_user(uint8_t user) {
	glb_user_code = user & 0x1f; // BDOS unoficially allows user areas 0-31
	// this may create folders from G-V if this function is called from an user program
	// It is an unwanted behavior, but kept as BDOS does it
#ifdef EMULATOR_USER_SUPPORT
	pal_make_user_dir();        // Creates the user dir (0-F[G-V]) if needed
#endif
}

uint8_t disk_check_sub(void) {
	uint8_t result;
	uint8_t o_code = glb_user_code;                         // Saves the current user code (original BDOS does not do this)
	fcb_hostname_to_fcb(GLB_TMP_FCB_ADDR, (uint8_t*)"$???????.???"); // The original BDOS in fact only looks for a file which start with $
#ifdef EMULATOR_BATCHA
	ram_write(GLB_TMP_FCB_ADDR, 1);                         // Forces it to be checked on drive A:
#endif
#ifdef EMULATOR_BATCH0
	glb_user_code = 0;                                  // Forces it to be checked on user 0
#endif
	result = (disk_search_first(GLB_TMP_FCB_ADDR, 0) == 0x00) ? 0xff : 0x00;
	glb_user_code = o_code;                             // Restores the current user code
	return (result);
}

