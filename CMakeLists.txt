cmake_minimum_required(VERSION 3.21)
project(Trimmi VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(TRIMMI_BUNDLE_FFMPEG "Copy ffmpeg/ffprobe next to the executable" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Svg Concurrent)

# FFmpeg: support vcpkg FindFFMPEG, unofficial-ffmpeg config, or pkg-config
set(TRIMMI_FFMPEG_LIBS "")
set(TRIMMI_FFMPEG_INCLUDES "")

if(TARGET unofficial::ffmpeg::ffmpeg)
    # already provided
elseif(COMMAND find_package)
    find_package(FFMPEG QUIET COMPONENTS avcodec avformat avutil swscale)
endif()

if(FFMPEG_FOUND)
    set(TRIMMI_FFMPEG_LIBS ${FFMPEG_LIBRARIES})
    set(TRIMMI_FFMPEG_INCLUDES ${FFMPEG_INCLUDE_DIRS})
else()
    find_package(unofficial-ffmpeg CONFIG QUIET)
    if(TARGET unofficial::ffmpeg::ffmpeg)
        set(TRIMMI_FFMPEG_LIBS unofficial::ffmpeg::ffmpeg)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PC_AVCODEC QUIET libavcodec)
            pkg_check_modules(PC_AVFORMAT QUIET libavformat)
            pkg_check_modules(PC_AVUTIL QUIET libavutil)
            pkg_check_modules(PC_SWSCALE QUIET libswscale)
        endif()

        find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h
            HINTS ${PC_AVCODEC_INCLUDE_DIRS} $ENV{FFMPEG_DIR}/include)
        find_library(AVCODEC_LIBRARY NAMES avcodec
            HINTS ${PC_AVCODEC_LIBRARY_DIRS} $ENV{FFMPEG_DIR}/lib)
        find_library(AVFORMAT_LIBRARY NAMES avformat
            HINTS ${PC_AVFORMAT_LIBRARY_DIRS} $ENV{FFMPEG_DIR}/lib)
        find_library(AVUTIL_LIBRARY NAMES avutil
            HINTS ${PC_AVUTIL_LIBRARY_DIRS} $ENV{FFMPEG_DIR}/lib)
        find_library(SWSCALE_LIBRARY NAMES swscale
            HINTS ${PC_SWSCALE_LIBRARY_DIRS} $ENV{FFMPEG_DIR}/lib)

        if(NOT AVCODEC_INCLUDE_DIR OR NOT AVCODEC_LIBRARY OR NOT AVFORMAT_LIBRARY
           OR NOT AVUTIL_LIBRARY OR NOT SWSCALE_LIBRARY)
            message(FATAL_ERROR
                "FFmpeg not found. Install via vcpkg (`ffmpeg`) or set FFMPEG_DIR.")
        endif()

        set(TRIMMI_FFMPEG_INCLUDES ${AVCODEC_INCLUDE_DIR})
        set(TRIMMI_FFMPEG_LIBS
            ${AVFORMAT_LIBRARY}
            ${AVCODEC_LIBRARY}
            ${SWSCALE_LIBRARY}
            ${AVUTIL_LIBRARY})
    endif()
endif()

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

target_include_directories(Trimmi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TRIMMI_FFMPEG_INCLUDES}
)

target_link_libraries(Trimmi PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Svg
    Qt6::Concurrent
    ${TRIMMI_FFMPEG_LIBS}
)

if(WIN32)
    target_compile_definitions(Trimmi PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(Trimmi PRIVATE
        dxgi
        ws2_32
        secur32
        bcrypt
        mfplat
        mfuuid
        strmiids
        ole32
        user32
    )
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

if(WIN32)
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
