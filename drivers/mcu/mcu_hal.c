#include "mcu_hal.h"

static reg_all_para_t *infotainment_mcu_all_app_para = NULL;

__s32 mcu_hal_init_para(void)
{
	__s32 ret = EPDK_FAIL;
	
	if (infotainment_mcu_all_app_para != NULL)
    {
        __msg("para already init...");
        return EPDK_OK;
    }
	else
	{   
		int fmcu;				//twi bus handle
		__hdle paramter_hdle=NULL;
		
		fmcu = open("/dev/mcu",O_RDWR);

		if(fmcu >= 0)
		{
			paramter_hdle=ioctl(fmcu, MCU_GET_PARAMTER_HDLE, (void *)0);

			if(paramter_hdle)
			{
				infotainment_mcu_all_app_para = (reg_all_para_t *)paramter_hdle;
				__msg("paramter_hdle=0x%2x..\n",paramter_hdle);

				close(fmcu);
				fmcu=NULL;
				
				return EPDK_OK;
			}

			close(fmcu);
			fmcu=NULL;
		}
	}


	return ret;	
}


void *mcu_hal_get_reg_handle_by_service_id(__u32 eApp)
{
    if (!infotainment_mcu_all_app_para)
    {
        __err("reg para not init...");
        return NULL;
    }

    switch (eApp)
    {
        case REG_APP_SYSTEM:
            return (void *)&infotainment_mcu_all_app_para->para_current.system_para;

        case REG_APP_INIT:
            return (void *)&infotainment_mcu_all_app_para->para_current.init_para;

        case REG_APP_ROOT:
            return (void *)&infotainment_mcu_all_app_para->para_current.root_para;

        case REG_APP_MOVIE:
            return (void *)&infotainment_mcu_all_app_para->para_current.movie_para;

        case REG_APP_MUSIC:
            return (void *)&infotainment_mcu_all_app_para->para_current.music_para;

        case REG_APP_PHOTO:
            return (void *)&infotainment_mcu_all_app_para->para_current.photo_para;

        case REG_APP_EBOOK:
            return (void *)&infotainment_mcu_all_app_para->para_current.ebook_para;

        case REG_APP_FM:
            return (void *)&infotainment_mcu_all_app_para->para_current.fm_para;

		case REG_APP_DAB:
			return (void *)&infotainment_mcu_all_app_para->para_current.dab_para;	

		case REG_APP_AUX:
            return (void *)&infotainment_mcu_all_app_para->para_current.aux_para;	

        case REG_APP_RECORD:
            return (void *)&infotainment_mcu_all_app_para->para_current.record_para;

        case REG_APP_CALENDAR:
            return (void *)&infotainment_mcu_all_app_para->para_current.calendar_para;

        case REG_APP_GAME:
            return (void *)&infotainment_mcu_all_app_para->para_current.game_para;

        case REG_APP_DICT:
            return (void *)&infotainment_mcu_all_app_para->para_current.dict_para;

		case REG_APP_BT:
			return (void *)&infotainment_mcu_all_app_para->para_current.bluetooth_para;	
#if 0//def UPDATE_BY_OTA			
		case REG_APP_WIFI:
			return (void *)&infotainment_mcu_all_app_para->para_current.wifi_para;	
#endif
        default:
            break;
    }

    __err("invalid para...");
    return NULL;
}









