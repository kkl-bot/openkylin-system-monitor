cmake_minimum_required(VERSION 3.16)
project(KylinMonitor VERSION 1.1.0 LANGUAGES CXX)

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

add_library(monitor_backend
    src/domain/metrics.cpp
    src/backend/file_reader.cpp
    src/backend/hwmon_reader.cpp
    src/backend/cpu_collector.cpp
    src/backend/memory_collector.cpp
    src/backend/gpu_collector.cpp
    src/backend/monitor_service.cpp)
target_include_directories(monitor_backend PUBLIC src)
target_link_libraries(monitor_backend PUBLIC Qt${QT_VERSION_MAJOR}::Core)

add_library(monitor_ui
    src/ui/theme.cpp
    src/ui/theme_watcher.cpp
    src/ui/monitor_model.cpp
    src/ui/history_chart.cpp
    src/ui/resource_tile.cpp
    src/ui/detail_window.cpp
    src/ui/mini_window.cpp
    src/app/application_controller.cpp
    resources/resources.qrc)
target_link_libraries(monitor_ui PUBLIC monitor_backend Qt${QT_VERSION_MAJOR}::Widgets)

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
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(collector_tests tests/collector_tests.cpp)
    target_link_libraries(collector_tests PRIVATE monitor_backend Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME collectors COMMAND collector_tests)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE monitor_ui Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME ui COMMAND ui_tests -platform offscreen)
    if(BUILD_PEONY_PLUGIN)
        add_executable(widget_backend_tests tests/widget_backend_tests.cpp)
        target_link_libraries(widget_backend_tests PRIVATE monitor_widget_backend Qt5::Test)
        add_test(NAME widget_backend COMMAND widget_backend_tests -platform offscreen)
        set_tests_properties(widget_backend PROPERTIES ENVIRONMENT "QT_QUICK_BACKEND=software")
    endif()
    if(BUILD_PEONY_INTEGRATION_TESTS)
        add_subdirectory(tests/peony)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS kylin-monitor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Application)
install(FILES packaging/kylin-monitor.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Application)
install(FILES resources/kylin-monitor.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps COMPONENT Application)
install(FILES LICENSE README.md THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/kylin-monitor)
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
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CPU, GPU and memory desktop monitor for openKylin")
include(CPack)
