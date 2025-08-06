/*
*********************************************************************************************************
*											        ePDK
*						            the Easy Portable/Player Develop Kits
*									           hello world sample
*
*						        (c) Copyright 2006-2007, Steven.ZGJ China
*											All	Rights Reserved
*
* File    : pfhead.h
* By      : Steven.ZGJ
* Version : V1.00
*********************************************************************************************************
*/
#include "kapi.h"
#include <mod_defs.h>
#include "mod_infotainment_i.h"

const __module_mgsec_t modinfo_desktop __attribute__((section(".magic"))) =
{
    {'e', 'P', 'D', 'K', '.', 'm', 'o', 'd'}, //.magic
    0x01000000,                             //.version
    EMOD_TYPE_APPS,                  //.type
    0xF0000,                                //.heapaddr
    0x400,                                  //.heapsize
    {
        //.mif
        &INFOTAINMENT_MInit,
        &INFOTAINMENT_MExit,
        &INFOTAINMENT_MOpen,
        &INFOTAINMENT_MClose,
        &INFOTAINMENT_MRead,
        &INFOTAINMENT_MWrite,
        &INFOTAINMENT_MIoctrl
    }
};
