if(CONFIG_SHIELD_REEL_LEFT)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../encoder/reel_encoder.c)
endif()

if(CONFIG_SHIELD_REEL_LEFT OR CONFIG_SHIELD_REEL_RIGHT OR CONFIG_SHIELD_REEL_DONGLE)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../display/reel_display_state.c)
endif()

if(CONFIG_ZMK_DISPLAY AND (CONFIG_SHIELD_REEL_LEFT OR CONFIG_SHIELD_REEL_RIGHT))
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../display/reel_status.c
    ${CMAKE_CURRENT_LIST_DIR}/../display/reel_canvas.c
    ${CMAKE_CURRENT_LIST_DIR}/../display/reel_assets.c)
endif()
