cmake_minimum_required(VERSION 3.20)
project(reel_display_preview C CXX)

if(NOT LVGL_DIR)
  message(FATAL_ERROR "Set LVGL_DIR to the LVGL revision pinned in ZMK's app/west.yml")
endif()
if(NOT ZEPHYR_DIR)
  message(FATAL_ERROR "Set ZEPHYR_DIR to the pinned Zephyr checkout (mono display adapter)")
endif()
set(LV_CONF_PATH "" CACHE STRING "" FORCE)
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
add_subdirectory(${LVGL_DIR} lvgl)
target_include_directories(lvgl SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(reel_display_preview
  render.c
  ${ZEPHYR_DIR}/modules/lvgl/lvgl_display_mono.c
  ../../display/reel_canvas.c
  ../../display/reel_assets.c)
target_link_libraries(reel_display_preview PRIVATE lvgl)
target_include_directories(reel_display_preview PRIVATE
  ../../display zephyr-shim ${ZEPHYR_DIR}/modules/lvgl/include)
option(REEL_TEST_INVERTED "Use the nice_view inversion required by the pinned adapter" ON)
target_compile_definitions(reel_display_preview PRIVATE
  "LV_CONF_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h\""
  CONFIG_LV_DRAW_BUF_STRIDE_ALIGN=1
  CONFIG_NICE_VIEW_WIDGET_INVERTED=$<BOOL:${REEL_TEST_INVERTED}>)
if(NOT MSVC)
  target_compile_options(reel_display_preview PRIVATE -Wall -Wextra -Werror)
endif()
