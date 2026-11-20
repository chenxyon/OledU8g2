/**
 * @file     OledU8g2.hpp
 * @brief    多面板通用 OLED/TFT 显示类 —— 基于 U8g2
 *
 * 统一接口适配 SSD1306/SH1106 (I²C) 和 ST7789/ILI9341 (SPI)。
 * init(panel, pins) 时传入面板类型 + 对应引脚结构体指针 (void*),
 * 组件内部自动 new 对应 U8g2 子类。
 *
 * UI 导航/按键交互接口已预留 (register_page / navigate_to / send_key ...),
 * 实现见 cpp，当前 U8g2 单色屏只实现网格导航核心, 触摸回调为空壳。
 *
 * 图标统一用 oled_icon_t 结构体, 由 png_to_bitmap.py 生成。
 */
#ifndef OLED_U8G2_HPP
#define OLED_U8G2_HPP

#include <Arduino.h>       /* U8g2 依赖 Arduino core */
#include <Wire.h>

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "U8g2lib.h"

#include "oled_types.h"
#include "display_iface.h"   /* 统一接口 IDisplay, OledU8g2 继承它 */

/* =========================================================
 *  C++ 类 —— 继承统一接口 IDisplay
 *
 *  项目代码可以:
 *    IDisplay *disp = new OledU8g2();
 *    disp->init(...); disp->show(...);  // 统一接口
 *
 *  或直接:
 *    OledU8g2 oled;
 *    oled.init(...); oled.show(...);   // 具体类特有方法也能用
 * ========================================================= */

class OledU8g2 : public IDisplay {
public:
    OledU8g2() = default;
    ~OledU8g2() = default;

    /* ---------- 初始化 ---------- */

    /**
     * @brief  初始化屏幕
     * @param panel  面板类型枚举
     * @param pins   引脚结构体指针 (根据 panel 类型选 oled_i2c_pins_t 或 oled_spi_pins_t)
     *               void* 设计让单一签名兼容所有屏, 内部按 panel 校验字段
     */
    esp_err_t init(oled_panel_t panel, const void *pins);
    esp_err_t deinit();

    /* =========================================================
     *  基础绘制 API (像素坐标, x/y 自动边界检查)
     * ========================================================= */

    /**
     * @brief  显示一行文字 (UTF-8, 支持 GB2312 中文 + ASCII)
     * @param text  字符串
     * @param x     左上角像素 x
     * @param y     顶部像素 y (字体位置 TOP)
     */
    esp_err_t show(const char *text, uint16_t x, uint16_t y);

    /**
     * @brief  显示文字 + 自动对齐 (OLED_ALIGN_*)
     */
    esp_err_t show_aligned(const char *text, oled_align_t align, uint16_t margin = 0);

    /**
     * @brief  绘制一个 oled_icon_t 图标 (png_to_bitmap.py 输出格式)
     * @param icon  图标指针 (内部读 icon->w, icon->h, min_screen 自动过滤)
     */
    esp_err_t draw_icon(const oled_icon_t *icon, uint16_t x, uint16_t y);

    /**
     * @brief  绘制图标 + 自动对齐
     */
    esp_err_t draw_icon_aligned(const oled_icon_t *icon, oled_align_t align, uint16_t margin = 0);

