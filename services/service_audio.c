// --- START OF FILE service_audio.c ---

#include "service_audio.h"
#include "bus_router.h"
#include <stdio.h>
#include <string.h>

// --- 调试宏 ---
static int g_audio_service_debug = 1;
static int g_audio_service_info = 1;
#define AUDIO_DEBUG(fmt, ...) do { if (g_audio_service_debug) { printf("[AudioService-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); } } while(0)
#define AUDIO_INFO(fmt, ...)  do { if (g_audio_service_info) { printf("[AudioService-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); } } while(0)
#define AUDIO_ERROR(fmt, ...) do { printf("[AudioService-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); } while(0)

// --- 内部定义 ---
#define AUDIO_QUEUE_SIZE 64

typedef struct {
    audio_state_t               current_state;
    msg_payload_service_audio_t service_data;
} audio_service_context_t;

// --- 静态变量 ---
static void* g_audio_queue_handle = NULL;
static int   g_audio_thread_handle = 0;
static audio_service_context_t g_audio_ctx;

// --- 静态函数声明 ---
static void audio_thread_entry(void* p_arg);
static void audio_event_handler(app_message_t* msg);
static void audio_notify_focus_changed(void);
static void audio_notify_dsp_updated(void);

// --- 调试接口 ---
void service_audio_set_debug(int enable) { g_audio_service_debug = enable; }
void service_audio_set_info(int enable)  { g_audio_service_info = enable; }

// --- Payload 管理 ---
msg_payload_service_audio_t* service_audio_payload_alloc(void) {
    return (msg_payload_service_audio_t*)infotainment_malloc(sizeof(msg_payload_service_audio_t));
}

void service_audio_payload_free(void* payload) {
    if (payload) {
        infotainment_free(payload);
    }
}

msg_payload_service_audio_t* service_audio_payload_copy(const void* src) {
    if (!src) return NULL;
    msg_payload_service_audio_t* dst = service_audio_payload_alloc();
    if (dst) {
        memcpy(dst, src, sizeof(msg_payload_service_audio_t));
    }
    return dst;
}

// --- 服务初始化/退出 ---
void service_audio_init(void) {
    AUDIO_INFO("Initializing...");

    memset(&g_audio_ctx, 0, sizeof(g_audio_ctx));
    g_audio_ctx.current_state = AUDIO_STATE_UNINITIALIZED;
    g_audio_ctx.service_data.focus_owner   = SRC_UNDEFINED;
    g_audio_ctx.service_data.mix_level     = 0;
    memset(g_audio_ctx.service_data.eq, 0, sizeof(g_audio_ctx.service_data.eq));
    g_audio_ctx.service_data.xbass_enabled = false;
    g_audio_ctx.service_data.delay_ms      = 0;

    g_audio_thread_handle = infotainment_thread_create(
        audio_thread_entry, NULL, TASK_PROI_LEVEL3, 1024 * 4, "ServiceAudioThread");
    if (g_audio_thread_handle <= 0) {
        AUDIO_ERROR("Failed to create thread");
    }
}

void service_audio_exit(void) {
    AUDIO_INFO("Exiting...");
    if (g_audio_thread_handle > 0) {
        // TODO: implement graceful thread exit if needed
    }
}

// --- 服务主线程 ---
static void audio_thread_entry(void* p_arg) {
    unsigned char err;
    AUDIO_INFO("Thread started.");

    g_audio_queue_handle = infotainment_queue_create(AUDIO_QUEUE_SIZE);
    if (!g_audio_queue_handle) { return; }

    if (bus_router_register_service(SRC_AUDIO_SERVICE, ADDRESS_AUDIO_SERVICE, g_audio_queue_handle,
                                    "AudioService", service_audio_payload_free, service_audio_payload_copy) < 0) {
        return;
    }

    g_audio_ctx.current_state = AUDIO_STATE_READY;
    msg_payload_service_audio_t* ready_payload = service_audio_payload_copy(&g_audio_ctx.service_data);
    if (ready_payload) {
        ready_payload->current_state = g_audio_ctx.current_state;
        bus_post_message(SRC_AUDIO_SERVICE, ADDRESS_UI | ADDRESS_SYSTEM_SERVICE, MSG_PRIO_NORMAL,
                         AUDIO_EVT_READY, ready_payload, SRC_AUDIO_SERVICE, NULL);
    }

    while (1) {
        if (infotainment_thread_TDelReq(EXEC_prioself)) {
            goto exit;
        }

        app_message_t* msg = (app_message_t*)infotainment_queue_pend(g_audio_queue_handle, 0, &err);
        if (!msg || err != OS_NO_ERR) {
            continue;
        }

        audio_event_handler(msg);

        if (msg->payload) {
            free_ref_counted_payload((ref_counted_payload_t*)msg->payload);
        }
        infotainment_free(msg);
    }

exit:
    AUDIO_INFO("Thread exiting...");
    bus_router_unregister_service(SRC_AUDIO_SERVICE);
    infotainment_thread_delete(EXEC_prioself);
}

