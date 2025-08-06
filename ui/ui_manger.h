#ifndef UI_MANAGER_H
#define UI_MANAGER_H
#include "lvgl.h"
#include <bus_router.h>
#include "message_defs.h" // 引入我们之前定义的消息结构
#include "res/lang.h"
#include "res/theme.h"
#include "image/lzma_Parse_Picture.h"
#include "fb_lib/GaussBlur.h"
#include "fb_lib/fb_file.h"
#include "listbar/ui_menu.h"

#include <math.h>


extern lv_ft_info_t lv_font_ssmall;//16	//16
extern lv_ft_info_t lv_font_small;//20		//21
extern lv_ft_info_t lv_font_medium;//24	//29
extern lv_ft_info_t lv_font_medium_ex;//24	//29
extern lv_ft_info_t lv_font_large;//24	//29
extern lv_ft_info_t lv_font_xlarge;




// 前向声明，避免循环引用
struct ui_manger_ops;

// 每个控件实例的上下文
typedef struct {
    ui_manger_id_t id;
    lv_obj_t*      instance; // 指向该控件的LVGL根对象
    const struct ui_manger_ops* ops; // 指向操作函数集
    bool           is_created;
} ui_manger_t;


// 标准操作函数指针结构体
typedef struct ui_manger_ops {
    /**
     * @brief 创建控件的LVGL对象
     * @param parent 父对象
     * @return 控件的根对象
     */
    lv_obj_t* (*create)(lv_obj_t* parent,void *arg);

    /**
     * @brief 销毁控件，释放所有资源
     * @param manger 指向控件实例的指针 (ui_manger_t*)
     */
    void (*destroy)(ui_manger_t* manger);

    /**
     * @brief 显示控件 (可带动画)
     * @param manger 指向控件实例的指针
     */
    void (*show)(ui_manger_t* manger);

    /**
     * @brief 隐藏控件 (可带动画)
     * @param manger 指向控件实例的指针
     */
    void (*hide)(ui_manger_t* manger);
    
    /**
     * @brief 处理来自服务线程的消息
     * @param manger 指向控件实例的指针
     * @param msg 收到的消息
     * @return 0表示成功处理
     */
    int (*handle_event)(ui_manger_t* manger, const app_message_t* msg);

} ui_manger_ops_t;

void ui_manager_init(void);
lv_obj_t* ui_manager_create(ui_manger_id_t id, lv_obj_t* parent, void *arg);
void ui_manager_dispatch_message(const app_message_t* msg);
void ui_manager_destroy(ui_manger_id_t id);
// 注册控件的函数
int ui_manager_register(ui_manger_id_t id, const ui_manger_ops_t* ops);
// 获取控件实例的函数
lv_obj_t* ui_manger_get_instance(ui_manger_id_t id);

void ui_main_init(void);

// UI日志控制函数
void ui_set_debug(int enable);
void ui_set_info(int enable);

#endif
