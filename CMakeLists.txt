cmake_minimum_required(VERSION 3.21)
project(XdrTablet VERSION 0.1.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Network SerialPort)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(XdrTablet
    src/main.cpp
    src/XdrClient.cpp
    src/XdrClient.h
)

qt_add_resources(XdrTablet "app_icon"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FILES
        assets/XdrTablet.png
)

set_target_properties(XdrTablet PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

qt_add_qml_module(XdrTablet
    URI XdrTablet
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/RdsMonitorWindow.qml
        qml/TmcWindow.qml
        qml/FrequencyScale.qml
        qml/VintageMeter.qml
        qml/TuningKnob.qml
        qml/VintageButton.qml
)

target_link_libraries(XdrTablet PRIVATE
    Qt6::Quick
    Qt6::Network
    Qt6::SerialPort
)

set_target_properties(XdrTablet PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS XdrTablet
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
