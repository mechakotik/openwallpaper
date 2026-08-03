cmake_minimum_required(VERSION 4.0)
project(wallpaperd)

include(GNUInstallDirs)
find_package(PkgConfig)

option(WD_DEBUG "Enable wasm debugging facilities" OFF)
option(WD_VIDEO "Enable video wallpaper support with mpv" ON)
option(WD_WLROOTS "Enable wlroots output support" ON)
option(WD_AUDIO_VISUALIZER "Enable audio visualization support" ON)
option(WD_BUNDLE_WAMRC "Build bundled wamrc for wasm AOT compilation" ON)
option(WD_WAMRC_STATIC_LLVM "Statically link vendored LLVM to bundled wamrc" OFF)

option(WD_SDL_SHARED "Dynamically link system SDL" OFF)
option(WD_SDL_IMAGE_SHARED "Dynamically link system SDL_image" OFF)
option(WD_SDL_SHADERCROSS_SHARED "Dynamically link system SDL_shadercross" OFF)
option(WD_FFTW_SHARED "Dynamically link system FFTW" OFF)

option(WD_SANITIZE "Build with sanitizers" OFF)

set(BUILD_SHARED_LIBS OFF)

if(WD_SANITIZE)
    add_compile_options(-fsanitize=address,leak,undefined -fno-sanitize=alignment)
    add_link_options(-fsanitize=address,leak,undefined -fno-sanitize=alignment)
endif()

