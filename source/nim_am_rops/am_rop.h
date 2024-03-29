#pragma once

#include "rop_common.h"

#define AM_ROP_ADDSPR4_POPR4R5R6R7PC          ROP_THUMBCODE(0x00103A84, 0)
#define AM_ROP_POPR2R3R4PC                    ROP_THUMBCODE(0x001006E8, 0)
#define AM_ROP_POPR1R2R3PC                    ROP_THUMBCODE(0x00102358, 0)
#define AM_ROP_MOVLRR3_BXR1                   ROP_ARMCODE  (0x00111C6C, 2)
#define AM_ROP_LDRR2_R1_LDRR3_R0_SPOILR0_BXLR ROP_THUMBCODE(0x00109A2E, 0)
#define AM_ROP_MOVR0R4_POPR4PC                ROP_THUMBCODE(0x0010A066, 0)
#define AM_ROP_MOVR1SP_BLXR6                  ROP_THUMBCODE(0x001070D0, 0)
#define AM_ROP_POPR1PC                        ROP_ARMCODE  (0x00106FD4, 0)
#define AM_ROP_MOVR0R1_POPR1R2R3R4R5R6R7PC    ROP_THUMBCODE(0x001105D6, 0)
#define AM_ROP_LDRR1_R1R2_POPR4R5R6PC         ROP_THUMBCODE(0x00107A36, 0)
#define AM_ROP_STRR3_R0R1_POPR4R5PC           ROP_THUMBCODE(0x00107390, 0)

#define AM_ROP_GETTLSADDR_ADD0X5C_POPR4PC     ROP_THUMBCODE(0x0010249C, 1)

#define AM_ROP_FILESTREAMDESCTRUCT_POPR4PC    ROP_THUMBCODE(0x0010A05C, 1)

#define AM_ROP_PXIAM9_HANDLE_ADDR             (0x00113280)

#ifdef AM_U_TARGET
#define AM_ROP_STACK_FIXUP 0x44
#else
#define AM_ROP_STACK_FIXUP 0x4C
#endif
