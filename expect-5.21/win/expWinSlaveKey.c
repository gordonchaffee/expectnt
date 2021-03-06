/*
 * expWinSlaveKey.c --
 *
 *	This has tables to do conversions from ASCII characters to 
 *	console keyboard input records.  Using them, a slave process
 *	can be driven as if someone was typing at the keyboard.
 *
 * Copyright (c) 1997 by Mitel Corporation
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "tcl.h"
#include "tclPort.h"
#include "expWin.h"
#include "expWinSlave.h"

EXP_KEY ExpModifierKeyArray[] = {
/* Control */ { 17,  29, 0},
/* LShift */  { 16,  42, 0},
/* RShift */  { 16,  54, 0},
/* Alt */     { 18,  56, 0},
};


EXP_KEY ExpAsciiToKeyArray[256] = {
/*   0 */ { 50,   3, RIGHT_CTRL_PRESSED|SHIFT_PRESSED},
/*   1 */ { 65,  30, RIGHT_CTRL_PRESSED},
/*   2 */ { 66,  48, RIGHT_CTRL_PRESSED},
/*   3 */ { 67,  46, RIGHT_CTRL_PRESSED},
/*   4 */ { 68,  32, RIGHT_CTRL_PRESSED},
/*   5 */ { 69,  18, RIGHT_CTRL_PRESSED},
/*   6 */ { 70,  33, RIGHT_CTRL_PRESSED},
/*   7 */ { 71,  34, RIGHT_CTRL_PRESSED},
/*   8 */ { 72,  35, RIGHT_CTRL_PRESSED},
/*   9 */ {  9,  15, RIGHT_CTRL_PRESSED},
/*  10 */ { 74,  36, RIGHT_CTRL_PRESSED},
/*  11 */ { 75,  37, RIGHT_CTRL_PRESSED},
/*  12 */ { 76,  38, RIGHT_CTRL_PRESSED},
/*  13 */ { 13,  28, 0},
/*  14 */ { 78,  49, RIGHT_CTRL_PRESSED},
/*  15 */ { 79,  24, RIGHT_CTRL_PRESSED},
/*  16 */ { 80,  25, RIGHT_CTRL_PRESSED},
/*  17 */ { 81,  16, RIGHT_CTRL_PRESSED},
/*  18 */ { 82,  19, RIGHT_CTRL_PRESSED},
/*  19 */ { 83,  31, RIGHT_CTRL_PRESSED},
/*  20 */ { 84,  20, RIGHT_CTRL_PRESSED},
/*  21 */ { 85,  22, RIGHT_CTRL_PRESSED},
/*  22 */ { 86,  47, RIGHT_CTRL_PRESSED},
/*  23 */ { 87,  17, RIGHT_CTRL_PRESSED},
/*  24 */ { 88,  45, RIGHT_CTRL_PRESSED},
/*  25 */ { 89,  21, RIGHT_CTRL_PRESSED},
/*  26 */ { 90,  44, RIGHT_CTRL_PRESSED},
/*  27 */ {219, 219, RIGHT_CTRL_PRESSED|SHIFT_PRESSED},
/*  28 */ {220, 220, RIGHT_CTRL_PRESSED|SHIFT_PRESSED},
/*  29 */ {221, 221, RIGHT_CTRL_PRESSED|SHIFT_PRESSED},
/*  30 */ { 54,  54, RIGHT_CTRL_PRESSED|SHIFT_PRESSED},
/*  31 */ {189, 189, RIGHT_CTRL_PRESSED|SHIFT_PRESSED},
/*  32 */ { 32,  32, 0},
/*  33 */ { 49,  49, SHIFT_PRESSED},
/*  34 */ {222, 222, SHIFT_PRESSED},
/*  35 */ { 51,  51, SHIFT_PRESSED},
/*  36 */ { 52,  52, SHIFT_PRESSED},
/*  37 */ { 53,  53, SHIFT_PRESSED},
/*  38 */ { 55,  55, SHIFT_PRESSED},
/*  39 */ {222, 222, 0},
/*  40 */ { 57,  57, SHIFT_PRESSED},
/*  41 */ { 48,  48, SHIFT_PRESSED},
/*  42 */ { 56,  56, SHIFT_PRESSED},
/*  43 */ {187, 187, SHIFT_PRESSED},
/*  44 */ {188, 188, 0},
/*  45 */ {189, 189, SHIFT_PRESSED},
/*  46 */ {190, 190, 0},
/*  47 */ {191, 191, 0},
/*  48 */ { 48,  48, 0},
/*  49 */ { 49,  49, 0},
/*  50 */ { 50,   3, 0},
/*  51 */ { 51,  51, 0},
/*  52 */ { 52,  52, 0},
/*  53 */ { 53,  53, 0},
/*  54 */ { 54,  54, 0},
/*  55 */ { 55,  55, 0},
/*  56 */ { 56,  56, 0},
/*  57 */ { 57,  57, 0},
/*  58 */ {186, 186, SHIFT_PRESSED},
/*  59 */ {186, 186, 0},
/*  60 */ {188, 188, SHIFT_PRESSED},
/*  61 */ {187, 187, SHIFT_PRESSED},
/*  62 */ {190, 190, SHIFT_PRESSED},
/*  63 */ {191, 191, SHIFT_PRESSED},
/*  64 */ { 50,   3, 0},
/*  65 */ { 65,  30, SHIFT_PRESSED},
/*  66 */ { 66,  48, SHIFT_PRESSED},
/*  67 */ { 67,  46, SHIFT_PRESSED},
/*  68 */ { 68,  32, SHIFT_PRESSED},
/*  69 */ { 69,  18, SHIFT_PRESSED},
/*  70 */ { 70,  33, SHIFT_PRESSED},
/*  71 */ { 71,  34, SHIFT_PRESSED},
/*  72 */ { 72,  35, SHIFT_PRESSED},
/*  73 */ { 73,  23, SHIFT_PRESSED},
/*  74 */ { 74,  36, SHIFT_PRESSED},
/*  75 */ { 75,  37, SHIFT_PRESSED},
/*  76 */ { 76,  38, SHIFT_PRESSED},
/*  77 */ { 77,  50, SHIFT_PRESSED},
/*  78 */ { 78,  49, SHIFT_PRESSED},
/*  79 */ { 79,  24, SHIFT_PRESSED},
/*  80 */ { 80,  25, SHIFT_PRESSED},
/*  81 */ { 81,  16, SHIFT_PRESSED},
/*  82 */ { 82,  19, SHIFT_PRESSED},
/*  83 */ { 83,  31, SHIFT_PRESSED},
/*  84 */ { 84,  20, SHIFT_PRESSED},
/*  85 */ { 85,  22, SHIFT_PRESSED},
/*  86 */ { 86,  47, SHIFT_PRESSED},
/*  87 */ { 87,  17, SHIFT_PRESSED},
/*  88 */ { 88,  45, SHIFT_PRESSED},
/*  89 */ { 89,  21, SHIFT_PRESSED},
/*  90 */ { 90,  44, SHIFT_PRESSED},
/*  91 */ {219, 219, 0},
/*  92 */ {220, 220, 0},
/*  93 */ {221, 221, 0},
/*  94 */ { 54,  54, SHIFT_PRESSED},
/*  95 */ {189, 189, SHIFT_PRESSED},
/*  96 */ {192, 192, 0},
/*  97 */ { 65,  30, 0},
/*  98 */ { 66,  48, 0},
/*  99 */ { 67,  46, 0},
/* 100 */ { 68,  32, 0},
/* 101 */ { 69,  18, 0},
/* 102 */ { 70,  33, 0},
/* 103 */ { 71,  34, 0},
/* 104 */ { 72,  35, 0},
/* 105 */ { 73,  23, 0},
/* 106 */ { 74,  36, 0},
/* 107 */ { 75,  37, 0},
/* 108 */ { 76,  38, 0},
/* 109 */ { 77,  50, 0},
/* 110 */ { 78,  49, 0},
/* 111 */ { 79,  24, 0},
/* 112 */ { 80,  25, 0},
/* 113 */ { 81,  16, 0},
/* 114 */ { 82,  19, 0},
/* 115 */ { 83,  31, 0},
/* 116 */ { 84,  20, 0},
/* 117 */ { 85,  22, 0},
/* 118 */ { 86,  47, 0},
/* 119 */ { 87,  17, 0},
/* 120 */ { 88,  45, 0},
/* 121 */ { 89,  21, 0},
/* 122 */ { 90,  44, 0},
/* 123 */ {219, 219, SHIFT_PRESSED},
/* 124 */ {220, 220, SHIFT_PRESSED},
/* 125 */ {221, 221, SHIFT_PRESSED},
/* 126 */ {192, 192, SHIFT_PRESSED},
/* 127 */ {  8,  14, RIGHT_CTRL_PRESSED},
};

/*
 * Also of interest
 *
 * LShift:  { 16,  42, 0}
 * RShift:  { 16,  54, 0}
 * Control: { 17,  29, 0}
 * Alt:     { 18,  56, 0}
 * LWin95:  { 91,  91, 0}
 * RWin95:  { 92,  92, 0}
 * WinPopup:{ 93,  93, 0}
 * Tab:     {  9,  15, 0}
 * CapsLock { 20,  58, 0}
 * ESC:     { 27,   1, 0}
 * F1:      {112,  59, 0}
 * F2:      {113,  60, 0}
 * F3:      {114,  61, 0}
 * F4:      {115,  62, 0}
 * F5:      {116,  63, 0}
 * F6:      {117,  64, 0}
 * F7:      {118,  65, 0}
 * F8:      {119,  66, 0}
 * F9:      {120,  67, 0}
 * F10:     {121,  68, 0}
 * F11:     {122,  87, 0}
 * F12:     {123,  88, 0}
 */

