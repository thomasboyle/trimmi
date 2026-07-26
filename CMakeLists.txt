cmake_minimum_required(VERSION 3.21)

set(TRIMMI_VERSION "0.0.0")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TRIMMI_VERSION_FILE)
    string(STRIP "${TRIMMI_VERSION_FILE}" TRIMMI_VERSION)
endif()

project(Trimmi VERSION ${TRIMMI_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(TRIMMI_BUNDLE_FFMPEG "Copy ffmpeg/ffprobe next to the executable" ON)
option(TRIMMI_RUN_WINDEPLOYQT "Run windeployqt after build (disable in CI; stage_payload does it)" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Svg Concurrent)

set(TRIMMI_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/TitleBar.cpp
    src/DropZone.cpp
    src/VideoPlayer.cpp
    src/TimelineWidget.cpp
    src/ThumbnailGenerator.cpp
    src/Exporter.cpp
    src/EncoderCapabilities.cpp
    src/TimeFormat.cpp
    src/GpuStatusCard.cpp
    resources/resources.qrc
)

add_executable(Trimmi WIN32 ${TRIMMI_SOURCES})

target_compile_definitions(Trimmi PRIVATE
    TRIMMI_VERSION_STR="${TRIMMI_VERSION}"
)

target_include_directories(Trimmi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Trimmi PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Svg
    Qt6::Concurrent
)

if(WIN32)
    target_compile_definitions(Trimmi PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(Trimmi PRIVATE dxgi)
endif()

if(MSVC)
    target_compile_options(Trimmi PRIVATE /W4 /permissive-)
else()
    target_compile_options(Trimmi PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS Trimmi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(TRIMMI_BUNDLE_FFMPEG)
    find_program(FFMPEG_EXECUTABLE NAMES ffmpeg ffmpeg.exe)
    find_program(FFPROBE_EXECUTABLE NAMES ffprobe ffprobe.exe)
    if(FFMPEG_EXECUTABLE)
        install(PROGRAMS ${FFMPEG_EXECUTABLE} DESTINATION ${CMAKE_INSTALL_BINDIR})
        add_custom_command(TARGET Trimmi POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FFMPEG_EXECUTABLE}" "$<TARGET_FILE_DIR:Trimmi>"
            COMMENT "Copying ffmpeg next to Trimmi")
    endif()
    if(FFPROBE_EXECUTABLE)
        install(PROGRAMS ${FFPROBE_EXECUTABLE} DESTINATION ${CMAKE_INSTALL_BINDIR})
        add_custom_command(TARGET Trimmi POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FFPROBE_EXECUTABLE}" "$<TARGET_FILE_DIR:Trimmi>"
            COMMENT "Copying ffprobe next to Trimmi")
    endif()
endif()

if(WIN32 AND TRIMMI_RUN_WINDEPLOYQT)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt windeployqt.exe
        HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET Trimmi POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --multimedia
                --compiler-runtime
                "$<TARGET_FILE:Trimmi>"
            COMMENT "Deploying Qt runtime + MSVC CRT with windeployqt")
    endif()
endif()