file(GLOB_RECURSE WD_SOURCES src/*.c external/sds/sds.c)
add_executable(wallpaperd ${WD_SOURCES})
target_include_directories(wallpaperd PRIVATE include external/sds)
set_property(SOURCE external/sds/sds.c APPEND PROPERTY COMPILE_OPTIONS
    -include "${CMAKE_CURRENT_SOURCE_DIR}/src/sds_alloc.h"
)

if(WD_VIDEO)
    pkg_check_modules(MPV QUIET mpv)
    if(MPV_FOUND)
        message(STATUS "Found libmpv, enabling video wallpaper support")
        target_compile_definitions(wallpaperd PRIVATE WD_VIDEO)
        target_include_directories(wallpaperd PRIVATE ${MPV_INCLUDE_DIRS})
        target_compile_options(wallpaperd PRIVATE ${MPV_CFLAGS_OTHER})
    else()
        message(WARNING "libmpv not found, not building video wallpaper support")
    endif()
endif()

if(WD_SDL_SHARED)
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
    target_link_libraries(wallpaperd PRIVATE SDL3::SDL3-shared)
else()
    set(SDL_STATIC ON)
    set(SDL_DISABLE_INSTALL ON)
    add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
    target_link_libraries(wallpaperd PRIVATE SDL3::SDL3-static)
    target_include_directories(wallpaperd PRIVATE external/SDL/include)
endif()

if(WD_SDL_IMAGE_SHARED)
    find_package(SDL3_image REQUIRED CONFIG REQUIRED COMPONENTS SDL3_image-shared)
    target_link_libraries(wallpaperd PRIVATE SDL3_image::SDL3_image-shared)
else()
    set(SDLIMAGE_VENDORED ON)
    set(SDLIMAGE_DEPS_SHARED OFF)
    set(SDLIMAGE_INSTALL OFF)
    set(SDLIMAGE_PNG ON)
    set(SDLIMAGE_WEBP ON)
    set(SDLIMAGE_AVIF OFF)
    set(SDLIMAGE_BMP OFF)
    set(SDLIMAGE_GIF OFF)
    set(SDLIMAGE_JPG OFF)
    set(SDLIMAGE_JXL OFF)
    set(SDLIMAGE_LBM OFF)
    set(SDLIMAGE_PCX OFF)
    set(SDLIMAGE_PNM OFF)
    set(SDLIMAGE_QOI OFF)
    set(SDLIMAGE_SVG OFF)
    set(SDLIMAGE_TGA OFF)
    set(SDLIMAGE_TIF OFF)
    set(SDLIMAGE_XCF OFF)
    set(SDLIMAGE_XPM OFF)
    set(SDLIMAGE_XV OFF)
    add_subdirectory(external/SDL_image EXCLUDE_FROM_ALL)
    target_link_libraries(wallpaperd PRIVATE SDL3_image::SDL3_image-static)
    target_include_directories(wallpaperd PRIVATE external/SDL_image/include)
endif()

if(WD_SDL_SHADERCROSS_SHARED)
    find_package(SDL3_shadercross REQUIRED CONFIG REQUIRED COMPONENTS SDL3_shadercross-shared)
    target_link_libraries(wallpaperd PRIVATE SDL3_shadercross::SDL3_shadercross-shared)
else()
    set(SDLSHADERCROSS_VENDORED ON)
    set(SDLSHADERCROSS_STATIC ON)
    set(SDLSHADERCROSS_DXC OFF)
    set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF)
    add_subdirectory(external/SDL_shadercross EXCLUDE_FROM_ALL)
    target_link_libraries(wallpaperd PRIVATE SDL3_shadercross::SDL3_shadercross-static)
    target_include_directories(wallpaperd PRIVATE external/SDL_shadercross/include)
endif()

set(WAMR_BUILD_PLATFORM "linux")
set(WAMR_BUILD_TARGET "X86_64")
set(WAMR_BUILD_INTERP 1)
if(WD_DEBUG)
    add_compile_definitions(WD_DEBUG=1)
    set(WAMR_BUILD_DEBUG_INTERP 1)
    set(WAMR_BUILD_FAST_INTERP 0)
else()
    set(WAMR_BUILD_DEBUG_INTERP 0)
    set(WAMR_BUILD_FAST_INTERP 1)
endif()
set(WAMR_BUILD_AOT 1)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 1)
set(WAMR_BUILD_REF_TYPES 1)
set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/wasm-micro-runtime)

if(WD_BUNDLE_WAMRC)
    if(WD_WAMRC_STATIC_LLVM)
        include(ExternalProject)

        if(CMAKE_BUILD_TYPE)
            set(WD_WAMRC_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
        else()
            set(WD_WAMRC_BUILD_TYPE Release)
        endif()

        set(WD_WAMRC_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/wallpaperd-wamrc-build")
        set(WD_WAMRC_BINARY_PATH "${CMAKE_CURRENT_BINARY_DIR}/wallpaperd-wamrc")
        set(WD_WAMRC_LLVM_EXTRA_CMAKE_FLAGS
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF")
        set(WD_WAMRC_LLVM_REPOSITORY "https://github.com/llvm/llvm-project.git"
            CACHE STRING "LLVM repository used for statically linked wallpaperd-wamrc")
        set(WD_WAMRC_LLVM_TAG "release/22.x"
            CACHE STRING "LLVM tag or branch used for statically linked wallpaperd-wamrc")
        set(WD_WAMRC_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        string(APPEND WD_WAMRC_EXE_LINKER_FLAGS " -static-libstdc++ -static-libgcc")
        string(STRIP "${WD_WAMRC_EXE_LINKER_FLAGS}" WD_WAMRC_EXE_LINKER_FLAGS)
        set(WD_WAMRC_LLVM_SOURCE_DIR "${WAMR_ROOT_DIR}/core/deps/llvm")
        set(WD_WAMRC_VENDORED_LLVM_CORE
            "${WD_WAMRC_LLVM_SOURCE_DIR}/build/lib/libLLVMCore.a")
        set(WD_WAMRC_VENDORED_LLVM_CMAKE_DIR
            "${WD_WAMRC_LLVM_SOURCE_DIR}/build/lib/cmake/llvm")

        ExternalProject_Add(wallpaperd-wamrc-llvm
            PREFIX "${CMAKE_CURRENT_BINARY_DIR}/wallpaperd-wamrc-llvm"
            GIT_REPOSITORY "${WD_WAMRC_LLVM_REPOSITORY}"
            GIT_TAG "${WD_WAMRC_LLVM_TAG}"
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
            SOURCE_DIR "${WD_WAMRC_LLVM_SOURCE_DIR}"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND "")

        ExternalProject_Add(wallpaperd-wamrc-build
            SOURCE_DIR "${WAMR_ROOT_DIR}/wamr-compiler"
            BINARY_DIR "${WD_WAMRC_BUILD_DIR}"
            DEPENDS wallpaperd-wamrc-llvm
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=${WD_WAMRC_BUILD_TYPE}
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                "-DCMAKE_EXE_LINKER_FLAGS=${WD_WAMRC_EXE_LINKER_FLAGS}"
                -DLLVM_DIR=${WD_WAMRC_VENDORED_LLVM_CMAKE_DIR}
                -DWAMR_BUILD_PLATFORM=${WAMR_BUILD_PLATFORM}
                -DWAMR_BUILD_TARGET=${WAMR_BUILD_TARGET}
                -DWAMR_BUILD_WITH_CUSTOM_LLVM=0
            BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --target wamrc --parallel
            INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_if_different <BINARY_DIR>/wamrc "${WD_WAMRC_BINARY_PATH}"
            INSTALL_BYPRODUCTS "${WD_WAMRC_BINARY_PATH}"
            USES_TERMINAL_CONFIGURE TRUE
            USES_TERMINAL_BUILD TRUE)

        ExternalProject_Add_Step(wallpaperd-wamrc-build prepare_llvm
            COMMAND "${CMAKE_COMMAND}" -E chdir "${WAMR_ROOT_DIR}/wamr-compiler"
                    ./build_llvm.sh --arch X86 --extra-cmake-flags "${WD_WAMRC_LLVM_EXTRA_CMAKE_FLAGS}"
            BYPRODUCTS "${WD_WAMRC_VENDORED_LLVM_CORE}"
            DEPENDEES download
            DEPENDERS configure
            USES_TERMINAL TRUE)

        message(STATUS "Building bundled wamrc with vendored static LLVM")
        target_compile_definitions(wallpaperd PRIVATE WD_WAMRC_BINARY="wallpaperd-wamrc")
        add_dependencies(wallpaperd wallpaperd-wamrc-build)
        install(PROGRAMS "${WD_WAMRC_BINARY_PATH}" DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME wallpaperd-wamrc)
    else()
        if(NOT DEFINED WAMR_BUILD_WITH_CUSTOM_LLVM)
            set(WAMR_BUILD_WITH_CUSTOM_LLVM 1)
        endif()

        find_package(LLVM QUIET CONFIG)
        if(NOT LLVM_FOUND AND NOT LLVM_DIR)
            file(GLOB WD_LLVM_CMAKE_DIRS LIST_DIRECTORIES true
                    /usr/lib/llvm*/lib64/cmake/llvm
                    /usr/lib/llvm*/lib/cmake/llvm
                    /usr/lib/llvm/*/lib64/cmake/llvm
                    /usr/lib/llvm/*/lib/cmake/llvm)
            list(REMOVE_DUPLICATES WD_LLVM_CMAKE_DIRS)
            list(SORT WD_LLVM_CMAKE_DIRS COMPARE NATURAL ORDER DESCENDING)

            foreach(WD_LLVM_CMAKE_DIR IN LISTS WD_LLVM_CMAKE_DIRS)
                if(EXISTS "${WD_LLVM_CMAKE_DIR}/LLVMConfig.cmake")
                    set(LLVM_DIR "${WD_LLVM_CMAKE_DIR}")
                    break()
                endif()
            endforeach()

            find_package(LLVM QUIET CONFIG)
        endif()

        if(LLVM_FOUND)
            message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}, building bundled wamrc")
            target_compile_definitions(wallpaperd PRIVATE WD_WAMRC_BINARY="wallpaperd-wamrc")
            unset(WAMR_BUILD_FAST_INTERP)
            add_subdirectory(external/wasm-micro-runtime/wamr-compiler EXCLUDE_FROM_ALL)
            set_target_properties(wamrc PROPERTIES OUTPUT_NAME wallpaperd-wamrc)
            add_dependencies(wallpaperd wamrc)
            install(PROGRAMS $<TARGET_FILE:wamrc> DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME wallpaperd-wamrc)
        else()
            message(WARNING "LLVM not found, not building bundled wamrc")
        endif()
    endif()
