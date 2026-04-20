# [PRELIMINARY, PROPOSED FUTURE WORK] CBM-Specific Extensions (stdcbm) for MEGA65/C64

This document defines the CBM-specific library extensions required to interface with the KERNAL and hardware of Commodore-based systems (specifically C64 and MEGA65). These functions bridge the gap between standard C and the unique "Logical File" architecture of the KERNAL.

## 1. Low-Level Memory Access (`peekpoke.h`)

Crucial for manipulating hardware registers (VIC-II, SID, CIA, etc.).

- **`PEEK(addr)` / `POKE(addr, val)`**: Read/Write a single byte.
- **`PEEKW(addr)` / `POKEW(addr, val)`**: Read/Write a 16-bit word (little-endian).
- **`PEEKL(addr)` / `POKEL(addr, val)`**: Read/Write a 32-bit dword (leveraging 45GS02 capabilities).

## 2. KERNAL File I/O (`cbm.h`)

The C64 KERNAL does not use file descriptors in the Unix sense. It uses "Logical File Numbers" (LFN), Device Numbers, and Secondary Addresses.

### Wrapper Functions
- **`cbm_open(lfn, device, sec_addr, name)`**: Sets name and LFS, then calls KERNAL `OPEN`.
- **`cbm_close(lfn)`**: Calls KERNAL `CLOSE`.
- **`cbm_read(lfn, buffer, size)`**: Uses `CHKIN` and `CHRIN` to fill a buffer.
- **`cbm_write(lfn, buffer, size)`**: Uses `CHKOUT` and `CHROUT` to output a buffer.
- **`cbm_load(name, device, address)`**: Calls KERNAL `LOAD`.
- **`cbm_save(name, device, start, end)`**: Calls KERNAL `SAVE`.

### Raw KERNAL Access
- **`cbm_k_setlfs(lfn, device, sec_addr)`**
- **`cbm_k_setnam(name)`**
- **`cbm_k_open()` / `cbm_k_close(lfn)`**
- **`cbm_k_chkin(lfn)` / `cbm_k_chkout(lfn)`**
- **`cbm_k_clrchn()`**: Restore default I/O (Keyboard/Screen).

## 3. Screen & Console Control

Beyond standard `putchar`, these handle the CBM's unique screen editor.

- **`clrscr()`**: Clear the screen (usually `CHROUT(147)`).
- **`revers(on)`**: Enable/disable reverse video.
- **`textcolor(color)`**: Set text color (POKEing 646 or VIC registers).
- **`bgcolor(color)`**: Set background color.
- **`gotoxy(x, y)`**: Move cursor via KERNAL `PLOT`.
- **`wherex()` / `wherey()`**: Get current cursor position.

## 4. System & Time

- **`getin()`**: Get a character from the keyboard buffer (non-blocking, calls KERNAL `GETIN`).
- **`rdtim()`**: Read the Jiffy Clock (3 bytes).
- **`settim(time)`**: Set the Jiffy Clock.
- **`waitvsync()`**: Wait for the raster beam to reach the bottom (essential for smooth graphics).

## 5. Directory & Disk Command

- **`cbm_dir(device)`**: Minimal implementation to read and print the disk directory.
- **`cbm_command(device, cmd)`**: Send a command string to the disk drive (Secondary Address 15).

## 6. Proposed File Structure

```text
lib/
├── include/
│   ├── cbm.h          # KERNAL wrappers and LFN constants
│   ├── peekpoke.h     # Memory access macros
│   └── mega65.h       # MEGA65-specific register definitions
├── src/
│   ├── cbm_io.c       # cbm_open/read/write/etc.
│   ├── cbm_kernal.s   # Raw assembly wrappers for KERNAL ROM calls
│   └── screen.c       # gotoxy/clrscr/etc.
```

## 7. Implementation Notes

- **The Status Byte (`ST`)**: Every KERNAL operation updates the `ST` variable ($90). The library must provide a way to check this (e.g., `cbm_status()`).
- **PETSCII vs ASCII**: The library must decide whether to handle PETSCII/ASCII conversion automatically in `puts` and `cbm_write`, or provide explicit conversion functions.
