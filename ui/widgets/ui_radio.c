#include "ui_manger.h"
#include "bus_router.h"
#include "lvgl.h"
#include <stdio.h>

// --- 私有数据结构 ---
// 用于存储此UI模块所有LVGL对象的指针，方便管理
typedef struct {
    // 主要显示区域
    lv_obj_t* band_label;       // 显示波段 (e.g., "FM1")
    lv_obj_t* freq_label;       // 显示频率 (e.g., "97.5")
    lv_obj_t* ps_name_label;    // 显示电台名称 (e.g., "LOVE 97.2FM")
    lv_obj_t* pty_name_label;   // 显示节目类型 (e.g., "Pop Music")

    // 状态指示灯
    lv_obj_t* st_indicator;     // 立体声 (Stereo) 指示
    lv_obj_t* loc_indicator;    // 近程 (Local) 指示
    lv_obj_t* af_indicator;     // 替代频率 (AF) 指示
    lv_obj_t* ta_indicator;     // 交通警告 (TA) 指示

    // 预设电台按钮
    lv_obj_t* preset_buttons[6]; // 6个预设电台按钮的数组
} radio_ui_t;


// --- 静态函数声明 ---
// LVGL事件回调
static void seek_prev_btn_event_cb(lv_event_t * e);
static void seek_next_btn_event_cb(lv_event_t * e);
static void band_btn_event_cb(lv_event_t * e);
static void preset_btn_event_cb(lv_event_t * e);




// --- 全局变量 ---
// 定义这个控件的操作函数集
static const ui_manger_ops_t radio_ops = {
    .create = radio_widget_create,
    .destroy = radio_widget_destroy,
    .handle_event = radio_widget_handle_event,
    // .show 和 .hide 可以使用默认实现或自定义
};


// --- 公共函数 ---
/**
 * @brief 向UI管理器注册Radio控件
 */
void ui_radio_register(void) {
    ui_manager_register(MANGER_ID_SOURCE, &radio_ops);
}


// --- 静态函数实现 ---

/**
 * @brief 创建Radio主界面的所有LVGL对象
 */
