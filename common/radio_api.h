#ifndef __RADIO_API_H__
#define __RADIO_API_H__ 
#include <stdint.h>
#include <string.h>
#include <infotainment_os_api.h>
#include <bus_router.h>
#include <sys/_intsup.h>

// 消息命令/事件类型
typedef enum {
    RADIO_CMD_UNDEFINED = SRC_RADIO_SERVICE,
/* --- UI -> 后端服务的请求 (CMD - Command) ---*/
        // 播放控制
    RADIO_CMD_PLAY_NEXT,                    // 
    RADIO_CMD_PLAY_PREV,                    //
    RADIO_CMD_SEEK_NEXT,                    // 自动搜索下一个电台
    RADIO_CMD_SEEK_PREV,                    // 自动搜索上一个电台
    RADIO_CMD_TUNE_UP,                      // 频率向上微调 (长按)
    RADIO_CMD_TUNE_DOWN,                    // 频率向下微调 (长按)
    RADIO_CMD_STOP_SEEK,                    // 停止当前的搜台动作
    RADIO_CMD_TOGGLE_MUTE,                  // 切换静音/非静音状态

    // 波段与模式
    RADIO_CMD_SWITCH_FM_BAND,               // 切换FM子波段 (FM1/FM2/FM3)
    RADIO_CMD_SWITCH_AM_BAND,               // 切换AM子波段 (AM1/AM2)
    RADIO_CMD_CHANGE_MODE_FM,               // 切换到FM模式
    RADIO_CMD_CHANGE_MODE_AM,               // 切换到AM模式

    RADIO_CMD_SETUP,                        // 

    // 预设电台
    RADIO_CMD_PLAY_PRESET,                  // 播放指定索引的预设电台
    RADIO_CMD_SAVE_PRESET,                  // 保存当前电台到指定索引 (长按)
    RADIO_CMD_SAVE_PRESET_FROM_LIST,            // 从预设列表中保存电台

    // 功能操作
    RADIO_CMD_AUTO_STORE,                   // 自动搜索并存储电台
    RADIO_CMD_PTY_SEEK,                     // 按 PTY 类型搜索
    RADIO_CMD_TOGGLE_LOC_DX,                // 切换近程(Local)/远程(DX)搜索
    
    // RDS 相关设置
    RADIO_CMD_TOGGLE_RDS,                   // 切换RDS功能总开关
    RADIO_CMD_TOGGLE_RDS_AF,                // 切换AF (替代频率) 功能开关
    RADIO_CMD_TOGGLE_RDS_TA,                // 切换TA (交通警告) 功能开关
    RADIO_CMD_TOGGLE_RDS_CT,                // 切换CT (时钟同步) 功能开关
    RADIO_CMD_SWITCH_PTY_TYPE,              // 在设置中选择要搜索的PTY类型

    // 地区设置
    RADIO_CMD_SWITCH_AREA,                  // 切换收音机地区设置
     // 【新增】内部命令，用于状态机自驱动
    RADIO_CMD_INTERNAL_SEEK_TIMEOUT,
    RADIO_CMD_INTERNAL_TUNE_STEP,

 /* --- 后端服务 -> UI的指令/事件 (EVT - Event) ---*/
    RADIO_EVT_READY,// 服务就绪信号

    // 核心信息更新
    RADIO_EVT_UPDATE_FREQUENCY,             // 事件：当前频率已变更，请UI刷新
    RADIO_EVT_UPDATE_BAND,                  // 事件：当前波段已变更 (FM1/AM2等)，请UI刷新
    RADIO_EVT_UPDATE_PRESET_LIST,           // 事件：预设电台列表已更新，请UI刷新
    
    // 状态指示器更新
    RADIO_EVT_UPDATE_INDICATORS,            // 事件：状态指示灯变更 (如ST, LOC, AF, TA)，请UI刷新
    RADIO_EVT_UPDATE_MUTE_STATUS,           // 事件：静音状态已变更，请UI刷新

    // RDS 信息更新
    RADIO_EVT_UPDATE_RDS_INFO,              // 事件：接收到新的RDS信息 (PS/PTY)，请UI刷新

    // 搜台状态更新
    RADIO_EVT_SEARCH_STARTED,               // 事件：搜台开始，请UI显示 "正在搜索..."
    RADIO_EVT_SEARCH_FINISHED,              // 事件：搜台结束，请UI清除状态显示

    // 搜台状态更新
    RADIO_EVT_SWITCH_BAND_STARTED,               //
    RADIO_EVT_SWITCH_BAND_FINISHED,              //
    
    // 对话框/提示
    RADIO_EVT_SHOW_TEMPORARY_DIALOG,        // 指令：显示一个临时提示信息 (如 "切换中...")
    RADIO_EVT_HIDE_DIALOG,                  // 指令：隐藏提示信息


    RADIO_MAX_CMD
} radio_api_cmd_t;

#define RADIO_MAX_PRESET			30 //0~29
#ifdef NO_FM3_AM2_BAND
#define		RADIO_BAND_MAX	2
#define		RADIO_MAX_CH		15
#else
#define		RADIO_MAX_CH		6
#define		RADIO_BAND_MAX    5
#endif

typedef struct radio_set_preset_payload {
    uint8_t preset_flag;
    uint8_t preset_index;
    uint8_t band;
} radio_set_preset_payload_t;

typedef enum{
    AUTO_SEEK=0,      // 自动搜台
    MANUAL_SEEK, // 手动搜台
    PERSET_SEEK,       // 按PTY类型搜台
    MAX_SEEK_TYPE
}radio_setup_seek_type_e;


