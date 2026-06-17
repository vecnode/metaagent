# Bundled FFmpeg (not committed — see third_party/README.md).
# LGPL 2.1+: link dynamically (DLL/.so) for commercial use without GPL contagion.

set(METAAGENT_FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/third_party/ffmpeg" CACHE PATH
    "Path to FFmpeg prefix (include/, lib/, bin/)")

function(metaagent_link_ffmpeg target)
    if(NOT EXISTS "${METAAGENT_FFMPEG_ROOT}/include/libavcodec/avcodec.h")
        message(FATAL_ERROR
            "FFmpeg not found at ${METAAGENT_FFMPEG_ROOT}\n"
            "Extract FFmpeg dev libraries there. See third_party/README.md")
    endif()

    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${METAAGENT_FFMPEG_ROOT}/include>")
    target_link_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${METAAGENT_FFMPEG_ROOT}/lib>")
    target_link_libraries(${target} PUBLIC avformat avcodec swscale avutil)
    target_compile_definitions(${target} PUBLIC METAAGENT_HAS_FFMPEG=1)
endfunction()

function(metaagent_stage_ffmpeg_runtime target)
    if(NOT WIN32 OR NOT EXISTS "${METAAGENT_FFMPEG_ROOT}/bin")
        return()
    endif()

    get_target_property(_target_type ${target} TYPE)
    if(NOT _target_type STREQUAL "EXECUTABLE")
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${METAAGENT_FFMPEG_ROOT}/bin"
            $<TARGET_FILE_DIR:${target}>
        COMMENT "Copying FFmpeg runtime DLLs next to ${target}")
endfunction()
