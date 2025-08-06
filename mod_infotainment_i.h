#ifndef __MOD_INFOTAINMENT_H__
#define __MOD_INFOTAINMENT_H__

#include <typedef.h>
#include <mod_defs.h>

typedef struct
{
    __u32           mid;
}__infotainment_t;


__s32 INFOTAINMENT_MInit(void);
__s32 INFOTAINMENT_MExit(void);
__mp *INFOTAINMENT_MOpen(__u32 mid, __u32 mod);
__s32 INFOTAINMENT_MClose(__mp *mp);
__u32 INFOTAINMENT_MRead(void *pdata, __u32 size, __u32 n, __mp *mp);
__u32 INFOTAINMENT_MWrite(const void *pdata, __u32 size, __u32 n, __mp *mp);
__s32 INFOTAINMENT_MIoctrl(__mp *mp, __u32 cmd, __s32 aux, void *pbuffer);


#endif