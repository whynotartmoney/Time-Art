idf_component_register(
    SRCS "main.c" "sand_timer.c" "sand_ui.c" "sand_chime.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer freertos log
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sand_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sand_font_32.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sand_font_64.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sand_font_64d.c"
)
