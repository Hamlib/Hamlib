# Simple CAT Protocol Specification

## Overview

Simple CAT is a text-based serial protocol for controlling amateur radio transceivers, designed for WSJT-X digital mode operation (FT8/FT4 and others).

## Physical Layer

| Parameter   | Value                           |
|-------------|---------------------------------|
| Interface   | USB CDC ACM (Virtual COM)       |
| Baud Rate   | 115200 (default), 1200 (boot)   |
| Data Bits   | 8                               |
| Stop Bits   | 1                               |
| Parity      | None                            |
| Flow Control| None                            |
| Line Ending | CR (`\r`) for commands          |
| Response    | LF (`\n`) terminated            |

## Command Format

```
<COMMAND> [ARGUMENT] <CR>
```

- Commands are **case-insensitive**
- Arguments are space-separated
- Commands terminate with CR (`\r`, ASCII 0x0D)
- Responses terminate with LF (`\n`, ASCII 0x0A)

## Response Format

| Type    | Format                      | Description                    |
|---------|-----------------------------|--------------------------------|
| Success | `OK <COMMAND> [VALUE]\n`    | Command executed successfully  |
| Error   | `ERR <reason>\n`            | Command failed                 |

## Commands

### Frequency Control

| Command         | Response              | Description                              |
|-----------------|-----------------------|------------------------------------------|
| `FREQ`          | `FREQ <Hz>`           | Get current RX frequency in Hz           |
| `FREQ <Hz>`     | `OK FREQ <Hz>`        | Set RX frequency (e.g., `FREQ 14074000`) |

**Example:**
```
FREQ
→ FREQ 14074000
FREQ 14074500
→ OK FREQ 14074500
```

### Mode Control

| Command           | Response            | Description                           |
|-------------------|---------------------|---------------------------------------|
| `MODE`            | `MODE <mode>`       | Get current operating mode            |
| `MODE <mode>`     | `OK MODE <mode>`    | Set mode: `USB`, `LSB`, `AM`, `FM`    |

**Supported Modes:**

| Mode  | Description                    |
|-------|--------------------------------|
| USB   | Upper Side Band (default)      |
| LSB   | Lower Side Band                |
| AM    | Amplitude Modulation           |
| FM    | Frequency Modulation           |

**Example:**
```
MODE
→ MODE USB
MODE LSB
→ OK MODE LSB
```

### Digital Mode (FT8/FT4/WSPR/Others)

| Command             | Response                | Description                        |
|---------------------|-------------------------|------------------------------------|
| `DIGMODE`           | `DIGMODE <mode>`        | Get current digital mode           |
| `DIGMODE <mode>`    | `OK DIGMODE <mode>`     | Set mode: `FT8` or `FT4` or other  |

**Example:**
```
DIGMODE
→ DIGMODE FT8
DIGMODE FT4
→ OK DIGMODE FT4
```

### TX Offset

| Command          | Response              | Description                            |
|------------------|-----------------------|----------------------------------------|
| `OFFSET`         | `OFFSET <Hz>`         | Get TX offset frequency in Hz          |
| `OFFSET <Hz>`    | `OK OFFSET <Hz>`      | Set TX offset (e.g., `OFFSET 1200`)    |

The TX offset is added to the RX frequency to calculate the transmit frequency for digital modes.

**Example:**
```
OFFSET
→ OFFSET 0
OFFSET 1200
→ OK OFFSET 1200
```

### PTT/TX Control

| Command      | Response          | Description                          |
|--------------|-------------------|--------------------------------------|
| `PTT`        | `PTT ON`/`OFF`    | Get PTT (Push-To-Talk) state       |
| `PTT ON`     | `OK PTT ON`       | Enable PTT (prepare for TX)        |
| `PTT OFF`    | `OK PTT OFF`      | Disable PTT                        |
| `TX ON`      | `OK TX ON`        | Start transmission                 |
| `TX OFF`     | `OK TX OFF`       | Stop transmission                  |
| `ABORT`      | `OK ABORT`        | Abort ongoing transmission         |

**Notes:**
- `PTT ON` prepares the transmitter but does not start RF output
- `TX ON` starts RF transmission (requires `PTT ON` first)
- `TX OFF` stops RF output but keeps PTT enabled
- `PTT OFF` fully disables transmit mode
- `ABORT` immediately stops any ongoing transmission

**Example:**
```
PTT
→ PTT OFF
PTT ON
→ OK PTT ON
TX ON
→ OK TX ON
TX OFF
→ OK TX OFF
PTT OFF
→ OK PTT OFF
ABORT
→ OK ABORT
```

### ITONE (Symbol Data)

| Command           | Response            | Description                              |
|-------------------|---------------------|------------------------------------------|
| `ITONE <symbols>` | `OK ITONE <count>`  | Load symbol data for transmission        |
| `ITONEHASH`       | `ITONEHASH <md5>`   | Get MD5 hash of loaded symbol data       |

**ITONE Format:**

Symbols are space-separated digits (0-9), representing the FT8/FT4/WSPR/other tone sequence:

```
ITONE 0 0 1 3 2 1 0 3 3 1 1 2 3 0 2 1...
→ OK ITONE 105
```

**Packed Format (alternative):**

Some implementations accept packed symbols where each character represents a tone:

```
ITONE 0013210331123021...
→ OK ITONE 105
```

**ITONEHASH Example:**
```
ITONEHASH
→ ITONEHASH 64d1ac9cfe09a9f5c401d63f180da386
```

The MD5 hash can be used to verify symbol data integrity or avoid redundant retransmissions.

### Help and Version

| Command   | Response                              | Description                    |
|-----------|---------------------------------------|--------------------------------|
| `HELP`    | `HELP <command list>`                 | List all supported commands    |
| `VERSION` | `VERSION <firmware version>`          | Get firmware version string    |

**HELP Example:**
```
HELP
→ HELP FREQ MODE DIGMODE OFFSET PTT TX ABORT ITONE ITONEHASH HELP VERSION
```

**VERSION Example:**
```
VERSION
→ VERSION SimpleCAT v1.2.3
```

## Complete Example Session

```
Host                                    Device
----                                    ------
FREQ<CR>
                                        → FREQ 14074000<LF>
FREQ 14074500<CR>
                                        → OK FREQ 14074500<LF>
MODE USB<CR>
                                        → OK MODE USB<LF>
DIGMODE FT4<CR>
                                        → OK DIGMODE FT4<LF>
OFFSET 1200<CR>
                                        → OK OFFSET 1200<LF>
ITONE 0 0 1 3 2 1 0 3 3 1 1 2...<CR>
                                        → OK ITONE 105<LF>
ITONEHASH<CR>
                                        → ITONEHASH 64d1ac9cfe09a9f5c401d63f180da386<LF>
PTT ON<CR>
                                        → OK PTT ON<LF>
TX ON<CR>
                                        → OK TX ON<LF>
TX OFF<CR>
                                        → OK TX OFF<LF>
PTT OFF<CR>
                                        → OK PTT OFF<LF>
```

## Error Codes

| Error Code                  | Description                          |
|-----------------------------|--------------------------------------|
| `ERR invalid frequency`     | Frequency out of valid range         |
| `ERR invalid mode`          | Unknown or unsupported mode string   |
| `ERR invalid itone value`   | Symbol character not in range 0-9    |
| `ERR too many itones`       | More than 255 symbols provided       |
| `ERR command too long`      | Command line exceeds buffer size     |
| `ERR unsupported command`   | Unknown command verb                 |
| `ERR tx not prepared`       | TX attempted without PTT ON first    |
| `ERR invalid offset`        | Offset value out of valid range      |
