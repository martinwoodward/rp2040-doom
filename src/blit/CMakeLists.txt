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

option(DOOM_HANG_DEBUG "Catch hard faults/hangs and report them on the next boot" OFF)
if(DOOM_HANG_DEBUG)
    target_sources(common_32blit INTERFACE ${CMAKE_CURRENT_LIST_DIR}/hang_debug.cpp)
    target_compile_definitions(common_32blit INTERFACE DOOM_HANG_DEBUG)
endif()

target_include_directories(common_32blit INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${32BLIT_DIR}/32blit # hack
)

target_compile_definitions(common_32blit INTERFACE BUILD_32BLIT)

target_link_libraries(common_32blit INTERFACE BlitEngine)

if(32BLIT_HW OR 32BLIT_PICO)
    # wrap file IO
    set(BLIT_LINKER_OPTS
        "LINKER:--wrap=fopen"
        "LINKER:--wrap=fclose"
        "LINKER:--wrap=fread"
        "LINKER:--wrap=fwrite"
        "LINKER:--wrap=fseek"
        "LINKER:--wrap=ftell"
        "LINKER:--wrap=remove"
        "LINKER:--wrap=rename"
        PARENT_SCOPE
    )

    target_sources(common_32blit INTERFACE file.cpp)
endif()