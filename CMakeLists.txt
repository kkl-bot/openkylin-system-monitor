cmake_minimum_required(VERSION 3.16)
project(KylinMonitor VERSION 1.4.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
option(BUILD_PEONY_PLUGIN "Build the native Peony desktop widget (Qt 5 host)" ON)
option(BUILD_PEONY_INTEGRATION_TESTS "Build tests using the installed ukui-quick SDK" OFF)
if(BUILD_PEONY_PLUGIN)
    # Peony 4.24 is a Qt 5 process: a Qt 6 plugin cannot be loaded into it.
    set(QT_DEFAULT_MAJOR_VERSION 5)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Qml Quick)
    set(QT_VERSION_MAJOR 5)
else()
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS DBus)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)

add_library(monitor_backend
    src/domain/metrics.cpp
    src/domain/snapshot_json.cpp
    src/backend/file_reader.cpp
    src/backend/hwmon_reader.cpp
    src/backend/cpu_collector.cpp
    src/backend/memory_collector.cpp
    src/backend/gpu_collector.cpp
    src/backend/monitor_service.cpp)
target_include_directories(monitor_backend PUBLIC src)
target_link_libraries(monitor_backend PUBLIC Qt${QT_VERSION_MAJOR}::Core)

# Process management belongs to the standalone task manager; the Peony module
# continues to link only the resource collectors it needs.
add_library(process_backend
    src/domain/process_types.cpp
    src/backend/process_collector.cpp
    src/backend/process_control.cpp
    src/backend/process_sampling_thread.cpp
    src/backend/process_service.cpp)
target_include_directories(process_backend PUBLIC src)
target_link_libraries(process_backend PUBLIC Qt${QT_VERSION_MAJOR}::Core)

add_library(network_backend
    src/backend/network_collector.cpp
    src/backend/proxy_reader.cpp
    src/backend/network_sampling_thread.cpp
    src/backend/network_service.cpp)
target_include_directories(network_backend PUBLIC src)
target_link_libraries(network_backend PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)

add_library(monitor_ui
    src/ui/theme.cpp
    src/ui/widget_appearance.cpp
    src/ui/paint_palette.cpp
    src/ui/ui_font.cpp
    src/ui/typography.cpp
    src/ui/flow_layout.cpp
    src/ui/theme_watcher.cpp
    src/ui/monitor_model.cpp
    src/ui/history_chart.cpp
    src/ui/resource_tile.cpp
    src/ui/resource_sparkline.cpp
    src/ui/resource_history_geometry.cpp
    src/ui/task_navigation.cpp
    src/ui/performance_workspace.cpp
    src/ui/performance_page.cpp
    src/ui/performance_metric.cpp
    src/ui/memory_composition_bar.cpp
    src/ui/detail_window.cpp
    src/ui/process_table_model.cpp
    src/ui/process_table_view.cpp
    src/ui/process_table_header.cpp
    src/ui/process_heat_delegate.cpp
    src/ui/process_filter_model.cpp
    src/ui/process_details_dialog.cpp
    src/ui/process_page.cpp
    src/ui/process_presentation.cpp
    src/ui/network_page.cpp
    src/ui/network_details_dialog.cpp
    src/ui/network_history_chart.cpp
    src/ui/network_presentation.cpp
    src/ui/network_rate_card.cpp
    src/ui/mini_window.cpp
    src/app/application_controller.cpp
    src/app/snapshot_endpoint.cpp
    src/app/process_controller.cpp
    src/app/network_controller.cpp
    resources/resources.qrc)
target_link_libraries(monitor_ui PUBLIC monitor_backend process_backend network_backend Qt${QT_VERSION_MAJOR}::Widgets PRIVATE Qt${QT_VERSION_MAJOR}::DBus)

add_executable(kylin-monitor src/main.cpp)
target_link_libraries(kylin-monitor PRIVATE monitor_ui)
if(BUILD_PEONY_PLUGIN)
    add_subdirectory(plugin)
endif()
if(MSVC)
    add_compile_options(/utf-8)
