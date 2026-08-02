cmake_minimum_required(VERSION 3.21)
project(XdrTablet VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Network)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(XdrTablet
    src/main.cpp
    src/XdrClient.cpp
    src/XdrClient.h
)

set_target_properties(XdrTablet PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

qt_add_qml_module(XdrTablet
    URI XdrTablet
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/FrequencyScale.qml
        qml/VintageMeter.qml
        qml/TuningKnob.qml
        qml/VintageButton.qml
)

target_link_libraries(XdrTablet PRIVATE Qt6::Quick Qt6::Network)

if(ANDROID)
    set_target_properties(XdrTablet PROPERTIES
        QT_ANDROID_PACKAGE_NAME "com.github.fredfrei.xdrtablet"
        QT_ANDROID_VERSION_NAME "0.1.2"
        QT_ANDROID_VERSION_CODE 3
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()

set_target_properties(XdrTablet PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS XdrTablet
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
