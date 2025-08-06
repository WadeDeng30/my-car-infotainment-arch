#include "radio_hal.h"
#include <sys/_types.h>


#define FM_DEVICE_PATH "/dev/fm"

static int g_fm_dev_handle = -1;

// 内部辅助函数，封装ioctl调用
static inline radio_hal_status_t fm_ioctl(__u32 cmd, void *arg) {
    if (g_fm_dev_handle < 0) {
        return RADIO_HAL_DEVICE_NOT_OPEN;
    }
    if (ioctl(g_fm_dev_handle, cmd, arg) == 0) {
        return RADIO_HAL_OK;
    }
    return RADIO_HAL_ERROR;
}

radio_hal_status_t radio_hal_init(void) {
    if (g_fm_dev_handle >= 0) {
        return RADIO_HAL_OK; // 已经初始化
    }
    g_fm_dev_handle = open(FM_DEVICE_PATH, O_RDWR);
    if (g_fm_dev_handle < 0) {
        printf("[RadioHAL-ERROR] Failed to open %s\n", FM_DEVICE_PATH);
        return RADIO_HAL_DEVICE_NOT_OPEN;
    }
    
    // 注意：原始代码中的很多硬件模式选择逻辑（如SI4634）
    // 应该在这里根据系统配置（例如从 dsk_reg_get_para_by_app 获取）来执行
    // 这里简化为只执行通用的初始化
    
    printf("[RadioHAL-INFO] Device %s opened, handle: %d\n", FM_DEVICE_PATH, g_fm_dev_handle);
    return RADIO_HAL_OK;
}

void radio_hal_deinit(void) {
    if (g_fm_dev_handle >= 0) {
        close(g_fm_dev_handle);
        g_fm_dev_handle = -1;
        printf("[RadioHAL-INFO] Device %s closed.\n", FM_DEVICE_PATH);
    }
}

bool radio_hal_is_ready(void) {
    return g_fm_dev_handle >= 0;
}

radio_hal_status_t radio_hal_switch_band(uint32_t band_index) {
    return fm_ioctl(DRV_FM_CMD_SWITCH_BAND, (void*)&band_index);
}

radio_hal_status_t radio_hal_switch_band_and_tune(uint32_t band_index,uint32_t preset_index) {
    unsigned long args[3]={0};
    args[0] = band_index;
    args[1] = preset_index;
    args[2] = 0; // 保留参数

    return fm_ioctl(DRV_FM_CMD_SWITCH_BAND, (void*)args);
}

radio_hal_status_t radio_hal_set_frequency(uint32_t freq_khz) {
    // 假设驱动的 DRV_FM_CMD_KEY 包含了设置频率的功能
    // 实际需要根据驱动定义调整
    // return fm_ioctl(DRV_FM_CMD_SET_FREQ, (void*)freq_khz);
    return RADIO_HAL_ERROR; // 示例：未实现
}

radio_hal_status_t radio_hal_play_preset(uint32_t preset_index) {
    return fm_ioctl(DRV_FM_CMD_TUNE_PRESET, (void*)&preset_index);
}

radio_hal_status_t radio_hal_set_preset(uint32_t preset_index) {
    return fm_ioctl(DRV_FM_CMD_SET_PRESET_LIST, (void*)&preset_index);
}

radio_hal_status_t radio_hal_cancel_preset(uint32_t preset_index) {
    return fm_ioctl(DRV_FM_CMD_CANCEL_PRESET_LIST, (void*)&preset_index);
}

radio_hal_status_t radio_hal_seek_up(void) {
	int temp_arg = FM_PREV;
	int stop_flag = 0;

	fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP_FLAG,(void *)&stop_flag);
    return fm_ioctl(DRV_FM_CMD_KEY, (void*)&temp_arg); // 使用 ui_fm_helpers.c 中的宏
}

radio_hal_status_t radio_hal_seek_down(void) {
	int temp_arg = FM_NEXT;
	int stop_flag = 0;

	fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP_FLAG,(void *)&stop_flag);
    return fm_ioctl(DRV_FM_CMD_KEY, (void*)&temp_arg);
}

