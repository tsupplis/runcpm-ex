#ifndef CCP_H
#define CCP_H

// CP/M BDOS calls
#define C_WRITE			2
#define C_READSTR		10
#define DRV_ALLRESET	13
#define DRV_SET			14
#define F_OPEN			15
#define F_CLOSE			16
#define F_DELETE		19
#define F_READ			20
#define F_WRITE			21
#define F_MAKE			22
#define F_RENAME		23
#define DRV_GET			25
#define F_DMAOFF		26
#define F_USERNUM		32

#define CmdFCB	BatchFCB + 36		// FCB for use by internal commands
#define ParFCB	0x005C				// FCB for use by line parameters
#define SecFCB	0x006C				// Secondary part of FCB for renaming files

#define inBuf	BDOSjmppage - 256	// Input buffer location
#define cmdLen	125					// Maximum size of a command line (sz+rd+cmd+\0)

#define defDMA	0x0080				// Default DMA address
#define defLoad	0x0100				// Default load address

// CCP global variables
uint8 curDrive;	// 0 -> 15 = A -> P	.. Current drive for the CCP (same as RAM[0x0004]
uint8 parDrive;	// 0 -> 15 = A -> P .. Drive for the first file parameter
uint8 curUser;	// 0 -> 15			.. Current user aread to access
uint8 prompt[5] = "\r\n >";
uint16 pbuf, perr;
uint8 blen;							// Actual size of the typed command line (size of the buffer)

static const char *Commands[] =
{
	// Standard CP/M commands
	"DIR",
	"ERA",
	"TYPE",
	"SAVE",
	"REN",
	"USER",
	// Extra CCP commands
	"DEL",
	"INFO",
	"EXIT",
	NULL
};

// Used to call BDOS from inside the CCP
uint8 _ccp_bdos(uint8 function, uint16 de, uint8 a) {
	SET_LOW_REGISTER(BC, function);
	DE = de;
	SET_HIGH_REGISTER(AF, a);
	_Bdos();
	return(HIGH_REGISTER(AF));
}

// Compares two strings (Atmel doesn't like strcmp)
uint8 _ccp_strcmp(char *stra, char *strb) {
	while (*stra && *strb && (*stra == *strb)) {
		stra++; strb++;
	}
	return(*stra == *strb);
}

// Gets the command ID number
uint8 _ccp_cnum(void) {
	uint8 result = 255;
	uint8 command[9];
	uint8 i = 0;

	if (!_RamRead(CmdFCB)) {	// If a drive was set, then the command is external
		while (i < 8 && _RamRead(CmdFCB + i + 1) != ' ') {
			command[i] = _RamRead(CmdFCB + i + 1);
			i++;
		}
		command[i] = 0;

		i = 0;
		while (Commands[i]) {
			if (_ccp_strcmp((char*)command, (char*)Commands[i])) {
				result = i; break;
			}
			i++;
		}
	}
	return(result);
}

// Returns true if character is a separator
uint8 _ccp_delim(uint8 ch) {
	return(ch == 0 || ch == ' ' || ch == '=' || ch == '.' || ch == ':' || ch == ';' || ch == '<' || ch == '>');
}

// Prints the FCB filename
void _ccp_printfcb(uint16 fcb, uint8 compact) {
	uint8 i, ch;

	ch = _RamRead(fcb);
	if (ch && compact) {
		_ccp_bdos(C_WRITE, ch + '@', 0x00);
		_ccp_bdos(C_WRITE, ':', 0x00);
	}

	for (i = 1; i < 12; i++) {
		ch = _RamRead(fcb + i);
		if (ch == ' ' && compact)
			continue;
		if (i == 9)
			_ccp_bdos(C_WRITE, compact ? '.' : ' ', 0x00);
		_ccp_bdos(C_WRITE, ch, 0x00);
	}
}