    /* ---- 图形基础 ---- */
    esp_err_t draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fill = false);
    esp_err_t draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    esp_err_t clear();

    /* ---- 手动刷新 (U8g2 内部) ---- */
    esp_err_t refresh();

    /* =========================================================
     *  时间显示 (内部 esp_timer 1Hz 刷新)
     * ========================================================= */

    /**
     * @brief  启用独立时间显示 (内部定时器自动更新)
     * @param on    true=启用, false=禁用
     * @param x, y  左上角像素坐标
     * @param fmt   strftime 格式串, 默认 "%H:%M:%S" (时:分:秒)
     */
    esp_err_t enable_clock(bool on, uint16_t x, uint16_t y,
                           const char *fmt = "%H:%M:%S");
    esp_err_t disable_clock();

    /* =========================================================
     *  状态栏 (预留: 组件只提供画的能力, 不管理业务状态)
     *  调用方自己决定: draw_icon(&wifi_icon, x, y) + draw_icon(&battery_icon, x, y)
     * ========================================================= */

    esp_err_t set_status_bar(const oled_status_bar_cfg_t &cfg);

    /* =========================================================
     *  UI 导航 (预留接口, 核心网格导航已实现)
     * ========================================================= */

    /**
     * @brief  注册一个页面 (最多 16 个)
     */
    esp_err_t register_page(const oled_page_t &page);

    /**
     * @brief  跳转到指定 URI 的页面 (压入页面栈, 最多 8 层)
     */
    esp_err_t navigate_to(const char *uri);

    /**
     * @brief  返回上一页 (弹出页面栈)
     */
    esp_err_t navigate_back();

    /**
     * @brief  发送一个按键事件 (外部按键中断/轮询后调用)
     */
    esp_err_t send_key(oled_key_t key);

    /**
     * @brief  手动设置焦点索引
     */
    esp_err_t set_focus(uint8_t index);

    /* ---- 图标网格绘制 (预留, 当前实现基础版) ---- */
    esp_err_t draw_grid(const oled_grid_item_t *items, uint8_t count,
                        const oled_grid_cfg_t &cfg,
                        uint16_t start_x = 0, uint16_t start_y = 0);

    /* ---- 页面指示器 (底部小圆点, 预留) ---- */
    esp_err_t draw_page_indicator(const oled_page_ctrl_t &ctrl,
                                   uint16_t x, uint16_t y);

    /* =========================================================
     *  触摸 (预留, U8g2 暂不实现)
     * ========================================================= */

    esp_err_t register_touch_callback(oled_touch_cb_t cb, void *user_ctx = nullptr);

    /* =========================================================
     *  查询 API
     * ========================================================= */

    bool                        is_ready()      const { return m_ready; }
    const oled_screen_info_t&   screen()        const { return m_screen; }
    oled_panel_t                panel()         const { return m_panel; }
    const oled_page_t*          current_page()  const;
    uint8_t                     focus_index()    const { return m_focus_index; }

private:
    /* ---- U8g2 基类指针, init 时指向具体子类 ---- */
    U8G2 *m_u8g2 = nullptr;

    /* ---- 当前面板信息 ---- */
    oled_panel_t          m_panel   = OLED_PANEL_MAX;
    oled_screen_info_t    m_screen  = {};
    bool                  m_ready   = false;

    /* ---- 时钟 ---- */
    esp_timer_handle_t    m_clock_timer = nullptr;
    uint16_t              m_clock_x     = 0;
    uint16_t              m_clock_y     = 0;
    char                  m_clock_fmt[32] = "%H:%M:%S";

    /* ---- UI 导航内部状态 ---- */
    oled_page_t           m_pages[16];
    uint8_t               m_page_count  = 0;
    const oled_page_t*    m_page_stack[8];
    uint8_t               m_page_depth  = 0;
    uint8_t               m_focus_index = 0;

    /* ---- 触摸 (预留) ---- */
    oled_touch_cb_t       m_touch_cb    = nullptr;
    void*                 m_touch_ctx    = nullptr;

    /* ---- 内部辅助 ---- */

    // init 时根据 panel 创建 U8g2 子类
    esp_err_t init_i2c(oled_panel_t panel, const oled_i2c_pins_t &pins);
    esp_err_t init_spi(oled_panel_t panel, const oled_spi_pins_t &pins);

    // esp_timer 静态回调 -> 成员函数
    static void IRAM_ATTR clock_timer_cb(void *arg);
    void        refresh_clock();

    // 默认网格按键处理 (方向键移动焦点, OK 跳转, CANCEL 返回)
    bool        default_key_handler(const oled_page_t *page, oled_key_t key,
                                    uint8_t *focus_index);

    // 默认网格渲染 (遍历 items 画图标+文字+焦点框)
    void        default_grid_render(const oled_page_t *page, uint8_t focus_index);

    // 根据 items 数量 + 屏幕宽算网格列数
    uint8_t     calc_cols_for_grid(const oled_page_t *page) const;

    // 对齐坐标计算
    uint16_t    align_x(oled_align_t align, uint16_t obj_w, uint16_t margin) const;
    uint16_t    align_y(oled_align_t align, uint16_t obj_h, uint16_t margin) const;
};

#endif /* OLED_U8G2_HPP */
