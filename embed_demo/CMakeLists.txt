# Demo: external code already has its own loop; embed ImGuiDx11App with
# minimal changes (a few lines) to visualize shared state in a separate
# UI thread / window.

add_executable(embed_demo
    main.cpp
    ${CMAKE_SOURCE_DIR}/Win32Window.cpp
    ${CMAKE_SOURCE_DIR}/Win32Window.h
    ${CMAKE_SOURCE_DIR}/ImGuiDx11App.cpp
    ${CMAKE_SOURCE_DIR}/ImGuiDx11App.h
    ${CMAKE_SOURCE_DIR}/UiWindow.h
)

target_link_libraries(embed_demo PRIVATE
    imgui
    d3d11
    dxgi
)