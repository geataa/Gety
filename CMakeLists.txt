cmake_minimum_required(VERSION 3.20)
project(Gety VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 100% Portable Static C++ Runtime (/MT in Release, /MTd in Debug)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_definitions(
    -DUNICODE
    -D_UNICODE
    -DWIN32_LEAN_AND_MEAN
    -DNOMINMAX
)

include_directories(
    src
    resources
)

set(SOURCES
    src/main.cpp
    src/core/Config.h
    src/core/Config.cpp
    src/core/I18n.h
    src/core/I18n.cpp
    src/core/Types.h
    src/core/SpeedLimiter.h
    src/core/SpeedLimiter.cpp
    src/core/BatchExpander.h
    src/core/BatchExpander.cpp
    src/core/ClipboardWatcher.h
    src/core/ClipboardWatcher.cpp
    src/core/Crypto.h
    src/core/Crypto.cpp
    src/core/JsonParser.h
    src/engine/WinHttpUtils.h
    src/engine/SegmentWorker.h
    src/engine/SegmentWorker.cpp
    src/engine/DownloadTask.h
    src/engine/DownloadTask.cpp
    src/engine/DownloadManager.h
    src/engine/DownloadManager.cpp
    src/engine/MediaExtractor.h
    src/engine/MediaExtractor.cpp
    src/engine/OllamaClient.h
    src/engine/OllamaClient.cpp
    src/engine/HuggingFaceClient.h
    src/engine/HuggingFaceClient.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/FloatingWindow.h
    src/ui/FloatingWindow.cpp
    src/ui/TrayIcon.h
    src/ui/TrayIcon.cpp
    src/ui/Dialogs.h
    src/ui/Dialogs.cpp
    src/ui/Direct2DRenderer.h
    src/ui/Direct2DRenderer.cpp
    resources/resource.h
    resources/Gety.rc
    resources/app.manifest
)

add_executable(Gety WIN32 ${SOURCES})

target_compile_options(Gety PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W3 $<$<CONFIG:Release>:/O2 /Oi /Ot> /Zi>
)

target_link_options(Gety PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/DEBUG /MAP:Gety.map>
)

target_link_libraries(Gety PRIVATE
    d2d1
    dwrite
    windowscodecs
    winhttp
    comctl32
    shlwapi
    ole32
    shell32
    winmm
    dwmapi
    crypt32
)

add_custom_command(TARGET Gety POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Gety>
        ${CMAKE_SOURCE_DIR}/Gety.exe
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tools
        $<TARGET_FILE_DIR:Gety>/tools
    COMMENT "Copying Gety.exe to root directory and tools to target directory"
)

add_executable(test_engine
    tests/test_engine.cpp
    src/core/Config.h
    src/core/Config.cpp
    src/core/I18n.h
    src/core/I18n.cpp
    src/core/Types.h
    src/core/SpeedLimiter.h
    src/core/SpeedLimiter.cpp
    src/core/BatchExpander.h
    src/core/BatchExpander.cpp
    src/core/Crypto.h
    src/core/Crypto.cpp
    src/core/JsonParser.h
    src/engine/WinHttpUtils.h
    src/engine/SegmentWorker.h
    src/engine/SegmentWorker.cpp
    src/engine/DownloadTask.h
    src/engine/DownloadTask.cpp
    src/engine/MediaExtractor.h
    src/engine/MediaExtractor.cpp
    src/engine/OllamaClient.h
    src/engine/OllamaClient.cpp
    src/engine/HuggingFaceClient.h
    src/engine/HuggingFaceClient.cpp
)

target_compile_options(test_engine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W3 $<$<CONFIG:Release>:/O2>>
)

target_link_libraries(test_engine PRIVATE
    winhttp
    shlwapi
    winmm
    crypt32
)