static lv_obj_t* radio_widget_create(lv_obj_t* parent, void* arg) {
    if (!parent) return NULL;

    // 将通用参数指针转换为Radio专用的载荷指针
    // UI管理器应确保在调用create时传递了正确的初始化数据
    msg_payload_service_radio_t *radio_arg = (msg_payload_service_radio_t *)arg;

    // 1. 分配并关联私有数据结构
    radio_ui_t* ui = lv_mem_alloc(sizeof(radio_ui_t));
    if (!ui) return NULL;

    // 2. 创建主容器
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_user_data(cont, ui); // 将ui指针与主容器关联
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- 顶部信息区 ---
    lv_obj_t* top_panel = lv_obj_create(cont);
    lv_obj_set_width(top_panel, lv_pct(95));
    lv_obj_set_flex_flow(top_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(top_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 使用 radio_arg 中的数据来初始化UI元素。如果 radio_arg 为空，则使用默认文本。
    ui->band_label = lv_label_create(top_panel);
    lv_label_set_text(ui->band_label, (radio_arg && radio_arg->band_str[0] != '\0') ? radio_arg->band_str : "FM1");
    lv_obj_set_style_text_font(ui->band_label, lv_font_medium.font, 0);

    ui->freq_label = lv_label_create(top_panel);
    if (radio_arg && radio_arg->frequency > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", (float)radio_arg->frequency / 1000.0f);
        lv_label_set_text(ui->freq_label, buf);
    } else {
        lv_label_set_text(ui->freq_label, "---.-");
    }
    lv_obj_set_style_text_font(ui->freq_label, lv_font_medium.font, 0);

    ui->ps_name_label = lv_label_create(top_panel);
    lv_label_set_text(ui->ps_name_label, (radio_arg && radio_arg->ps_name[0] != '\0') ? radio_arg->ps_name : "Station Name");
    lv_obj_set_style_text_font(ui->ps_name_label, lv_font_medium.font, 0);
    
    ui->pty_name_label = lv_label_create(top_panel);
    lv_label_set_text(ui->pty_name_label, (radio_arg && radio_arg->pty_name[0] != '\0') ? radio_arg->pty_name : "");
    lv_obj_set_style_text_font(ui->pty_name_label, lv_font_medium.font, 0);


    // --- 中部控制区 ---
    lv_obj_t* ctrl_panel = lv_obj_create(cont);
    lv_obj_set_width(ctrl_panel, lv_pct(90));
    lv_obj_set_flex_flow(ctrl_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_panel, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btn_prev = lv_btn_create(ctrl_panel);
    lv_obj_add_event_cb(btn_prev, seek_prev_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_prev = lv_label_create(btn_prev);
    lv_label_set_text(lbl_prev, LV_SYMBOL_PREV);

    lv_obj_t* btn_band = lv_btn_create(ctrl_panel);
    lv_obj_add_event_cb(btn_band, band_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_band = lv_label_create(btn_band);
    lv_label_set_text(lbl_band, "BAND");

    lv_obj_t* btn_next = lv_btn_create(ctrl_panel);
    lv_obj_add_event_cb(btn_next, seek_next_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, LV_SYMBOL_NEXT);

    // --- 底部预设区 ---
    lv_obj_t* preset_panel = lv_obj_create(cont);
    lv_obj_set_width(preset_panel, lv_pct(95));
    lv_obj_set_layout(preset_panel, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(preset_panel, (const lv_coord_t[]){LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, 0);
    lv_obj_set_style_grid_row_dsc_array(preset_panel, (const lv_coord_t[]){LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST}, 0);

    for (int i = 0; i < 6; i++) {
        ui->preset_buttons[i] = lv_btn_create(preset_panel);
        lv_obj_set_grid_cell(ui->preset_buttons[i], LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
        lv_obj_add_event_cb(ui->preset_buttons[i], preset_btn_event_cb, LV_EVENT_ALL, (void*)(intptr_t)i); // 将索引作为用户数据传递

        lv_obj_t* lbl = lv_label_create(ui->preset_buttons[i]);
        lv_label_set_text_fmt(lbl, "P%d", i + 1);
        lv_obj_center(lbl);
    }

    return cont;
}

/**
 * @brief 销毁Radio控件时释放私有数据
 */
static void radio_widget_destroy(ui_manger_t* widget) {
    if (widget && widget->instance) {
        radio_ui_t* ui = (radio_ui_t*)lv_obj_get_user_data(widget->instance);
        // 销毁所有子对象
        lv_obj_clean(widget->instance);
        // 释放私有数据
        lv_obj_set_user_data(widget->instance, NULL); // 清除关联的私有数据
        // 清理控件实例
        lv_obj_del(widget->instance);

        if(ui) {
            // 释放私有数据结构
            lv_mem_free(ui);
        }
       
    }
}

/**
 * @brief 处理来自服务端的事件，并更新UI
 */
static int radio_widget_handle_event(ui_manger_t* widget, const app_message_t* msg) {
    if (!widget || !widget->instance || !msg) return -1;

    // 从 widget->instance (即cont) 中获取私有数据
    radio_ui_t* ui = (radio_ui_t*)lv_obj_get_user_data(widget->instance);
    if (!ui) return -1;

    // 将通用载荷转换为Radio专用载荷
    const msg_payload_service_radio_t* payload = (const msg_payload_service_radio_t*)msg->payload;

    switch (msg->command) {
        // --- 处理核心信息更新事件 ---
        case RADIO_EVT_UPDATE_FREQUENCY:
            if (payload) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", (float)payload->frequency / 1000.0f);
                lv_label_set_text(ui->freq_label, buf);
            }
            break;

        case RADIO_EVT_UPDATE_BAND:
            if (payload) {
                lv_label_set_text(ui->band_label, payload->band_str);
            }
            break;

        case RADIO_EVT_UPDATE_RDS_INFO:
             if (payload) {
                lv_label_set_text(ui->ps_name_label, payload->ps_name);
                lv_label_set_text(ui->pty_name_label, payload->pty_name);
            }
            break;

        // --- 处理指示灯状态更新事件 ---
        case RADIO_EVT_UPDATE_INDICATORS:
            if (payload) {
                // 根据位掩码来显示或隐藏指示灯
                // (假设INDICATOR_MASK_ST, INDICATOR_MASK_LOC等宏已定义)
                // lv_obj_add_flag(ui->st_indicator, (payload->indicators_mask & INDICATOR_MASK_ST) ? 0 : LV_OBJ_FLAG_HIDDEN);
                // lv_obj_add_flag(ui->loc_indicator, (payload->indicators_mask & INDICATOR_MASK_LOC) ? 0 : LV_OBJ_FLAG_HIDDEN);
                // ...
            }
            break;
        
        // --- 处理搜台状态 ---
        case RADIO_EVT_SEARCH_STARTED:
            lv_label_set_text(ui->ps_name_label, "Searching...");
            break;

        case RADIO_EVT_SEARCH_FINISHED:
            // 搜索结束后，服务端会紧接着发送一条UPDATE_RDS_INFO事件，所以这里可以只清除文本
            lv_label_set_text(ui->ps_name_label, ""); 
            break;

        default:
            // 其他不关心的消息，直接忽略
            break;
    }
    return 0;
}


// --- LVGL 事件回调函数 ---

static void seek_prev_btn_event_cb(lv_event_t * e) {
    // 发送“上一个电台”命令到服务端
    bus_post_message(SRC_UI, ADDRESS_RADIO_SERVICE,MSG_PRIO_NORMAL, RADIO_CMD_SEEK_PREV, NULL, NULL);
}

static void seek_next_btn_event_cb(lv_event_t * e) {
    // 发送“下一个电台”命令到服务端
    bus_post_message(SRC_UI, ADDRESS_RADIO_SERVICE,MSG_PRIO_NORMAL, RADIO_CMD_SEEK_NEXT, NULL, NULL);
}

static void band_btn_event_cb(lv_event_t * e) {
    // 发送“切换波段”命令
    // 这里可以根据当前波段决定是发FM还是AM切换，或者让服务端自己决定
    bus_post_message(SRC_UI, ADDRESS_RADIO_SERVICE,MSG_PRIO_NORMAL, RADIO_CMD_SWITCH_FM_BAND, NULL, NULL);
}

static void preset_btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    intptr_t index = (intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        // 短按：播放预设
        bus_post_message(SRC_UI, ADDRESS_RADIO_SERVICE,MSG_PRIO_NORMAL, RADIO_CMD_PLAY_PRESET, (void *)&index, NULL);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        // 长按：保存预设
        bus_post_message(SRC_UI, ADDRESS_RADIO_SERVICE,MSG_PRIO_NORMAL, RADIO_CMD_SAVE_PRESET, (void *)&index, NULL);
        
    }
}
