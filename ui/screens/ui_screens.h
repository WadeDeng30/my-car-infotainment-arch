// --- 修改后的文件 ui_screens.h ---

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include <elibs_stdio.h>
#include <elibs_string.h>
#include <stdlib.h>
#include <kapi.h>
#include <lvgl.h>
#include <bus_router.h>
#include <service_system.h>
#include <service_radio.h>
#include <message_defs.h>
#include <ui_manger.h>
#include <infotainment_os_api.h>

#define MAX_WIDGETS_PER_LAYOUT 10 // 每个布局最多允许10个小部件
#define MAX_LAYOUT_PROFILES UI_SCREEN_COUNT     // 您提到的5种不同控件分布界面

#define SCREEN_PRIORITY_1   1000
#define SCREEN_PRIORITY_2   900 
#define SCREEN_PRIORITY_3   800
#define SCREEN_PRIORITY_4   700
#define SCREEN_PRIORITY_5   600
#define SCREEN_PRIORITY_6   500


// 描述单个控件的状态
typedef struct {
    ui_manger_id_t type;         // 控件的类型 (e.g., 是收音机还是时钟)
    bool          is_visible;   // 该控件当前是否可见
    int32_t       x;            // 在父容器中的X坐标
    int32_t       y;            // 在父容器中的Y坐标
    int32_t       width;        // 控件宽度
    int32_t       height;       // 控件高度
    void*         create_arg;   // 【新增】创建控件时传递的特定参数
} widget_state_t;

// 描述一个完整的布局方案 (例如，您的5种界面之一)
typedef struct {
    ui_screen_id_t           profile_id; // 布局方案的唯一ID (e.g., "Layout_A", "Layout_B")
    unsigned short       priority;      // 目标屏幕布局的ID
    bus_router_address_t required_mask;  // 该布局启动所必需的服务地址掩码
    uint32_t             required_timeout; // 该布局启动所必需的服务超时时间
    const char*          description;    // 描述，便于调试
    uint32_t       widget_count;
    widget_state_t widgets[MAX_WIDGETS_PER_LAYOUT];
} layout_profile_t;



/**
 * @brief 【新增】用于表示控件集合的位掩码类型
 * 
 * 假设 MANGER_ID_MAX 不超过 64
 */
typedef uint64_t ui_widget_mask_t;

/**
 * @brief 【新增】根据屏幕ID查询其包含的所有控件类型
 * 
 * 这个函数会遍历指定屏幕布局的控件列表，并返回一个代表所有控件类型的位掩码。
 * 例如，如果一个布局包含 MANGER_ID_CLOCK (ID=3) 和 MANGER_ID_SOURCE (ID=2)，
 * 那么返回的掩码将是 (1 << 3) | (1 << 2) = 8 | 4 = 12。
 * 
 * @param screen_id 要查询的屏幕ID
 * @return 一个 ui_widget_mask_t 类型的位掩码。如果屏幕未找到，返回 0。
 */
ui_widget_mask_t ui_screen_get_widgets_by_id(ui_screen_id_t screen_id);

/**
 * @brief 【新增】辅助宏，用于检查一个掩码中是否包含某个控件
 * @param mask 要检查的掩码
 * @param widget_id 要查找的控件ID
 * @return 如果包含则为 true，否则为 false
 */
#define UI_WIDGET_MASK_HAS_WIDGET(mask, widget_id) (mask&widget_id)//(((mask) & ((ui_widget_mask_t)1 << (widget_id))) != 0)

// --- 公共查询接口 ---

/**
 * @brief 根据屏幕ID获取其完整的布局配置信息
 * @param screen_id 目标屏幕的ID
 * @return 指向 layout_profile_t 的只读指针，未找到则返回 NULL
 */
const layout_profile_t* ui_screen_get_profile_by_id(ui_screen_id_t screen_id);
const layout_profile_t* ui_screen_get_all_profiles(int* count);
/**
 * @brief 3. 【新增】创建开机后的第一个界面
 *
 * 此函数应在 ui_main_init 中被调用，用于构建初始UI。
 * @param initial_screen_id 要创建的初始屏幕ID
 */
void ui_screen_create_initial(ui_screen_id_t initial_screen_id);
void ui_screens_init(void);

/**
 * @brief 4. 【新增】切换到指定的屏幕布局
 *
 * 此函数会销毁当前屏幕上的所有控件，并根据新的布局ID创建新控件。
 * @param screen_id 目标屏幕的ID
 * @param anim_out (可选) 旧屏幕消失的动画模板，传 NULL 则无动画。
 * @param anim_in  (可选) 新屏幕进入的动画模板，传 NULL 则无动画。
 */
void ui_screen_switch_to(ui_screen_id_t screen_id, lv_anim_t* anim_out, lv_anim_t* anim_in);

#endif // UI_SCREENS_H