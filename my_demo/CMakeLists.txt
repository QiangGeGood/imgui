# my_demo executable: links the imgui static library defined at top-level.
# Framework sources (Win32Window / ImGuiDx11App / UiWindow) live in the
# project root so other modules can include and reuse them.

add_executable(my_demo WIN32
    main.cpp
    HelloWindow.cpp
    HelloWindow.h
    StatsWindow.cpp
    StatsWindow.h
    ProgressWindow.cpp
    ProgressWindow.h
    ${CMAKE_SOURCE_DIR}/Win32Window.cpp
    ${CMAKE_SOURCE_DIR}/Win32Window.h
    ${CMAKE_SOURCE_DIR}/ImGuiDx11App.cpp
    ${CMAKE_SOURCE_DIR}/ImGuiDx11App.h
    ${CMAKE_SOURCE_DIR}/UiWindow.h
)

target_link_libraries(my_demo PRIVATE
    imgui
    d3d11
    dxgi
)

# Use main() entry point even with WIN32 subsystem (MSVC).
if (MSVC)
    set_target_properties(my_demo PROPERTIES
        LINK_FLAGS "/ENTRY:mainCRTStartup"
    )
endif()