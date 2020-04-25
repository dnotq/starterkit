#
# Starter Kit executable target.
#

add_executable(starterkit
    starterkit.c
    sk_gui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp # remove if not needed.
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/examples/imgui_impl_sdl.cpp     # change to implementation.
    ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp # change to implementation.
    ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.c
)

target_compile_definitions(starterkit PRIVATE
    IMGUI_USE_STB_SPRINTF
)

target_include_directories(starterkit PRIVATE
    ${SDL2_INCLUDE_DIR}
    ${STB_DIR}
    ${LIBUV_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/examples/libs/gl3w
)

target_link_libraries(starterkit
    uv
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
)


# Windows needs the DLL with the EXE, and VS always adds the build type to
# whatever is in CMAKE_RUNTIME_OUTPUT_DIRECTORY.  There does not seem to
# any other way to figure out the build type when it is not specified.
if ( WIN32 )
    if ( NOT CMAKE_BUILD_TYPE )
        set(WINDIR "Debug")
    else ()
        set(WINDIR "Release")
    endif ()

    add_custom_command(TARGET starterkit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${SDL2_DIR}/lib/${ARCHTYPE}/SDL2.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${WINDIR}/SDL2.dll)
endif ()
