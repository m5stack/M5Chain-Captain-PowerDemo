#include "m5cc_display.hpp"

#include <cstdio>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m5cc_board.hpp"
#include "m5stack_chain_captain.h"

namespace m5cc::display {
namespace {

constexpr char kTag[] = "m5cc_display";

bool g_started                 = false;
lv_obj_t* g_title              = nullptr;
lv_obj_t* g_line1              = nullptr;
lv_obj_t* g_line2              = nullptr;
lv_obj_t* g_counter            = nullptr;
TaskHandle_t g_draw_task       = nullptr;
std::uint32_t g_draw_period_ms = 33;

void style_label(lv_obj_t* label)
{
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
}

void draw_task(void*)
{
    std::uint32_t frame = 0;
    for (;;) {
        if (bsp_display_lock(0)) {
            char text[24];
            std::snprintf(text, sizeof(text), "f=%u", static_cast<unsigned>(frame));
            ++frame;
            if (g_counter != nullptr) {
                lv_label_set_text(g_counter, text);
            }
            lv_obj_invalidate(lv_screen_active());
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(g_draw_period_ms));
    }
}

}  // namespace

Result start()
{
    if (g_started) {
        return {};
    }
    if (bsp_display_start() == nullptr) {
        return {ESP_FAIL, 0};
    }
    g_started = true;
    ESP_LOGD(kTag, "DISPLAY_STARTED panel=jd9853 res=240x240");
    return {};
}

bool started()
{
    return g_started;
}

Result set_brightness(std::uint8_t percent)
{
    if (!g_started) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    return {board::set_backlight(percent), 0};
}

Result show_frame(const char* title, const char* line1, const char* line2)
{
    if (!g_started) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    if (!bsp_display_lock(0)) {
        return {ESP_ERR_TIMEOUT, 0};
    }

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    if (g_title == nullptr) {
        g_title = lv_label_create(screen);
        lv_obj_align(g_title, LV_ALIGN_TOP_MID, 0, 16);
        style_label(g_title);

        g_line1 = lv_label_create(screen);
        lv_obj_align(g_line1, LV_ALIGN_TOP_MID, 0, 56);
        style_label(g_line1);

        g_line2 = lv_label_create(screen);
        lv_obj_align(g_line2, LV_ALIGN_TOP_MID, 0, 84);
        style_label(g_line2);

        g_counter = lv_label_create(screen);
        lv_obj_align(g_counter, LV_ALIGN_BOTTOM_MID, 0, -16);
        style_label(g_counter);
    }

    lv_label_set_text(g_title, title != nullptr ? title : "");
    lv_label_set_text(g_line1, line1 != nullptr ? line1 : "");
    lv_label_set_text(g_line2, line2 != nullptr ? line2 : "");
    lv_label_set_text(g_counter, "t=0s");

    bsp_display_unlock();
    return {};
}

Result show_solid_white()
{
    if (!g_started) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    if (!bsp_display_lock(0)) {
        return {ESP_ERR_TIMEOUT, 0};
    }

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    // A full-load frame must be a single flat colour: hide any labels left over
    // from an earlier information frame.
    lv_obj_t* const labels[] = {g_title, g_line1, g_line2, g_counter};
    for (lv_obj_t* label : labels) {
        if (label != nullptr) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_invalidate(screen);

    bsp_display_unlock();
    return {};
}

Result tick_1hz(std::uint32_t seconds)
{
    if (!g_started || g_counter == nullptr) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    if (!bsp_display_lock(0)) {
        return {ESP_ERR_TIMEOUT, 0};
    }
    char text[16];
    std::snprintf(text, sizeof(text), "t=%us", static_cast<unsigned>(seconds));
    lv_label_set_text(g_counter, text);
    // Full-screen invalidation: the visible change is one flushed frame per call.
    lv_obj_invalidate(lv_screen_active());
    bsp_display_unlock();
    return {};
}

Result start_continuous_draw(std::uint32_t hz)
{
    // A solid-colour frame has no counter label, so the refresh must not depend
    // on g_counter: the workload is the full-screen invalidation itself.
    if (!g_started) {
        return {ESP_ERR_INVALID_STATE, 0};
    }
    if (hz == 0) {
        return {ESP_ERR_INVALID_ARG, 0};
    }
    if (g_draw_task != nullptr) {
        return {};
    }
    g_draw_period_ms = 1000U / hz;
    if (g_draw_period_ms == 0U) {
        g_draw_period_ms = 1U;
    }
    if (xTaskCreate(draw_task, "m5cc_draw", 4096, nullptr, 3, &g_draw_task) != pdPASS) {
        g_draw_task = nullptr;
        return {ESP_ERR_NO_MEM, 0};
    }
    return {};
}

bool continuous_draw_running()
{
    return g_draw_task != nullptr;
}

}  // namespace m5cc::display