// Initializes the FCB
void _ccp_initFCB(uint16 address) {
	uint8 i;

	for (i = 0; i < 36; i++)
		_RamWrite(address + i, 0x00);
	for (i = 0; i < 11; i++) {
		_RamWrite(address + 1 + i, 0x20);
		_RamWrite(address + 17 + i, 0x20);
	}
}

// Name to FCB
uint8 _ccp_nameToFCB(uint16 fcb) {
	uint8 pad, plen, ch, n = 0;

	// Checks for a drive and places it on the Command FCB
	if (_RamRead(pbuf + 1) == ':') {
		ch = toupper(_RamRead(pbuf++));
		_RamWrite(fcb, ch - '@');		// Makes the drive 0x1-0xF for A-P
		pbuf++;							// Points pbuf past the :
		blen -= 2;
	}

	if (blen) {
		fcb++;

		plen = 8;
		pad = ' ';
		while (blen && plen) {
			ch = toupper(_RamRead(pbuf));
			if (_ccp_delim(ch)) {
				break;
			}
			pbuf++; blen--;
			if (ch == '*')
				pad = '?';
			if (pad == '?' || ch == '?') {
				ch = pad;
				n = n | 0x80;	// Name is not unique
			}
			plen--; n++;
			_RamWrite(fcb++, ch);
		}

		while (plen--)
			_RamWrite(fcb++, pad);

		plen = 3;
		pad = ' ';
		if (ch == '.') {
			pbuf++; blen--;
		}
		while (blen && plen) {
			ch = toupper(_RamRead(pbuf));
			if (_ccp_delim(ch)) {
				break;
			}
			pbuf++; blen--;
			if (ch == '*')
				pad = '?';
			if (pad == '?' || ch == '?') {
				ch = pad;
				n = n | 0x80;	// Name is not unique
			}
			plen--; n++;
			_RamWrite(fcb++, ch);
		}

		while (plen--)
			_RamWrite(fcb++, pad);
	}

	return(n);
}

// Converts the ParFCB name to a number
uint16 _ccp_fcbtonum() {
	uint8 ch;
	uint16 n = 0;
	uint8 pos = ParFCB + 1;
	while (TRUE) {
		ch = _RamRead(pos++);
		if (ch<'0' || ch>'9')
			break;
		n = (n * 10) + (ch - 0x30);
	}
	return(n);
}

// DIR command
void _ccp_dir(void) {
	uint8 i;
	uint8 dirHead[6] = "A: ";
	uint8 dirSep[4] = " : ";
	uint32 fcount = 0;	// Number of files printed
	uint32 ccount = 0;	// Number of columns printed

	if (_RamRead(ParFCB + 1) == ' ') {
		for (i = 1; i < 12; i++)
			_RamWrite(ParFCB + i, '?');
	}

	dirHead[0] = _RamRead(ParFCB) ? _RamRead(ParFCB) + '@' : 'A';

	if (!_SearchFirst(ParFCB, TRUE)) {
		_puts((char*)dirHead);
		_ccp_printfcb(tmpFCB, FALSE);
		fcount++; ccount++;
		while (!_SearchNext(ParFCB, TRUE)) {
			if (!ccount) {
				_puts("\r\n");
				_puts((char*)dirHead);
			} else {
				_puts((char*)dirSep);
			}
			_ccp_printfcb(tmpFCB, FALSE);
			fcount++; ccount++;
			if (ccount > 3)
				ccount = 0;
		}
	} else {
		_puts("No file");
	}
}

// ERA command
void _ccp_era(void) {
	if (_ccp_bdos(F_DELETE, ParFCB, 0x00))
		_puts("No file");
}

// TYPE command
void _ccp_type(void) {
	uint8 i, c;
	uint16 a;

	if (!_ccp_bdos(F_OPEN, ParFCB, 0x00)) {
		while (!_ccp_bdos(F_READ, ParFCB, 0x00)) {
			i = 128;
			a = dmaAddr;
			while (i) {
				c = _RamRead(a);
				if (c == 0x1A)
					break;
				_ccp_bdos(C_WRITE, c, 0x00);
				i--; a++;
			}
		}
	} else {
		_ccp_printfcb(ParFCB, TRUE);
		_puts("?\r\n");
	}
}

