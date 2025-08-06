#include "ui_manger.h"
#include "ui_screens.h"
#include "widgets/ui_source.h"
#include "widgets/ui_status_bar.h"
#include "widgets/ui_quick_access.h"
#include "widgets/ui_clock.h"
#include "widgets/ui_sys_control.h"

#if 1//def SKILL_FOR_SHUN_LISTCHANNEL_REPEATED //mllee 110824 
	#define __msg(...)    		(eLIBs_printf("L%d(%s):", __LINE__, __FILE__),                 \
							     eLIBs_printf(__VA_ARGS__))	
#endif   


// 存储所有已注册控件信息的静态数组
static ui_manger_t g_manger_registry[MANGER_ID_MAX];

ui_screen_id_t g_manger_screen = UI_SCREEN_HOME; // 当前屏幕的ID

void ui_manager_init(void) {
    memset(g_manger_registry, 0, sizeof(g_manger_registry));
	ui_status_bar_register_init();
	ui_quick_access_register_init();
	ui_source_widget_init();
	ui_clock_register_init();
	ui_sys_control_register_init();
}

// 在应用启动时，由各个控件模块调用此函数进行注册
int ui_manager_register(ui_manger_id_t id, const ui_manger_ops_t* ops) {
    // 检查ID是否在有效范围内
    // 并且操作函数集不为空
    if (!ops || id <= MANGER_ID_NONE || id >= MANGER_ID_MAX) {
        return -1; // 无效的ID或操作函数集
    }
    
    g_manger_registry[id].id = id;
    g_manger_registry[id].ops = ops;
    g_manger_registry[id].instance = NULL;
    g_manger_registry[id].is_created = false;

    return 0; // 成功注册
}

// UI线程在需要时调用，用于真正创建控件
lv_obj_t* ui_manager_create(ui_manger_id_t id, lv_obj_t* parent, void *arg) {
    if (id <= MANGER_ID_NONE || id >= MANGER_ID_MAX || !g_manger_registry[id].ops->create) {
        return NULL;
    }

    if (g_manger_registry[id].is_created) {
        // 如果已经创建，直接返回实例
        return g_manger_registry[id].instance;
    }

    lv_obj_t* obj = g_manger_registry[id].ops->create(parent,arg);
    if (obj) {
        g_manger_registry[id].instance = obj;
        g_manger_registry[id].is_created = true;
        // 将控件上下文关联到lv_obj_t，方便事件处理
        lv_obj_set_user_data(obj, &g_manger_registry[id]);
    }
    return obj;
}

// **核心：消息分发函数**
void ui_manager_dispatch_message(const app_message_t* msg) {

    ui_manger_id_t id = MANGER_ID_NONE;
    if (!msg) return;

     ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;


    // 【核心修改】优先处理全局系统消息
    if (msg->source == SRC_SYSTEM_SERVICE) {
        switch (msg->command) {
            case SYSTEM_EVT_SYSTEM_OPEN_UI: {
                // 系统允许UI开机的第一个界面
                system_state_payload_t* payload = (system_state_payload_t*)wrapper->ptr;
                g_manger_screen = payload->screen_id; // 更新当前屏幕状态
                eLIBs_printf("UI Manager: Opening initial screen %d\n", g_manger_screen);
                // 调用屏幕创建函数
                ui_screen_create_initial(g_manger_screen); 
                return; // 系统消息处理完毕，直接返回
            }

            case SYSTEM_EVT_SYSTEM_SWITCH_TO_SCREEN: {
                system_state_payload_t* payload = (system_state_payload_t*)wrapper->ptr;
                if (payload) {
                    printf("UI Manager: Received request to switch to screen %d\n", payload->screen_id);
                    // 调用屏幕切换函数
                    ui_screen_switch_to(payload->screen_id, NULL, NULL); 
                }
                return; // 系统消息处理完毕，直接返回
            }
        }
    }




    if(msg->reserved)
        id = (ui_manger_id_t)(*((int *)msg->reserved));
//__msg("id=%d\n",id);
    if (id > MANGER_ID_NONE && id < MANGER_ID_MAX) {//指定控件
        if (g_manger_registry[id].ops && g_manger_registry[id].ops->handle_event) {
            // 调用对应控件注册的事件处理函数
            g_manger_registry[id].ops->handle_event(&g_manger_registry[id], msg);
        }
    }
    else{//广播类型
        for(int i = MANGER_ID_NONE + 1; i < MANGER_ID_MAX; i++) {
			//__msg("i=%d\n",i);
            if (g_manger_registry[i].ops && g_manger_registry[i].ops->handle_event) {
                // 调用所有已创建控件的事件处理函数
               // __msg("i=%d\n",i);
                g_manger_registry[i].ops->handle_event(&g_manger_registry[i], msg);
            }
        }
        
    }
	//__msg("id=%d\n",id);
}

// 其他辅助函数
void ui_manager_destroy(ui_manger_id_t id) {
    // ... 查找并调用 ops->destroy ...
    if (id <= MANGER_ID_NONE || id >= MANGER_ID_MAX || !g_manger_registry[id].is_created) {
        return; // 无效的ID或未创建
    }
    if (g_manger_registry[id].ops && g_manger_registry[id].ops->destroy) {
        g_manger_registry[id].ops->destroy(&g_manger_registry[id]);
    }
    // 清理注册表信息
    g_manger_registry[id].id = MANGER_ID_NONE;
    g_manger_registry[id].is_created = false;
    g_manger_registry[id].instance = NULL;
    g_manger_registry[id].ops = NULL;

}

lv_obj_t *ui_manger_get_instance(ui_manger_id_t id) {
    if (id <= MANGER_ID_NONE || id >= MANGER_ID_MAX || !g_manger_registry[id].is_created) {
        return NULL; // 无效的ID或未创建
    }
    return g_manger_registry[id].instance;
}