else()
    target_compile_options(monitor_backend PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(monitor_ui PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(process_backend PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(network_backend PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(layout_preview tests/layout_preview.cpp)
    target_link_libraries(layout_preview PRIVATE monitor_ui)
    add_executable(network_preview tests/network_preview.cpp)
    target_link_libraries(network_preview PRIVATE monitor_ui)
    add_executable(collector_tests tests/collector_tests.cpp)
    target_link_libraries(collector_tests PRIVATE monitor_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME collectors COMMAND collector_tests)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME ui COMMAND ui_tests -platform offscreen)
    add_executable(ui_font_tests tests/ui_font_tests.cpp)
    target_link_libraries(ui_font_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME ui_fonts COMMAND ui_font_tests -platform offscreen)
    add_executable(resource_tile_tests tests/resource_tile_tests.cpp)
    target_link_libraries(resource_tile_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME resource_tiles COMMAND resource_tile_tests -platform offscreen)
    add_executable(process_backend_tests tests/process_backend_tests.cpp)
    target_link_libraries(process_backend_tests PRIVATE process_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME process_backend COMMAND process_backend_tests)
    add_executable(process_service_tests tests/process_service_tests.cpp)
    target_link_libraries(process_service_tests PRIVATE process_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME process_service COMMAND process_service_tests)
    add_executable(process_ui_tests tests/process_ui_tests.cpp)
    target_link_libraries(process_ui_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME process_ui COMMAND process_ui_tests -platform offscreen)
    add_executable(task_manager_workflow_tests tests/task_manager_workflow_tests.cpp)
    target_link_libraries(task_manager_workflow_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME task_manager_workflow COMMAND task_manager_workflow_tests -platform offscreen)
    set_tests_properties(process_backend process_service process_ui task_manager_workflow PROPERTIES TIMEOUT 45)
    foreach(test_name network_collector proxy_reader network_service)
        add_executable(${test_name}_tests tests/${test_name}_tests.cpp)
        target_link_libraries(${test_name}_tests PRIVATE network_backend Qt${QT_VERSION_MAJOR}::Test)
        add_test(NAME ${test_name} COMMAND ${test_name}_tests)
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 45)
    endforeach()
    add_executable(network_ui_tests tests/network_ui_tests.cpp)
    target_link_libraries(network_ui_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME network_ui COMMAND network_ui_tests -platform offscreen)
    set_tests_properties(network_ui PROPERTIES TIMEOUT 45)
    add_executable(network_workflow_tests tests/network_workflow_tests.cpp)
    target_link_libraries(network_workflow_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME network_workflow COMMAND network_workflow_tests -platform offscreen)
    set_tests_properties(network_workflow PROPERTIES TIMEOUT 45)
    if(BUILD_PEONY_PLUGIN)
        add_executable(widget_backend_tests tests/widget_backend_tests.cpp)
        target_link_libraries(widget_backend_tests PRIVATE monitor_widget_backend Qt5::Test)
        add_test(NAME widget_backend COMMAND widget_backend_tests -platform offscreen)
        set_tests_properties(widget_backend PROPERTIES ENVIRONMENT "QT_QUICK_BACKEND=software")
    endif()
    if(BUILD_PEONY_INTEGRATION_TESTS)
        add_subdirectory(tests/peony)
        add_executable(widget_appearance_tests tests/widget_appearance_tests.cpp)
        target_link_libraries(widget_appearance_tests PRIVATE monitor_ui Qt5::Test)
        add_test(NAME widget_appearance COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-peony.sh" "$<TARGET_FILE:widget_appearance_tests>")
        set_tests_properties(widget_appearance PROPERTIES TIMEOUT 45)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS kylin-monitor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Application)
install(FILES packaging/kylin-monitor.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Application)
install(FILES resources/kylin-monitor.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps COMPONENT Application)
install(FILES LICENSE README.md THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/kylin-monitor)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/kylin-monitor/docs)
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "kylin-monitor")
set(CPACK_DEBIAN_PACKAGE_NAME "kylin-monitor")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_PACKAGE_FILE_NAME "kylin-monitor-${PROJECT_VERSION}-linux-amd64")
set(CPACK_PACKAGE_CONTACT "Kylin Monitor maintainers")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
if(BUILD_PEONY_PLUGIN)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "peony (>= 4.24), libukui-quick-framework1 (>= 4.25), qml-module-org-ukui-quick-items, qml-module-org-ukui-quick-platform, qml-module-qtquick-controls2, qml-module-qtquick-layouts")
endif()
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Task manager with network monitoring and CPU, GPU, memory desktop widget for openKylin")
include(CPack)