// SAVE command
void _ccp_save(void) {
	uint16 pages = _ccp_fcbtonum();
	uint16 i, dma;

	if (pages > 255) {
		_ccp_printfcb(ParFCB, TRUE);
		_puts("?\r\n");
	} else {
		while (_RamRead(pbuf) == ' ' && blen) {		// Skips any leading spaces
			pbuf++; blen--;
		}
		_ccp_nameToFCB(ParFCB);						// Loads file name onto the ParFCB
		if (_ccp_bdos(F_MAKE, ParFCB, 0x00)) {
			_puts("Create error");
		} else {
			if (_ccp_bdos(F_OPEN, ParFCB, 0x00)) {
				_puts("Open Error");
			} else {
				pages *= 2;									// Calculates the number of CP/M blocks to write
				dma = defLoad;
				for (i = 0; i < pages; i++) {
					_ccp_bdos(F_DMAOFF, dma, 0x00);
					_ccp_bdos(F_WRITE, ParFCB, 0x00);
					dma += 128;
					_puts(".");
				}
				_ccp_bdos(F_CLOSE, ParFCB, 0x00);
			}
		}
	}
}

// REN command
void _ccp_ren(void) {
	uint8 ch, i;
	pbuf++;
	_HostnameToFCBname(_RamSysAddr(pbuf), _RamSysAddr(SecFCB + 1));
	for (i = 0; i < 12; i++) {	// Swap the filenames on the fcb
		ch = _RamRead(ParFCB + i);
		_RamWrite(ParFCB + i, _RamRead(SecFCB + i));
		_RamWrite(SecFCB + i, ch);
	}
	if (_ccp_bdos(F_RENAME, ParFCB, 0x00)) {
		_ccp_printfcb(ParFCB, TRUE);
		_puts("?\r\n");
	}
}

// USER command
void _ccp_user(void) {
	curUser = (uint8)_ccp_fcbtonum();
	if (curUser < 16) {
		_ccp_bdos(F_USERNUM, curUser, 0x00);
	} else {
		_puts("User: 0->16 only");
	}
}

// INFO command
void _ccp_info(void) {
	_puts("RunCPM version " VERSION "\r\n");
	_puts("BDOS Page set to "); _puthex16(BDOSjmppage); _puts("\r\n");
	_puts("BIOS Page set to "); _puthex16(BIOSjmppage);
}

// External (.COM) command
uint8 _ccp_external(void) {
	uint8 result = FALSE;
	uint16 loadAddr = defLoad;

	_RamWrite(CmdFCB + 9, 'C');
	_RamWrite(CmdFCB + 10, 'O');
	_RamWrite(CmdFCB + 11, 'M');

	if (!_ccp_bdos(F_OPEN, CmdFCB, 0x00)) {
		_ccp_bdos(F_DMAOFF, loadAddr, 0x00);
		while (!_ccp_bdos(F_READ, CmdFCB, 0x00)) {
			loadAddr += 128;
			_ccp_bdos(F_DMAOFF, loadAddr, 0x00);
		}
		_ccp_bdos(F_DMAOFF, defDMA, 0x00);

		Z80reset();			// Resets the Z80 CPU
		SET_LOW_REGISTER(BC, _RamRead(0x0004));	// Sets C to the current drive/user
		PC = 0x0100;		// Sets CP/M application jump point
		Z80run();			// Starts simulation

		result = TRUE;
	}

	return(result);
}

// Prints a command error
void _ccp_cmdError() {
	uint8 ch;

	while (ch = _RamRead(perr++)) {
		if (ch == ' ')
			break;
		_ccp_bdos(C_WRITE, toupper(ch), 0x00);
	}
	_puts("?\r\n");
}