typedef enum{
    CUR_NO_SEEK=0,      // 
    CUR_AUTO_SEEK,      // 
    CUR_MANUAL_SEEK, // 
    CUR_PERSET_SEEK,       // 
    CUR_PTY_SEEK, 
    CUR_AUTOSTORE_SEEK,       // 
}radio_cur_seeking_type_e;




typedef enum{
    RADIO_SETUP_SEEK_TYPE=0,      // 
    RADIO_SETUP_AREA, // 
    RADIO_SETUP_RDS_ON_OFF,       //
    RADIO_SETUP_AF_ON_OFF, // 
    RADIO_SETUP_TA_ON_OFF, // TA开关
    RADIO_SETUP_TP_ON_OFF, //
    RADIO_SETUP_CT_ON_OFF, //
    RADIO_SETUP_LOC_ON_OFF, //
    RADIO_SETUP_MONO_ON_OFF, //
    RADIO_SETUP_COUNT//
    
}tuner_setup_type;


typedef struct radio_setup_payload {
    tuner_setup_type setup_type; // 设置类型
    int32_t setting_value; // 设置值 (如地区索引、开关状态
} radio_setup_payload_t;




#define RADIO_EUROPE1_BAND		1
#define RADIO_USA1_BAND			2
#define RADIO_EUROPE2_BAND		3//russia
#define RADIO_LATAM1_BAND		4
#define RADIO_ASIA_BAND			5
#define RADIO_JAPAN_BAND		6
#define RADIO_MID_EAST_BAND		7
#define RADIO_AUSTR_BAND		8
#define RADIO_S_AMERICA_BAND	9
#define RADIO_OTHERS_BAND		10
#define RADIO_OCEANIA_BAND		11
#define RADIO_HONGKONG_BAND		12


//Tuner RDS status mask
#define RADIO_RDS_AF		0x01
#define RADIO_RDS_TA		0x02
#define RADIO_RDS_TP		0x04
#define RADIO_RDS_PS		0x08
#define RADIO_RDS_TRAFFIC	0x10
#define RADIO_ST_FLAG	    0x20

/**
 * @brief Radio服务的消息载荷(payload)结构体定义 (非联合体版本)
 *
 * @details
 * 此结构体将所有可能用到的数据字段作为独立成员直接包含。
 * 使用时，发送方和接收方需要根据消息ID (radio_api_cmd_t)
 * 来约定填充和解析哪个成员字段。
 *
 * 例如，当消息ID是 RADIO_CMD_PLAY_PRESET 时，程序应填充和读取 preset_index 字段。
 * 当消息ID是 RADIO_EVT_UPDATE_FREQUENCY 时，程序应填充和读取 frequency 字段。
 */
typedef struct radio_state_payload {


    uint8_t preset_index;//指是当前电台是否preset电台

    /**
     * @brief 通用设置值
     * @note  适用于需要传递一个枚举ID的命令, 如: RADIO_CMD_SWITCH_AREA
     */
    int32_t setting_value;

    /**
     * @brief 频率值 (例如: 107100 代表 107.1 MHz)
     * @note  适用于事件: RADIO_EVT_UPDATE_FREQUENCY
     */
    uint32_t frequency;

    /**
     * @brief 状态指示灯的位掩码 (bitmask), 用于表示ST, LOC, AF, TA等状态
     * @note  适用于事件: RADIO_EVT_UPDATE_INDICATORS
     */
    uint32_t indicators_mask;

    /**
     * @brief 对话框/提示的超时时间 (毫秒)，0代表不自动消失
     * @note  适用于事件: RADIO_EVT_SHOW_TEMPORARY_DIALOG
     */
    uint32_t timeout_ms;




    char band_str[8];

    /**
     * @brief RDS 电台名称 (Program Service)
     * @note  适用于事件: RADIO_EVT_UPDATE_RDS_INFO
     */
    char ps_name[16];
    char pty_name[16];


    /**
     * @brief 对话框或提示信息文本
     * @note  适用于事件: RADIO_EVT_SHOW_TEMPORARY_DIALOG
     */
    char message[64];

    char band;
    char pty;
    
    uint32_t max_freq;
    uint32_t min_freq;
    uint32_t tu_step;

    #ifdef NO_FM3_AM2_BAND	
	uint32_t	curFreq_Band[3]; 
	uint32_t	FM1_3_AM1_2_freq[3][15]; 
	uint32_t	FM1_3_AM1_2_freq_un[3][15];//if == 0xFFFF,means no valid frequency
    #else
    uint32_t	curFreq_Band[6]; 
	uint32_t	FM1_3_AM1_2_freq[6][6]; 
	uint32_t	FM1_3_AM1_2_freq_un[6][6];
	#endif

    unsigned char tuner_area[32]; // 用于存储地区设置的字符串描述
    unsigned char tuner_area_count; 

    unsigned char area; // 用于存储当前地区的索引
    unsigned char seek_type; // 用于存储搜索类型
    unsigned char pty_seek_type; // 用于存储PTY搜索类型
    unsigned char rds_on; // RDS功能开关
    unsigned char af_on; // AF功能开关
    unsigned char ta_on; // TA功能开关
    unsigned char tp_on; // TP功能开关
    unsigned char ct_on; // CT功能开关
    unsigned char local_seek_on; // CT功能开关
    unsigned char mono_on; // 单声道开关

    radio_cur_seeking_type_e cur_seeking_type; //



} radio_state_payload_t;

#endif