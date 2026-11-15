cmake_minimum_required(VERSION 3.20)
project(reel_display_preview C CXX)

if(NOT LVGL_DIR)
  message(FATAL_ERROR "Set LVGL_DIR to the LVGL revision pinned in ZMK's app/west.yml")
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
  ../../display/reel_canvas.c
  ../../display/reel_assets.c)
target_link_libraries(reel_display_preview PRIVATE lvgl)
target_include_directories(reel_display_preview PRIVATE ../../display)
if(NOT MSVC)
  target_compile_options(reel_display_preview PRIVATE -Wall -Wextra -Werror)
endif()