// Main CCP code
void _ccp(void) {
	uint8 i;

	_puts(CCPHEAD);

	_ccp_bdos(F_USERNUM, 0x0000, 0x00);					// Set current user
	_ccp_bdos(DRV_ALLRESET, 0x0000, 0x00);
	_ccp_bdos(DRV_SET, curDrive, 0x00);

	while (TRUE) {
		curDrive = _ccp_bdos(DRV_GET, 0x0000, 0x00);	// Get current drive
		curUser = _ccp_bdos(F_USERNUM, 0x00FF, 0x00);	// Get current user
		_RamWrite(0x0004, (curUser << 4) + curDrive);	// Set user/drive on addr 0x0004

		parDrive = curDrive;							// Initially the parameter drive is the same as the current drive

		prompt[2] = 'A' + curDrive;						// Shows the prompt
		_puts((char*)prompt);

		_RamWrite(inBuf, cmdLen);						// Sets the buffer size to read the command line
		_ccp_bdos(C_READSTR, inBuf, 0x00);				// and reads the command line

		blen = _RamRead(inBuf + 1);						// Obtains the number of bytes read

		_ccp_bdos(F_DMAOFF, defDMA, 0x00);				// Reset current DMA

		if (blen) {
			_RamWrite(inBuf + 2 + blen, 0);				// "Closes" the read buffer with a \0
			pbuf = inBuf + 2;							// Points pbuf to the first command character

			while (_RamRead(pbuf) == ' ' && blen) {		// Skips any leading spaces
				pbuf++; blen--;
			}
			if (!blen)									// There were only spaces
				continue;
			if (_RamRead(pbuf) == ';')					// Found a comment line
				continue;

			_ccp_initFCB(CmdFCB);						// Initializes the command FCB

			perr = pbuf;								// Saves the pointer in case there's an error
			if (_ccp_nameToFCB(CmdFCB) > 8) {			// Extracts the command from the buffer
				_ccp_cmdError();						// Command name cannot be non-unique or have an extension
				continue;
			}

			if (_RamRead(CmdFCB) && _RamRead(CmdFCB + 1) == ' ') {	// Command as a simple drive select
				_ccp_bdos(DRV_SET, _RamRead(CmdFCB) - 1, 0x00);
				continue;
			}

			RAM[defDMA] = blen;							// Move the command line at this point to 0x0080
			for (i = 0; i < blen; i++) {
				RAM[defDMA + i + 1] = _RamRead(pbuf + i);
			}
			RAM[defDMA + i + 1] = 0;

			while (_RamRead(pbuf) == ' ' && blen) {		// Skips any leading spaces
				pbuf++; blen--;
			}

			_ccp_initFCB(ParFCB);						// Initializes the parameter FCB
			_ccp_nameToFCB(ParFCB);						// Loads the next file parameter onto the parameter FCB

			_puts("\r\n");
			// Checks if the command is valid and executes
			switch (_ccp_cnum()) {
			case 0:		// DIR
				_ccp_dir();		break;
			case 1:		// ERA
				_ccp_era();		break;
			case 2:		// TYPE
				_ccp_type();	break;
			case 3:		// SAVE
				_ccp_save();	break;
			case 4:		// REN
				_ccp_ren();		break;
			case 5:		// USER
				_ccp_user();	break;
				// Extra commands
			case 6:		// DEL is an alias to ERA
				_ccp_era();		break;
			case 7:		// INFO
				_ccp_info();	break;
			case 8:		// EXIT
				Status = 1;		break;
			case 255:	// It is an external command
				if (_ccp_external())	// Will fall down to default if it doesn't exist
					break;
			default:
				_ccp_cmdError();
				break;
			}
		}
		if (Status == 1)	// This is set by a call to BIOS 0 - ends CP/M
			break;
	}
}

#endif