// --- 核心事件处理函数 ---
static void audio_event_handler(app_message_t* msg) {
    ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
    msg_payload_service_audio_t* payload = wrapper ? wrapper->ptr : NULL;

    AUDIO_DEBUG("Cmd:0x%X from:0x%X", msg->command, msg->source);

    switch (msg->command) {
    case AUDIO_CMD_FOCUS_REQUEST:
        g_audio_ctx.service_data.focus_owner = payload ? payload->focus_owner : msg->source;
        AUDIO_INFO("Focus request: 0x%X", g_audio_ctx.service_data.focus_owner);
        audio_notify_focus_changed();
        break;
    case AUDIO_CMD_FOCUS_RELEASE:
        if (g_audio_ctx.service_data.focus_owner ==
            (payload ? payload->focus_owner : msg->source)) {
            g_audio_ctx.service_data.focus_owner = SRC_UNDEFINED;
            AUDIO_INFO("Focus released");
            audio_notify_focus_changed();
        }
        break;
    case AUDIO_CMD_SET_MIX:
        if (payload) {
            g_audio_ctx.service_data.mix_level = payload->mix_level;
            AUDIO_INFO("Mix level set: %d", payload->mix_level);
            audio_notify_dsp_updated();
        }
        break;
    case AUDIO_CMD_SET_EQ:
        if (payload) {
            memcpy(g_audio_ctx.service_data.eq, payload->eq,
                   sizeof(g_audio_ctx.service_data.eq));
            AUDIO_INFO("EQ updated");
            audio_notify_dsp_updated();
        }
        break;
    case AUDIO_CMD_SET_XBASS:
        if (payload) {
            g_audio_ctx.service_data.xbass_enabled = payload->xbass_enabled;
            AUDIO_INFO("XBass %s", payload->xbass_enabled ? "enabled" : "disabled");
            audio_notify_dsp_updated();
        }
        break;
    case AUDIO_CMD_SET_DELAY:
        if (payload) {
            g_audio_ctx.service_data.delay_ms = payload->delay_ms;
            AUDIO_INFO("Delay set: %u ms", payload->delay_ms);
            audio_notify_dsp_updated();
        }
        break;
    default:
        break;
    }
}

// --- 通知辅助函数 ---
static void audio_notify_focus_changed(void) {
    msg_payload_service_audio_t* payload = service_audio_payload_copy(&g_audio_ctx.service_data);
    if (payload) {
        bus_post_message(SRC_AUDIO_SERVICE, ADDRESS_UI | ADDRESS_SYSTEM_SERVICE, MSG_PRIO_NORMAL,
                         AUDIO_EVT_FOCUS_CHANGED, payload, SRC_AUDIO_SERVICE, NULL);
    }
}

static void audio_notify_dsp_updated(void) {
    msg_payload_service_audio_t* payload = service_audio_payload_copy(&g_audio_ctx.service_data);
    if (payload) {
        bus_post_message(SRC_AUDIO_SERVICE, ADDRESS_UI | ADDRESS_SYSTEM_SERVICE, MSG_PRIO_NORMAL,
                         AUDIO_EVT_DSP_UPDATED, payload, SRC_AUDIO_SERVICE, NULL);
    }
}

// --- END OF FILE service_audio.c ---

