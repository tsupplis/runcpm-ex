#ifndef _DEFAULTS_H
#define _DEFAULTS_H


#define RAM_SIZE 60

#ifdef ARDUINO
#define RAM_SPI
#define EMULATOR_LED 13
#define EMULATOR_HOSTOS      0x01
#else
#define EMULATOR_HOSTOS      0x02
#endif

#ifdef RAM_SPI
#define RAM_SPI_PERIPHERAL    SPI
#define RAM_SPI_CS   10
#define RAM_SPI_CHIP SRAM_23LC1024
#endif

#ifdef ARDUINO
#define SD_SPI_CS 4
#endif

/* Definitions for file/console based debugging */
//#define DEBUG
//#define DEBUG_LOG	// Writes extensive call trace information to RunCPM.log
//#define DEBUG_LOG_TO_CONSOLE	// Writes debug information to console instead of file
//#define DEBUG_LOG_ONLY 22	// If defined will log only this BDOS (or BIOS) function
#define DEBUG_LOG_PATH "runcpm.log"

/* RunCPM version for the greeting header */
#define EMULATOR_VERSION	 "2.9"
#define EMULATOR_VERSION_BCD 0x29

/* Definition of which CCP to use (must define only one) */
//#define CCP_INTERNAL	// If this is defined, an internal CCP will emulated
//#define CCP_DR
//#define CCP_CCPZ
//#define CCP_ZCPR2
#define CCP_ZCPR3
//#define CCP_Z80

/* Definition of the CCP memory information */
//

/* Definition for CP/M 2.2 user number support */

#define USER_SUPPORT	// If this is defined, CP/M user support is added. RunCPM will ignore the contents of the /A, /B folders and instead
						// look for /A/0 /A/1 and so on, as well for the other drive letters.
						// User numbers are 0-9, then A-F for users 10-15. On case sensitive file-systems the usercodes A-F folders must be uppercase.
						// This preliminary feature should emulate the CP/M user.

#define BATCHA			// If this is defined, the $$$.SUB will be looked for on drive A:
//#define BATCH0		// If this is defined, the $$$.SUB will be looked for on user area 0
						// The default behavior of DRI's CP/M 2.2 was to have $$$.SUB created on the current drive/user while looking for it
						// on drive A: current user, which made it complicated to run SUBMITs when not logged to drive A: user 0



#endif