endif()

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
include(${SHARED_DIR}/utils/uncommon/shared_uncommon.cmake)
add_library(wallpaperd-vmlib ${WAMR_RUNTIME_LIB_SOURCE})
target_sources(wallpaperd PRIVATE ${UNCOMMON_SHARED_SOURCE})
target_link_libraries(wallpaperd PRIVATE wallpaperd-vmlib)

if(WD_WLROOTS)
    function(add_protocol PROTOCOL_NAME)
        set(PROTOCOL_XML "${CMAKE_CURRENT_SOURCE_DIR}/wayland/${PROTOCOL_NAME}.xml")
        set(PROTOCOL_C "${CMAKE_CURRENT_BINARY_DIR}/${PROTOCOL_NAME}.c")
        set(PROTOCOL_H "${CMAKE_CURRENT_BINARY_DIR}/${PROTOCOL_NAME}.h")

        add_custom_command(
            OUTPUT ${PROTOCOL_C} ${PROTOCOL_H}
            COMMAND ${WAYLAND_SCANNER} private-code ${PROTOCOL_XML} ${PROTOCOL_C}
            COMMAND ${WAYLAND_SCANNER} client-header ${PROTOCOL_XML} ${PROTOCOL_H}
            DEPENDS ${PROTOCOL_XML}
        )

        target_sources(wallpaperd PRIVATE ${PROTOCOL_C})
        target_include_directories(wallpaperd PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    endfunction()

    find_program(WAYLAND_SCANNER NAMES wayland-scanner)
    if(WAYLAND_SCANNER)
        message(STATUS "Found wayland-scanner")
        add_protocol(wayland)
        add_protocol(xdg-shell)
        add_protocol(wlr-layer-shell-unstable-v1)
        add_protocol(hyprland-lock-notify-v1)

        set(YYJSON_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(external/yyjson EXCLUDE_FROM_ALL)
        target_link_libraries(wallpaperd PRIVATE yyjson)
        target_compile_definitions(wallpaperd PRIVATE -DWD_WLROOTS)
    else()
        message(WARNING "wayland-scanner not found, disabling wlroots output support")
    endif()
endif()

if(WD_AUDIO_VISUALIZER)
    pkg_check_modules(PIPEWIRE QUIET libpipewire-0.3)
    pkg_check_modules(SPA QUIET libspa-0.2)
    set(AUDIO_AVAILABLE OFF)
    if(PIPEWIRE_FOUND AND SPA_FOUND)
        message(STATUS "Found PipeWire and libspa, enabling PipeWire support in audio visualizer")
        target_sources(wallpaperd PRIVATE external/cava/input/pipewire.c)
        target_compile_definitions(wallpaperd PRIVATE WD_PIPEWIRE)
        target_include_directories(wallpaperd PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${SPA_INCLUDE_DIRS})
        target_compile_options(wallpaperd PRIVATE ${PIPEWIRE_CFLAGS_OTHER} ${SPA_CFLAGS_OTHER})
        set(AUDIO_AVAILABLE ON)
    else()
        message(STATUS "PipeWire or libspa not found, disabling PipeWire support in audio visualizer")
    endif()

    pkg_check_modules(PULSE QUIET libpulse-simple)
    if(PULSE_FOUND)
        message(STATUS "Found PulseAudio, enabling its support in audio visualizer")
        target_sources(wallpaperd PRIVATE external/cava/input/pulse.c)
        target_compile_definitions(wallpaperd PRIVATE WD_PULSE)
        target_include_directories(wallpaperd PRIVATE ${PULSE_INCLUDE_DIRS})
        target_compile_options(wallpaperd PRIVATE ${PULSE_CFLAGS_OTHER})
        set(AUDIO_AVAILABLE ON)
    else()
        message(STATUS "PulseAudio not found, disabling its support in audio visualizer")
    endif()

    pkg_check_modules(PORTAUDIO QUIET portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message(STATUS "Found PortAudio, enabling its support in audio visualizer")
        target_sources(wallpaperd PRIVATE external/cava/input/portaudio.c)
        target_compile_definitions(wallpaperd PRIVATE WD_PORTAUDIO)
        target_include_directories(wallpaperd PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
        target_compile_options(wallpaperd PRIVATE ${PORTAUDIO_CFLAGS_OTHER})
        set(AUDIO_AVAILABLE ON)
    else()
        message(STATUS "PortAudio not found, disabling its support in audio visualizer")
    endif()

    if(AUDIO_AVAILABLE)
        target_sources(wallpaperd PRIVATE
            external/cava/cavacore.c
            external/cava/input/common.c
        )
        target_include_directories(wallpaperd PRIVATE external/cava)
        target_compile_definitions(wallpaperd PRIVATE WD_AUDIO_VISUALIZER=1)

        find_package(Threads REQUIRED)
        target_link_libraries(wallpaperd PRIVATE ${WD_AUDIO_VISUALIZER_LIBS} Threads::Threads)

        if(WD_FFTW_SHARED)
            pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
            target_link_libraries(wallpaperd PRIVATE PkgConfig::FFTW3)
        else()
            add_subdirectory(external/fftw3 EXCLUDE_FROM_ALL)
            target_include_directories(fftw3 PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/fftw3/api>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/external/fftw3>
            )
            target_link_libraries(wallpaperd PRIVATE fftw3)
            target_include_directories(wallpaperd PRIVATE external/fftw3/api ${CMAKE_CURRENT_BINARY_DIR}/external/fftw3)
        endif()
    else()
        message(WARNING "No audio backend found, disabling audio visualizer suppport")
    endif()
else()
    target_compile_definitions(wallpaperd PRIVATE WD_AUDIO_VISUALIZER=0)
endif()

install(TARGETS wallpaperd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
