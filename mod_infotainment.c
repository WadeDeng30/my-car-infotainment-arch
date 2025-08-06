/*
**************************************************************************************************************
*											         ePDK
*						            the Easy Portable/Player Develop Kits
*									           infotainment system
*
*						        	 (c) Copyright 2007-2010, ANDY, China
*											 All Rights Reserved
*
* File    	: mod_infotainment.c
* By      	: Andy.zhang
* Func		: system lib
* Version	: v1.0
* ============================================================================================================
* 2009-8-25 8:53:32  andy.zhang  create this file, implements the fundemental interface;
**************************************************************************************************************
*/
#include "infotainment_main.h"
#include "mod_infotainment_i.h"
#include <log.h>
#include <stdio.h>
#include <license_check.h>
#include <elibs_stdio.h>

__infotainment_t infotainment_data;

#define __msg(...)			(eLIBs_printf("INIT MSG:L%d:", __LINE__),				 \
                                            eLIBs_printf(__VA_ARGS__)) 




/*
****************************************************************************************************
*
*             INFOTAINMENT_MInit
*
*  Description:
*       INFOTAINMENT_MInit
*
*  Parameters:
*
*  Return value:
*       EPDK_OK
*       EPDK_FAIL
****************************************************************************************************
*/
__s32 INFOTAINMENT_MInit(void)
{
    return EPDK_OK;
}
/*
****************************************************************************************************
*
*             INFOTAINMENT_MExit
*
*  Description:
*       INFOTAINMENT_MExit
*
*  Parameters:
*
*  Return value:
*       EPDK_OK
*       EPDK_FAIL
****************************************************************************************************
*/
__s32 INFOTAINMENT_MExit(void)
{
    return EPDK_OK;
}
/*
****************************************************************************************************
*
*             INFOTAINMENT_MOpen
*
*  Description:
*       INFOTAINMENT_MOpen
*
*  Parameters:
*
*  Return value:
*       EPDK_OK
*       EPDK_FAIL
****************************************************************************************************
*/
__mp *INFOTAINMENT_MOpen(__u32 mid, __u32 mod)
{
    __msg("----------------------INFOTAINMENT_MOpen --------------------------\n");

    infotainment_data.mid  = mid;

    infotainment_main((void *)&infotainment_data);

    return (__mp *)&infotainment_data;
}
/*
****************************************************************************************************
*
*             INFOTAINMENT_MClose
*
*  Description:
*       INFOTAINMENT_MClose
*
*  Parameters:
*
*  Return value:
*       EPDK_OK
*       EPDK_FAIL
****************************************************************************************************
*/
__s32 INFOTAINMENT_MClose(__mp *mp)
{
    // __here__;
    // __here__;

    return EPDK_OK;
}
/*
****************************************************************************************************
*
*             INFOTAINMENT_MRead
*
*  Description:
*       INFOTAINMENT_MRead
*
*  Parameters:
*
*  Return value:
*       EPDK_OK
*       EPDK_FAIL
****************************************************************************************************
*/
__u32 INFOTAINMENT_MRead(void *pdata, __u32 size, __u32 n, __mp *mp)
{
    return size * n;
}
/*
****************************************************************************************************
*
*             INFOTAINMENT_MWrite
*
*  Description:
*       INFOTAINMENT_MWrite
*
*  Parameters:
*
*  Return value:
*       EPDK_OK
*       EPDK_FAIL
****************************************************************************************************
*/
__u32 INFOTAINMENT_MWrite(const void *pdata, __u32 size, __u32 n, __mp *mp)
{
    return size * n;
}
/*
****************************************************************************************************
*
*             INFOTAINMENT_MIoctrl
*
*  Description:
*       INFOTAINMENT_MIoctrl
*
*  Parameters:
*
*  Return value:
*       EPDK_OK
*       EPDK_FAIL
****************************************************************************************************
*/
__s32 INFOTAINMENT_MIoctrl(__mp *mp, __u32 cmd, __s32 aux, void *pbuffer)
{
    switch (cmd)
    {
        default:
        {
            printf("INFOTAINMENT_MIoctrl cmd=%d\n", cmd);
            app_ioctrl_check(mp,cmd,aux,pbuffer);
        }
        break;
    }

    return EPDK_OK;
}






