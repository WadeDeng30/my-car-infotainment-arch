/*
* =====================================================================================
*
*       Filename:  linker.lds
*
*    Description:  This file describes the memory layout (memory blocks) as virtual
*                  memory addresses.This linker script is passed to the binutils ld
*                  with '-T' options and then to link compiler output into elf run-
*                  able output file.
*
*
*        Version:  1.0
*        Created:  2017年08月26日 11时52分57秒
*       Revision:  none
*
*         Author:  czl
*   Organization: AllWinner Technology.
*
* =====================================================================================
*/

MEMORY
{
    /* dram0 area, mmu enabled, 64M size, flat mapping of dram0_0_seg. */
    dram0_0_seg (RWX) :                 org = 0xc2000000, len = 0x4000000

    /* modules area, mmu enabled, 256M size. */
    dram0_1_seg (RWX) :                 org = 0xe0000000, len = 0x10000000

    /* shell area, mmu enabled, 64M size, flat mapping of dram0_0_seg. */
    dram0_2_seg (RWX) :                 org = 0xe9200000, len = 0x2000000

    /* MAGIC section area. just use in loader. */
    magic_info_seg (R) :                org = 0xffff0000, len = 0x100
}

INCLUDE configuration.lds

/* must be defined in the code. */
ENTRY(LEOPARD_MInit)

INCLUDE section.lds