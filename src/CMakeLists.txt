##
# Starter Kit Executable.
#

set(EXEC_NAME "starterkit")

# Files to build the executable.
add_executable(${EXEC_NAME}
    disco.cpp
    program.c
    draw_ui.cpp
    draw_3d.c
    xyz/xyz.c
    log/log.c
    ${GL3W_DIR}/src/gl3w.c
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp    # change to implementation.
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp # change to implementation.
    ${IMGUI_DIR}/imgui_demo.cpp # remove if not needed.
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

# This will set imgui to use STB printf replacement, and prevent the
# logging system from also including the implementation (it can only
# be included once for a binary).
target_compile_definitions(${EXEC_NAME} PRIVATE
    IMGUI_USE_STB_SPRINTF
)

target_include_directories(${EXEC_NAME} PRIVATE
    .
    cbuf
    cidx
    log
    xyz
    ${SDL2_INC_DIR}
    ${GL3W_DIR}/include
    ${CGLM_DIR}/include
    ${STB_DIR}
    ${IMGUI_DIR}
    ${EXTRA_INCLUDES}
)

#target_link_directories(${EXEC_NAME} PRIVATE
#    ${SDL2_LIB_DIR}
#)

target_link_libraries(${EXEC_NAME} PRIVATE
    ${SDL2_LINK_LIBS}
    ${OPENGL_LIBRARIES}
    ${IMGUI_LINK_LIBS}
)

# Windows needs the SDL2 DLL in the same directory with the EXE.
# MSVS also adds a build-type, Debug or Release, to the output
# directory path, i.e. build/bin/Debug/program.exe
if ( SDL2_RUNTIME )
    get_filename_component(SDL2_RUNTIME_NAME "${SDL2_RUNTIME}" NAME)
    add_custom_command(TARGET ${EXEC_NAME} POST_BUILD
        MAIN_DEPENDENCY "${SDL2_LIBRARY}"
        BYPRODUCTS "${SDL2_RUNTIME_NAME}"
        COMMENT "Copying SDL2 DLL"
        COMMAND "${CMAKE_COMMAND}" -E copy "${SDL2_RUNTIME}" "$<TARGET_FILE_DIR:${EXEC_NAME}>/${SDL2_RUNTIME_NAME}"
    )
endif ()
