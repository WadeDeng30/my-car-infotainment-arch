// --- 修改后的文件 ui_screens.c ---

#include "ui_screens.h"
#include "ui_screens_bg.h"
// --- 私有全局变量 ---
static lv_obj_t* g_current_screen_container = NULL; // 指向当前屏幕的根容器
static ui_screen_id_t g_current_screen_id = UI_SCREEN_NONE; // 当前屏幕的ID

// --- 私有函数声明 ---
static void screen_destroy_all_widgets(lv_obj_t* screen);
static void screen_create_widgets_from_profile(lv_obj_t* screen, const layout_profile_t* profile);


/**
 * @brief 1. 【新增】为每个布局中需要特定参数的控件定义静态配置变量
 *
 * 这样做可以让我们在 g_default_ui_layouts 中直接引用这些变量的地址。
 */
//static ui_source_create_config_t g_source_fm_config = { .initial_source = UI_SOURCE_TYPE_FM };
//static ui_source_create_config_t g_source_bt_config = { .initial_source = UI_SOURCE_TYPE_BT_MUSIC };



static  layout_profile_t g_default_layout_profiles[] = {
    {
        .profile_id = UI_SCREEN_HOME,
        .priority = SCREEN_PRIORITY_1,
        .required_mask = (ADDRESS_RADIO_SERVICE | ADDRESS_SYSTEM_SERVICE), // 依赖 Radio, System, 和 Msg 服务
        .required_timeout = 5000, // 5秒超时
        .description = "Default Home Screen",
        .widget_count = 5,
        .widgets = {
            { .type = MANGER_ID_SIDEBAR_MENU, .is_visible = true, .x = 0,  .y = 0, .width = 80,  .height = 720, .create_arg = NULL },
            { .type = MANGER_ID_QUICK_ACCESS, .is_visible = true, .x = 100, .y = 20, .width = 340, .height = 680, .create_arg = NULL },
            // 这里为 SOURCE 控件指定了创建参数，告诉它要创建一个 FM 播放器
            { .type = MANGER_ID_SOURCE,       .is_visible = true, .x = 460, .y = 20, .width = 390, .height = 680, .create_arg = NULL },
            { .type = MANGER_ID_CLOCK,        .is_visible = true, .x = 870, .y = 20,.width = 390, .height = 312, .create_arg = NULL },
            { .type = MANGER_ID_SYSTEM_CONTROLS, .is_visible = true, .x = 870, .y = 352,.width = 390, .height = 348, .create_arg = NULL },
        }
    },
    {
        .profile_id = UI_SCREEN_HOME_HALT_ADS,
        .priority = SCREEN_PRIORITY_1,
        .required_mask = (ADDRESS_RADIO_SERVICE | ADDRESS_SYSTEM_SERVICE | ADDRESS_MCU_SERVICE), // 假设它依赖 MCU 服务
        .required_timeout = 2000, // 2秒超时
        .description = "Home Screen with ADS",
        .widget_count = 5,
        .widgets = {
            { .type = MANGER_ID_SIDEBAR_MENU, .is_visible = true, .x = 20,  .y = 20, .width = 90,  .height = 680, .create_arg = NULL },
            { .type = MANGER_ID_CLOCK,        .is_visible = true, .x = 130, .y = 20, .width = 200, .height = 220, .create_arg = NULL },
            // 在这个布局中，Source控件可能显示的是不同的内容，比如蓝牙音乐
            { .type = MANGER_ID_SOURCE,       .is_visible = true, .x = 340, .y = 20, .width = 450, .height = 220, .create_arg = NULL },
            { .type = MANGER_ID_QUICK_ACCESS, .is_visible = true, .x = 810, .y = 20, .width = 450, .height = 220, .create_arg = NULL },
            { .type = MANGER_ID_ADS_CONTROLS, .is_visible = true, .x = 130, .y = 260,.width = 1130,.height = 440, .create_arg = NULL },
        }
    }
};

/**
 * @brief 运行时使用的布局配置 (可读写)
 * 这个数组在系统启动时，会首先被默认配置初始化，
 * 然后尝试从外部文件（如JSON）加载并覆盖。
 */
static layout_profile_t g_runtime_layout_profiles[MAX_LAYOUT_PROFILES];
static int g_num_runtime_profiles = 0;


/**
 * @brief 查询函数现在应该从可写的 g_runtime_layout_profiles 中查找
 */
const layout_profile_t* ui_screen_get_profile_by_id(ui_screen_id_t screen_id) {
    for (int i = 0; i < g_num_runtime_profiles; i++) {
        if (g_runtime_layout_profiles[i].profile_id == screen_id) {
            return &g_runtime_layout_profiles[i]; // 返回指向可写内存的 const 指针
        }
    }
    return NULL;
}

/**
 * @brief 【实现】获取所有已定义的布局配置文件的列表
 */
