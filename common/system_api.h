#ifndef __SYSTEM_API_H__
#define __SYSTEM_API_H__

// 消息命令/事件类型
typedef enum {
    SYSTEM_CMD_UNDEFINED = SRC_SYSTEM_SERVICE,
    // --- UI -> 后端服务的请求 (CMD - Command) ---
    SYSTEM_CMD_REQUEST_SWITCH_SCREEN,       // 请求跳转到新界面
    SYSTEM_CMD_RELEASE_CURRENT_SCREEN,       // 

        // --- 来自服务的强制请求 ---
    SYSTEM_CMD_FORCE_REQUEST_DISPLAY,   // 服务请求强制显示一个屏幕
    SYSTEM_CMD_FORCE_RELEASE_DISPLAY,   // 服务释放其强制占用的屏幕

    // --- 来自UI的用户导航请求 ---
    SYSTEM_CMD_USER_NAVIGATE_TO,        // 用户请求导航到某个常规屏幕
    SYSTEM_CMD_USER_MINIMIZE_FORCED,    // 用户请求最小化当前可最小化的强制屏幕
    SYSTEM_CMD_USER_RESTORE_FORCED,     // 用户请求恢复一个被最小化的强制屏幕


    // --- 后端服务 -> UI的指令/事件 (EVT - Event) ---
    SYSTEM_EVT_READY = SRC_SYSTEM_SERVICE+0x2000,// 服务就绪信号
    SYSTEM_EVT_SYSTEM_OPEN_UI,// 指令：系统允许UI开机的第一个界面
    SYSTEM_EVT_SYSTEM_SWITCH_TO_SCREEN,// 指令：跳转到指定屏幕
        // --- 广播事件 ---
    SYSTEM_EVT_FORCED_STACK_CHANGED,    // 强制显示栈内容发生变化时广播

    SYSTEM_MAX_CMD
} system_api_cmd_t;



// 为所有可管理的控件/窗口定义一个唯一的ID
typedef enum {
    MANGER_ID_NONE=0,
    MANGER_ID_SIDEBAR_MENU=0x01,
    MANGER_ID_SOURCE=0x02,
    MANGER_ID_CLOCK=0x04,    
    MANGER_ID_ADS_CONTROLS=0x08,    
    MANGER_ID_SYSTEM_CONTROLS=0x10,    
    MANGER_ID_PHONE_MIRROR=0x20,
    MANGER_ID_SETTINGS=0x40,    
    MANGER_ID_QUICK_ACCESS=0x80,   


    // ... 添加更多
    MANGER_ID_MAX,
} ui_manger_id_t;

/**
 * @brief 定义所有本地播放源的类型
 */
typedef enum {
    UI_SOURCE_TYPE_NONE,
    UI_SOURCE_TYPE_FM,
    UI_SOURCE_TYPE_AM, // 假设 AM 和 FM 都由 Radio Service 管理
    UI_SOURCE_TYPE_DAB,
    UI_SOURCE_TYPE_BT_MUSIC,
    UI_SOURCE_TYPE_USB_MUSIC,
} ui_source_type_t;




// UI界面ID定义
typedef enum {
    //HOME
    UI_SCREEN_NONE = 0, // 未定义的屏幕
    UI_SCREEN_HOME,//default
    UI_SCREEN_HOME_SOURCE,//source 展开
    UI_SCREEN_HOME_HALT_ADS,//展开ADS

    //video for movie、landspace mirror、DP、carplay、AA ...
    UI_SCREEN_VIDEO,//default

    
    UI_SCREEN_SETTINGS,
    UI_SCREEN_COUNT,//最大layout数量
} ui_screen_id_t;



/**
 * @brief 【新增】定义媒体会话的上下文意图 (Media Context Intent)
 * 
 * 这个枚举描述了请求显示和播放的“原因”或“性质”。
 */
