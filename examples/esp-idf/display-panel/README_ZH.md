# 显示与触摸面板

[English](README.md)

使用 LVGL 9.3.0 和 `espressif/esp_lvgl_adapter` 0.6.3 启动 Waveshare BSP 3.0.0 显示栈，依次显示红色/绿色/蓝色/白色屏幕，然后创建一个全屏 LVGL 触摸区域，在 GT911 触摸位置绘制黑色方块。

该项目固定为 4 英寸 720 x 720 面板选择。所有 LVGL 对象更改均受 BSP 显示锁保护；LVGL 定时器任务由 BSP 管理。

```sh
idf.py set-target esp32p4
idf.py flash monitor
```

将此示例用作 LCD/触摸组合冒烟测试。面板输出、坐标方向、多点触摸行为和绘制延迟需要在开发板上测试，不能由 CI 编译确认。
