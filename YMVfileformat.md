new / c++ .ymv format specifications

An fm2 variant.  Example:

```
version 1
emuVersion 9.9.10
rerecordCount 1
cdGameName SHINOBI LEGIONS                                                                                                 
cdInfo CD-1/1
cdItemNum T-2301H
cdVersion V1.000
cdDate 08/02/1995
cdRegion U
emulateBios 1     //1 if yabause's internal bios emulation is used. if a real bios image is used, 0
isPal 0           //1 if the console is emulated as PAL
sh2CoreType 1
sndCoreType 2
vidCoreType 2
cartType 0
cdRomPath H:      //this will either have a drive letter if a physical or virtual cd rom drive is used, or the cd image filename
|0|LRUDSABCXYZWE| //the actual controller data is text based mnemonics
```

SH2 Core
```
#define SH2CORE_INTERPRETER             0
#define SH2CORE_DEBUGINTERPRETER        1
```
Video Core
```
#define VIDCORE_DEFAULT         -1
#define VIDCORE_DUMMY           0
#define VIDCORE_OGL   1
#define VIDCORE_SOFT   2
```
Core Sound
```
#define SNDCORE_DEFAULT -1
#define SNDCORE_DUMMY   0
#define SNDCORE_WAV     10 // should really be 1, but I'll probably break people's stuff
```
Port Specific Sound
```
#define SNDCORE_DIRECTX 2
#define SNDCORE_SDL 1
#define SNDCORE_AL  2
```
Carts
```
#define CART_NONE               0
#define CART_PAR                1
#define CART_BACKUPRAM4MBIT     2
#define CART_BACKUPRAM8MBIT     3
#define CART_BACKUPRAM16MBIT    4
#define CART_BACKUPRAM32MBIT    5
#define CART_DRAM8MBIT          6
#define CART_DRAM32MBIT         7
#define CART_NETLINK            8
#define CART_ROM16MBIT          9
#define CART_JAPMODEM          10
```


old / pure c .ymv file format specifications - deprecated
```
0x000 YMV indicator
0x004 Emulator Version - 0.9.9
0x00a CD cdinfo - this and the following are extracted from the cd header
0x013 CD itemnum
0x01e CD version
0x025 CD date
0x030 CD gamename - internal name of cd
0x0a1 CD region - the region of the cd
0x0ac Re-records
0x0b0 emulatebios - whether yabause bios replacement is used or a real one
0x0b4 IsPal - whether the emulated system is pal or not
0x0b8 padding
```