const layout_profile_t* ui_screen_get_all_profiles(int* count) {
    if (count) {
        *count = g_num_runtime_profiles;
    }
    return g_runtime_layout_profiles; // 直接返回静态配置数组的指针
}


/**
 * @brief 【新增】根据屏幕ID查询其包含的所有控件类型的实现
 */
ui_widget_mask_t ui_screen_get_widgets_by_id(ui_screen_id_t screen_id) {
    // 1. 首先，找到对应的布局配置文件
    const layout_profile_t* profile = ui_screen_get_profile_by_id(screen_id);
    
    if (!profile) {
        // 如果找不到屏幕，返回一个空掩码
        return 0;
    }
    
    // 2. 初始化一个空掩码
    ui_widget_mask_t widget_mask = 0;
    
    // 3. 遍历该布局的所有控件
    for (int i = 0; i < profile->widget_count; i++) {
        // 获取控件的类型ID
        ui_manger_id_t widget_type = profile->widgets[i].type;
        
        // 检查ID是否在有效范围内 (防止位移溢出)
        if (widget_type > MANGER_ID_NONE && widget_type < MANGER_ID_MAX) {
            // 将对应的位置为 1
            widget_mask |= ((ui_widget_mask_t)1 << widget_type);
        }
    }
    
    return widget_mask;
}



/**
 * @brief 创建开机后的第一个界面
 */
void ui_screen_create_initial(ui_screen_id_t initial_screen_id) {
    const layout_profile_t* profile = ui_screen_get_profile_by_id(initial_screen_id);
    if (!profile) {
        printf("Screen Manager: Initial screen ID %d not found!\n", initial_screen_id);
        // 可以创建一个错误提示界面
        return;
    }

    // LVGL 的默认屏幕作为我们的根容器
    
    g_current_screen_container = lv_scr_act();
    lv_obj_remove_style_all(g_current_screen_container); // 移除所有样式，作为透明容器
    lv_obj_set_style_bg_color(g_current_screen_container, lv_color_hex(0x101010), 0); // 设置一个深色背景
	screen_create_background(g_current_screen_container,initial_screen_id);
    screen_create_widgets_from_profile(g_current_screen_container, profile);

    g_current_screen_id = initial_screen_id;
    printf("Screen Manager: Initial screen '%d' created.\n", initial_screen_id);
}


/**
 * @brief 切换到指定的屏幕布局
 */
void ui_screen_switch_to(ui_screen_id_t screen_id, lv_anim_t* anim_out, lv_anim_t* anim_in) {
    if (g_current_screen_id == screen_id) {
        return; // 目标是当前屏幕，无需切换
    }

    const layout_profile_t* profile = ui_screen_get_profile_by_id(screen_id);
    if (!profile) {
        printf("Screen Manager: Switch to screen ID %d failed, profile not found!\n", screen_id);
        return;
    }

    // 1. 销毁旧屏幕上的所有控件
    screen_destroy_all_widgets(g_current_screen_container);

    // 【预留动画接口】
    // 在实际项目中，这里会更复杂。通常会创建两个屏幕对象，
    // 一个用于旧屏幕滑出，一个用于新屏幕滑入。
    // 为简化，我们目前只做即时切换。
    if (anim_out || anim_in) {
        printf("Screen Manager: Animation is not yet implemented.\n");
        // 伪代码:
        // lv_obj_t* old_screen = g_current_screen_container;
        // lv_obj_t* new_screen = lv_obj_create(lv_scr_act());
        // screen_create_widgets_from_profile(new_screen, profile);
        // start_animation(old_screen, new_screen, anim_out, anim_in);
    }

    // 2. 在同一个容器上创建新屏幕的控件
    screen_create_widgets_from_profile(g_current_screen_container, profile);

    g_current_screen_id = screen_id;
    printf("Screen Manager: Switched to screen '%d'.\n", screen_id);
}


// --- 私有辅助函数 ---

/**
 * @brief 销毁一个屏幕容器内的所有由 ui_manager 管理的子控件
 */
static void screen_destroy_all_widgets(lv_obj_t* screen) {
    if (!screen) return;

    uint32_t child_cnt = lv_obj_get_child_cnt(screen);
    // 从后往前遍历并删除，因为删除操作会改变子对象列表
    for (int i = child_cnt - 1; i >= 0; i--) {
        lv_obj_t* child = lv_obj_get_child(screen, i);
        if (child) {
            ui_manger_t* manger_ctx = (ui_manger_t*)lv_obj_get_user_data(child);
            if (manger_ctx) {
                // 使用 ui_manager 来销毁，确保状态被正确清理
                ui_manager_destroy(manger_ctx->id);
            } else {
                // 如果不是由 manager 管理的，直接删除
                lv_obj_del(child);
            }
        }
    }
}


/**
 * @brief 根据布局配置，在一个屏幕容器上创建所有控件
 */