typedef enum {
    MEDIA_INTENT_NONE = 0,
    
    // --- 高优先级、强制性意图 ---
    MEDIA_INTENT_PHONE_CALL,          // 电话通话
    MEDIA_INTENT_REVERSE_BEEP,        // 倒车提示音
    MEDIA_INTENT_ADAS_ALERT,          // ADAS 警告
    MEDIA_INTENT_TRAFFIC_ANNOUNCEMENT, // 交通信息通报 (TA/TI)

    // --- 中等优先级、瞬时意图 ---
    MEDIA_INTENT_NAVIGATION_PROMPT,   // 导航语音播报
    MEDIA_INTENT_VR_SESSION,          // 语音识别会话

    // --- 普通优先级、用户选择的媒体娱乐 ---
    MEDIA_INTENT_USER_MEDIA_PLAYBACK, // 用户主动选择的媒体播放 (FM, BT, USB...)

    // --- 低优先级意图 ---
    MEDIA_INTENT_SYSTEM_SOUND,        // 系统提示音 (如按键音)
    
} media_intent_t;



/**
 * @brief 显示意图 (Display Intent)
 * 
 * 用于描述一个模块想要在屏幕上呈现什么内容的核心需求。
 * 这是一个纯数据结构，用于在消息中传递。
 */
typedef struct {
    /**
     * @brief 核心：请求显示的控件ID。
     * 例如 MANGER_ID_PHONE_CALL_WIDGET, MANGER_ID_SOURCE 等。
     */
    ui_manger_id_t   widget_id;
    
    /**
     * @brief (可选) 内容源类型。
     * 当 widget_id 是 MANGER_ID_SOURCE 时，此字段用于指定
     * 具体的内容源，如 FM, BT_MUSIC, TA 等。
     * 对于其他 widget_id，此字段可以忽略或设为 UI_SOURCE_TYPE_NONE。
     */
    ui_source_type_t source_type;

    // (未来可扩展)
    // int requested_size; // 例如：希望大窗口还是小窗口
    // int requested_position; // 例如：希望在屏幕左侧还是右侧
    
} display_intent_t;


// 2. Payload 结构体
// 服务发起强制请求的 payload
// 1. 服务发起强制请求的 payload
typedef struct {
    display_intent_t intent;         // 【核心】服务表达的显示意图
    media_intent_t   media_intent;   // “为什么”: 因为这是一条交通警告
    bool             is_minimizable; // 是否可被用户最小化
} force_request_payload_t;

// 【修改】请求的 payload 现在包含 intent
typedef struct {
    display_intent_t intent;         // “是什么”: 我要显示 SOURCE 控件，内容是 FM
    bool             is_minimizable;
} request_display_intent_payload_t;

// 用户恢复的 payload
typedef struct {
    service_id_t requester_id; // 要恢复的服务的ID
} user_restore_payload_t;

// 发送给UI的 SWITCH_TO_SCREEN 指令的 payload
typedef struct {
    ui_screen_id_t   screen_id;
    ui_source_type_t initial_source_type;
} switch_to_screen_payload_t;



// 强制栈变化事件的 Payload
#define MAX_FORCE_STACK_DEPTH 5
typedef struct {
    display_intent_t intent;
    service_id_t   requester_id;
    bool           is_minimized;
} forced_stack_item_info_t;

typedef struct {
    int count;
    forced_stack_item_info_t stack[MAX_FORCE_STACK_DEPTH];
} forced_stack_changed_payload_t;




typedef struct system_resquest_switch_screen_payload{
    ui_manger_id_t widget_id; // 指定要切换的控件ID
    ui_source_type_t source_type; // 告诉UI，Source控件应该以何种模式创建
} system_resquest_switch_screen_payload_t;



typedef struct system_state_payload{
    ui_screen_id_t screen_id; // 当前屏幕状态
     ui_source_type_t initial_source_type; // 告诉UI，Source控件应该以何种模式创建
} system_state_payload_t;

#endif