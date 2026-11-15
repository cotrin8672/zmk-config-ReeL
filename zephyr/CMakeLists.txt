if(CONFIG_SHIELD_REEL_DONGLE)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src/aml.c)
endif()