static void screen_create_widgets_from_profile(lv_obj_t* screen, const layout_profile_t* profile) {
    if (!screen || !profile) return;

    for (int i = 0; i < profile->widget_count; ++i) {
        const widget_state_t* widget_cfg = &profile->widgets[i];

        // 使用 ui_manager 创建控件，并传入特定参数
        lv_obj_t* widget_instance = ui_manager_create(widget_cfg->type, screen, widget_cfg->create_arg);

        if (widget_instance) {
            // 设置控件的位置和大小
            lv_obj_set_pos(widget_instance, widget_cfg->x, widget_cfg->y);
            lv_obj_set_size(widget_instance, widget_cfg->width, widget_cfg->height);

            // 根据配置设置可见性
            if (!widget_cfg->is_visible) {
                lv_obj_add_flag(widget_instance, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            printf("Screen Manager: Failed to create widget of type %d\n", widget_cfg->type);
        }
    }
}


// --- 2. 【新增】初始化、加载和保存函数 ---


/**
 * @brief 从 JSON 文件加载布局配置
 * @param filepath JSON文件的路径
 * @return 0 on success, -1 on failure
 */
int ui_screens_load_from_json(const char* filepath) {
    #if 0
    // 这是一个伪代码实现
    // 1. 读取文件内容到内存缓冲区
    char* json_buffer = read_file_to_string(filepath);
    if (!json_buffer) return -1;

    // 2. 使用 JSON 解析库解析缓冲区
    json_object* root = json_parse(json_buffer);
    if (!root) { free(json_buffer); return -1; }
    
    // 3. 遍历 JSON 数组，填充 g_runtime_layout_profiles
    //    清空旧的运行时配置
    memset(g_runtime_layout_profiles, 0, sizeof(g_runtime_layout_profiles));
    g_num_runtime_profiles = 0;
    
    json_array* profiles_array = json_get_array(root, "profiles");
    for (int i = 0; i < json_array_get_count(profiles_array); i++) {
        json_object* profile_obj = json_array_get_object(profiles_array, i);
        // ...
        // 从 profile_obj 中解析出 id, name, priority, widgets 等信息
        // 填充到 g_runtime_layout_profiles[i] 中
        // ...
        g_num_runtime_profiles++;
    }

    // 4. 清理资源
    json_free(root);
    free(json_buffer);
    return 0;
    #else
    return -1;
    #endif
}

/**
 * @brief 将当前的运行时布局配置保存为 JSON 文件
 * @param filepath 要保存到的文件路径
 * @return 0 on success, -1 on failure
 */
int ui_screens_save_to_json(const char* filepath) {
    #if 0
    // 伪代码实现
    // 1. 创建一个顶层的 JSON 对象
    json_object* root = json_create_object();
    json_array* profiles_array = json_create_array();

    // 2. 遍历 g_runtime_layout_profiles, 将其转换为 JSON 格式
    for (int i = 0; i < g_num_runtime_profiles; i++) {
        json_object* profile_obj = json_create_object();
        json_add_number(profile_obj, "profile_id", g_runtime_layout_profiles[i].profile_id);
        // ... 添加 name, priority 等 ...
        
        json_array* widgets_array = json_create_array();
        for (int j = 0; j < g_runtime_layout_profiles[i].widget_count; j++) {
            json_object* widget_obj = json_create_object();
            // ... 添加 type, x, y, width, height ...
            json_array_add_object(widgets_array, widget_obj);
        }
        json_add_array(profile_obj, "widgets", widgets_array);
        
        json_array_add_object(profiles_array, profile_obj);
    }
    json_add_array(root, "profiles", profiles_array);

    // 3. 将 JSON 对象序列化为字符串
    char* json_string = json_serialize(root);

    // 4. 将字符串写入文件
    write_string_to_file(filepath, json_string);

    // 5. 清理资源
    free(json_string);
    json_free(root);
    return 0;
    #else
    return 0;
    #endif
}


/**
 * @brief 初始化屏幕配置管理器
 * 
 * - 将默认配置拷贝到运行时配置中。
 * - 尝试从外部文件加载配置来覆盖默认值。
 */
void ui_screens_init(void) {
    // a. 首先，用默认配置填充运行时配置
    memset(g_runtime_layout_profiles, 0, sizeof(g_runtime_layout_profiles));
    int default_count = sizeof(g_default_layout_profiles) / sizeof(g_default_layout_profiles[0]);
    memcpy(g_runtime_layout_profiles, g_default_layout_profiles, sizeof(g_default_layout_profiles));
    g_num_runtime_profiles = default_count;
    
    // b. 尝试从 JSON 文件加载
    if (ui_screens_load_from_json("/etc/layouts.json") == 0) {
        printf("[UI_Screens] Successfully loaded layouts from JSON file.\n");
    } else {
        printf("[UI_Screens] Failed to load layouts from JSON, using factory defaults.\n");
    }
}


