add_library(common_32blit INTERFACE)
target_sources(common_32blit INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/i_blitsound.cpp
    ${CMAKE_CURRENT_LIST_DIR}/i_endoom.c
    ${CMAKE_CURRENT_LIST_DIR}/i_input.c
    ${CMAKE_CURRENT_LIST_DIR}/i_system.c
    ${CMAKE_CURRENT_LIST_DIR}/i_timer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/i_video.c

    ${CMAKE_CURRENT_LIST_DIR}/../pico/stubs.c # stricmp

    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)

target_include_directories(common_32blit INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${32BLIT_DIR}/32blit # hack
)

target_compile_definitions(common_32blit INTERFACE BUILD_32BLIT)