radio_hal_status_t radio_hal_tune_up(void) {
	int stop_flag = 0;
	int temp_arg = FM_MANUAL_SEEK_UP;
	fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP_FLAG,(void *)&stop_flag);
    return fm_ioctl(DRV_FM_CMD_KEY, (void*)&temp_arg);
}

radio_hal_status_t radio_hal_tune_down(void) {
	int stop_flag = 0;
	int temp_arg = FM_MANUAL_SEEK_DOWN;
	fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP_FLAG,(void *)&stop_flag);
    return fm_ioctl(DRV_FM_CMD_KEY, (void*)&temp_arg);
}

radio_hal_status_t radio_hal_seek_stop(void) {
    //先停止搜台，再设置停止标志
	int stop_flag = 1;
	fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP, (void*)0);
    return fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP_FLAG, (void*)&stop_flag);
}

radio_hal_status_t radio_hal_auto_store(void) {
	int temp_arg = FM_AUTO_STORE;
	int stop_flag = 0;

	fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP_FLAG,(void *)&stop_flag);
    return fm_ioctl(DRV_FM_CMD_KEY, (void*)&temp_arg);
}

radio_hal_status_t radio_hal_pty_seek(uint32_t pty_code) {
    int temp_arg = FM_AUTO_STORE;
	int stop_flag = 0;

	fm_ioctl(DRV_FM_CMD_SET_SEARCH_STOP_FLAG,(void *)&stop_flag);
    return fm_ioctl(DRV_FM_CMD_PTY_SEEK, (void*)&pty_code);
}

radio_hal_status_t radio_hal_set_area(uint32_t area_code) {
    return fm_ioctl(DRV_FM_CMD_SET_ARE, (void*)&area_code);
}

radio_hal_status_t radio_hal_set_loc_dx_mode(bool is_local) {
    uint32_t mode = is_local ? LOCAL_SEEK_ON : LOCAL_SEEK_OFF;
    return fm_ioctl(DRV_FM_CMD_SET_LOCAL_SEEK_ON_OFF, (void*)&mode);
}

radio_hal_status_t radio_hal_set_rds_enabled(bool enabled) {
    uint32_t mode = enabled ? RDS_ON : RDS_OFF;
    return fm_ioctl(DRV_FM_CMD_SET_RDS_ON_OFF, (void*)&mode);
}

radio_hal_status_t radio_hal_set_rds_af_enabled(bool enabled) {
    uint32_t mode = enabled ? RDS_AF_ON : RDS_AF_OFF;
    return fm_ioctl(DRV_FM_CMD_SET_RDS_AF_ON_OFF, (void*)&mode);
}

radio_hal_status_t radio_hal_set_rds_ta_enabled(bool enabled) {
    uint32_t mode = enabled ? RDS_TA_ON : RDS_TA_OFF;
    return fm_ioctl(DRV_FM_CMD_SET_RDS_TA_ON_OFF, (void*)&mode);
}

radio_hal_status_t radio_hal_set_rds_ct_enabled(bool enabled) {
    uint32_t mode = enabled ? RDS_CT_ON : RDS_CT_OFF;
    return fm_ioctl(DRV_FM_CMD_SET_RDS_CT_ON_OFF, (void*)&mode);
}

radio_hal_status_t radio_hal_set_seek_type(uint32_t seek_type) {
    // 这里假设 DRV_FM_CMD_SET_SEEK_TYPE 是设置搜台类型的命令
    return fm_ioctl(DRV_FM_CMD_SET_SEEK_TYPE, (void*)&seek_type);
}

__s32 radio_hal_fm_get_init_status(void)
{
	__u8 status = -1;
	__s32 ret=0;

	ret = fm_ioctl(DRV_FM_CMD_TUNE_INIT_STATUS,(void *)&status);
	if(ret != RADIO_HAL_OK)
		status = -1;
	
	return status;	 
}






