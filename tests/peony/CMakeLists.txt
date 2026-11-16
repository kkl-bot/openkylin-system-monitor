find_package(Qt5 REQUIRED COMPONENTS Qml Quick Test)
find_package(ukui-quick-framework CONFIG QUIET)
if(TARGET ukui-quick::framework)
    set(UKUI_TEST_LIBRARIES ukui-quick::framework)
else()
    set(UKUI_QUICK_SOURCE_DIR "" CACHE PATH "Optional ukui-quick public headers matching the installed runtime")
    if(NOT EXISTS "${UKUI_QUICK_SOURCE_DIR}/framework/widget/widget-loader.h")
        message(FATAL_ERROR "Install libukui-quick-dev or set UKUI_QUICK_SOURCE_DIR for native integration tests")
    endif()
    find_library(UKUI_FRAMEWORK_LIBRARY NAMES ukui-quick-framework libukui-quick-framework.so.1 REQUIRED)
    find_library(UKUI_CORE_LIBRARY NAMES ukui-quick-core libukui-quick-core.so.1 REQUIRED)
    add_library(ukui_test_sdk INTERFACE)
    target_include_directories(ukui_test_sdk INTERFACE
        "${UKUI_QUICK_SOURCE_DIR}/core"
        "${UKUI_QUICK_SOURCE_DIR}/framework/widget"
        "${UKUI_QUICK_SOURCE_DIR}/framework/widget-ui"
        "${UKUI_QUICK_SOURCE_DIR}/framework/config")
    target_link_libraries(ukui_test_sdk INTERFACE "${UKUI_FRAMEWORK_LIBRARY}" "${UKUI_CORE_LIBRARY}")
    set(UKUI_TEST_LIBRARIES ukui_test_sdk)
endif()
add_executable(peony_integration_tests peony_integration_tests.cpp)
target_link_libraries(peony_integration_tests PRIVATE ${UKUI_TEST_LIBRARIES} Qt5::Quick Qt5::Widgets Qt5::Test)
target_compile_definitions(peony_integration_tests PRIVATE
    MONITOR_WIDGET_PATH="${CMAKE_BINARY_DIR}/widgets"
    MONITOR_QML_PATH="${CMAKE_BINARY_DIR}/qml")
add_dependencies(peony_integration_tests kylinmonitorplugin)
add_executable(desktop_capture desktop_capture.cpp)
target_link_libraries(desktop_capture PRIVATE Qt5::Gui)
# Run through scripts/test-peony.sh so dconf, D-Bus and the display are isolated.
add_test(NAME peony_integration COMMAND bash "${PROJECT_SOURCE_DIR}/scripts/test-peony.sh" "$<TARGET_FILE:peony_integration_tests>")
set_tests_properties(peony_integration PROPERTIES TIMEOUT 90)
