cmake_minimum_required(VERSION 3.16)
project(max-screenshooter VERSION 0.1.0 LANGUAGES CXX)

find_package(ECM 5.100 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets DBus)
find_package(KF5 5.100 REQUIRED COMPONENTS Config WindowSystem)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)

add_library(screenshooter_core STATIC
    src/annotation.cpp
    src/capture.cpp
    src/geometry.cpp
    src/output.cpp
    src/overlay.cpp
    src/pinwindow.cpp
    src/settings.cpp
    src/toolbar.cpp
)
target_include_directories(screenshooter_core PUBLIC src)
target_link_libraries(screenshooter_core PUBLIC
    Qt5::Widgets
    Qt5::DBus
    KF5::ConfigCore
    KF5::WindowSystem
    PkgConfig::XCB
)

add_executable(max-screenshooter src/main.cpp)
target_compile_definitions(max-screenshooter PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(max-screenshooter PRIVATE screenshooter_core)

install(TARGETS max-screenshooter ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
configure_file(data/max-screenshooter.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/max-screenshooter.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/max-screenshooter.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES data/max-screenshooter.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

if(BUILD_TESTING)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    add_executable(coretest tests/coretest.cpp)
    target_link_libraries(coretest PRIVATE screenshooter_core Qt5::Test)
    add_test(NAME coretest COMMAND coretest -platform offscreen)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
