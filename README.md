
/infotainment/
│
├── ... (Makefile, Kconfig, main.c)
│
├── include/
│   └── message_defs.h
│
├── bus/
│   ├── bus_router.c
│   └── bus_router.h
│
├── services/
│   ├── services_manager.c
│   ├── service_system.c
│   ├── service_input.c      # 【新增】输入服务
│   └── ...
│
├── ui/
│   ├── ui_main.c
│   ├── ui_manager.c
│   ├── screens/
│   └── widgets/
│
└── drivers/
    ├── drv_display.c
    └── drv_input.c          # 【重要】LVGL输入驱动