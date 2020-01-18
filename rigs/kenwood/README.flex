FlexRadio 6xxx series notes and Hamlib errata by Steve Conklin, AI4QR.

Commands as documented from Flex

----------Kenwood Command Set----------
FA Slice receiver A frequency.
FB Slice receiver B frequency.
FR Receive VFO
FT Transmit VFO
IF Transceiver Status
KS Keying speed
MD DSP Mode
RX Set receive (TX off)
SH Set DSP filter high frequency
SL Set DSP filter low frequency
TX Set transmit (TX on)

----------FlexRadio Command Set----------
ZZFA Slice receiver A frequency
ZZFB Slice receiver B frequency
ZZFI Slice Receiver A DSP filter
ZZFJ Slice Receiver B DSP filter
ZZIF Transceiver Status
ZZMD Slice receiver A DSP Mode
ZZME Slice receiver B DSP Mode
ZZSW Set slice Transmit flag (RX A or RX B)
ZZTX Set MOX (on/off)

================================================
rigctl commands used to test operation
(freq set and read)
f
F 14100000
F 14070000
also test to see if RX and TX can be set out of band

(mode set and read with passband)
m
M USB 5 [Should set to USB and 1.6K]
M USB 20000 [should set to USb and 4K]
also test other modes

(vfo set and read)
v
V VFOB
[opens with a 5K split]

(set and get split freq)
i
I 14078000
[ Even if split is turned off, this sets VFOB]

(split set and read)
NOTE: When split off, TX VFO is always A
s
[ shows split and VFOA is TX]
S 1 VFOA
s
S 1 VFOB

(keying speed)
l KEYSPD
L KEYSPD 25
l KEYSPD
L KEYSPD 10
l KEYSPD

(filter edges)
l SLOPE_HIGH
l SLOPE_LOW

===
Supported:
case RIG_LEVEL_KEYSPD: 'KS'
case RIG_LEVEL_SLOPE_HIGH: 'SH'
needs list of frequencies
case RIG_LEVEL_SLOPE_LOW: 'SL'

Not supported:
case RIG_LEVEL_RFPOWER: 'PC'
case RIG_LEVEL_AF: 'AG'
case RIG_LEVEL_RF: 'RG'
case RIG_LEVEL_SQL: 'SQ'
case RIG_LEVEL_AGC: 'GT'
case RIG_LEVEL_ATT: 'RA'
case RIG_LEVEL_PREAMP: 'PA'
case RIG_LEVEL_CWPITCH: 'PT'



