# MetaAgent desktop application — webview shell + embedded HTTP server.
# Links the portable metaagent library; UE plugin is unaffected.

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Deep Unreal plugin paths can exceed MAX_PATH during FetchContent; use a short cache dir.
if(WIN32)
    set(_metaagent_fetch_base "$ENV{LOCALAPPDATA}/metaagent-app-deps")
    if(NOT _metaagent_fetch_base STREQUAL "")
        file(TO_CMAKE_PATH "${_metaagent_fetch_base}" _metaagent_fetch_base)
        set(FETCHCONTENT_BASE_DIR "${_metaagent_fetch_base}" CACHE PATH "FetchContent download directory")
    endif()
endif()

option(METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE "Keep console window visible for Windows Release builds" OFF)
option(METAAGENT_APP_STATIC_MSVC_RUNTIME_RELEASE "Use static MSVC runtime in non-Debug configs" ON)

include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if(WIN32 AND MSVC)
    set(_mingw_patterns "msys64" "msys2" "mingw64" "mingw32")
    set(_env_vars "INCLUDE" "CPATH" "C_INCLUDE_PATH" "CPLUS_INCLUDE_PATH" "PATH")
    foreach(_var ${_env_vars})
        if(DEFINED ENV{${_var}})
            string(TOLOWER "$ENV{${_var}}" _value_lower)
            foreach(_pat ${_mingw_patterns})
                if(_value_lower MATCHES "${_pat}")
                    if(_var STREQUAL "PATH")
                        message(WARNING
                            "[metaagent-app] PATH contains '${_pat}'. Use a Visual Studio Developer Command Prompt.")
                    else()
                        message(FATAL_ERROR
                            "[metaagent-app] Unsafe Windows build environment: ${_var} contains '${_pat}'. "
                            "Use MSVC (Visual Studio 2022 Build Tools), not MinGW/MSYS2.")
                    endif()
                endif()
            endforeach()
        endif()
    endforeach()
endif()

if(WIN32 AND NOT MSVC)
    message(FATAL_ERROR
        "[metaagent-app] The desktop app requires MSVC on Windows (WebView2). "
        "Open a Visual Studio Developer Command Prompt and re-run CMake.")
endif()

if(WIN32)
    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG master
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(webview
    GIT_REPOSITORY https://github.com/webview/webview
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(webview)

set(BOOST_INCLUDE_LIBRARIES asio system)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.gz
)
FetchContent_MakeAvailable(Boost)

FetchContent_Declare(cpp-embedlib
    GIT_REPOSITORY https://github.com/yhirose/cpp-embedlib
    GIT_TAG main
)
FetchContent_MakeAvailable(cpp-embedlib)

cpp_embedlib_add(MetaAgentWebAssets
    FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/public
    NAMESPACE MetaAgentWeb
)

add_executable(metaagent-app
    src/main.cpp
    src/metaagent_host.cpp
    src/metaagent_http_mount.cpp
    ${CMAKE_SOURCE_DIR}/tools/sync_http_client.cpp
)

target_include_directories(metaagent-app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tools
)

target_link_libraries(metaagent-app PRIVATE
    metaagent
    httplib::httplib
    webview::core
    MetaAgentWebAssets
    cpp-embedlib-httplib
    Boost::asio
    Boost::system
)

if(MSVC)
    target_compile_options(metaagent-app PRIVATE /W4 /wd4459)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(metaagent-app PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_compile_definitions(metaagent-app PRIVATE
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        $<$<BOOL:${METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE}>:METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE=1>
        $<$<NOT:$<BOOL:${METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE}>>:METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE=0>
    )
    target_link_libraries(metaagent-app PRIVATE ws2_32 mswsock)

    if(MSVC AND NOT METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE)
        target_link_options(metaagent-app PRIVATE
            $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>
            $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>
        )
    endif()

    if(MSVC AND METAAGENT_APP_STATIC_MSVC_RUNTIME_RELEASE)
        set_property(TARGET metaagent-app PROPERTY
            MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<NOT:$<CONFIG:Debug>>:MultiThreaded>"
        )
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
        set(_wv2_arch "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_wv2_arch "x64")
    else()
        set(_wv2_arch "x86")
    endif()
    if(DEFINED FETCHCONTENT_BASE_DIR AND NOT "${FETCHCONTENT_BASE_DIR}" STREQUAL "")
        set(_webview2_deps_root "${FETCHCONTENT_BASE_DIR}")
    else()
        set(_webview2_deps_root "${CMAKE_BINARY_DIR}/_deps")
    endif()
    set(_webview2_loader_dll
        "${_webview2_deps_root}/microsoft_web_webview2-src/build/native/${_wv2_arch}/WebView2Loader.dll"
    )

    if(EXISTS "${_webview2_loader_dll}")
        add_custom_command(TARGET metaagent-app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_webview2_loader_dll}"
                $<TARGET_FILE_DIR:metaagent-app>/WebView2Loader.dll
            COMMENT "Copying WebView2Loader.dll"
        )
    else()
        message(WARNING "WebView2Loader.dll not found at ${_webview2_loader_dll}")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icons/app_icon.ico")
        add_custom_command(TARGET metaagent-app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:metaagent-app>/icons
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/icons/app_icon_small.ico
                $<TARGET_FILE_DIR:metaagent-app>/icons/app_icon_small.ico
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/icons/app_icon.ico
                $<TARGET_FILE_DIR:metaagent-app>/icons/app_icon.ico
            COMMENT "Copying application icons"
        )
    endif()
endif()

if(APPLE)
    target_link_libraries(metaagent-app PRIVATE
        "-framework CoreFoundation"
        "-framework Security"
    )
endif()
