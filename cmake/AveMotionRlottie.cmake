set(AVEMOTION_RLOTTIE_AVAILABLE FALSE)
set(AVEMOTION_RLOTTIE_INCLUDE_DIR "")
set(AVEMOTION_RLOTTIE_INTERNAL_LOTTIE_DIR "")
set(AVEMOTION_RLOTTIE_INTERNAL_VECTOR_DIR "")
set(AVEMOTION_RLOTTIE_INTERNAL_PIXMAN_DIR "")
set(AVEMOTION_RLOTTIE_GENERATED_INCLUDE_DIR "")
set(AVEMOTION_RLOTTIE_REPOSITORY "none")
set(AVEMOTION_RLOTTIE_COMMIT "none")
set(AVEMOTION_RLOTTIE_LICENSE_FAMILY "none")

if(AVEMOTION_RLOTTIE_VARIANT STREQUAL "none")
    return()
elseif(AVEMOTION_RLOTTIE_VARIANT STREQUAL "samsung")
    set(_avemotion_rlottie_source
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlottie/samsung/source")
    set(AVEMOTION_RLOTTIE_REPOSITORY "https://github.com/Samsung/rlottie")
    set(AVEMOTION_RLOTTIE_COMMIT "2cab35db755b0e39df40b679969495e90d39c578")
    set(AVEMOTION_RLOTTIE_LICENSE_FAMILY
        "MIT core headers/sources with separately licensed bundled components")
elseif(AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    set(_avemotion_rlottie_source
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rlottie/telegram/source")
    set(AVEMOTION_RLOTTIE_REPOSITORY "https://github.com/TelegramMessenger/rlottie")
    set(AVEMOTION_RLOTTIE_COMMIT "67f103bc8b625f2a4a9e94f1d8c7bd84c5a08d1d")
    set(AVEMOTION_RLOTTIE_LICENSE_FAMILY
        "LGPL-2.1-or-later with separately licensed bundled components")
else()
    message(FATAL_ERROR
        "Invalid AVEMOTION_RLOTTIE_VARIANT=${AVEMOTION_RLOTTIE_VARIANT}; "
        "expected samsung, telegram, or none")
endif()

if(NOT EXISTS "${_avemotion_rlottie_source}/CMakeLists.txt")
    message(FATAL_ERROR "Vendored rlottie source is missing: ${_avemotion_rlottie_source}")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build vendored reference implementation statically" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(LOTTIE_MODULE OFF CACHE BOOL "Disable dynamic image modules in the reference lab" FORCE)
set(LOTTIE_THREAD ON CACHE BOOL "Preserve upstream asynchronous rendering support" FORCE)
set(LOTTIE_CACHE ON CACHE BOOL "Preserve upstream model cache support" FORCE)
set(LOTTIE_TEST OFF CACHE BOOL "Use AveMotion characterization tests" FORCE)
set(LOTTIE_CCACHE OFF CACHE BOOL "Do not alter the host compiler launcher" FORCE)
set(LOTTIE_ASAN OFF CACHE BOOL "Sanitizers are controlled by AveMotion presets" FORCE)

set(_avemotion_rlottie_binary_dir
    "${CMAKE_BINARY_DIR}/_deps/rlottie-${AVEMOTION_RLOTTIE_VARIANT}")

# The pinned rlottie projects still declare cmake_minimum_required(VERSION 3.3).
# CMake 4.0 removed policy compatibility below 3.5.  CMake documents
# CMAKE_POLICY_VERSION_MINIMUM specifically for a parent project adding an
# unmodified third-party project through add_subdirectory().  Keep this scoped
# to the vendored dependency so AveMotion's own policy level is unaffected.
set(_avemotion_policy_minimum_was_defined FALSE)
if(DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(_avemotion_policy_minimum_was_defined TRUE)
    set(_avemotion_saved_policy_minimum "${CMAKE_POLICY_VERSION_MINIMUM}")
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# rlottie enables ASM unconditionally.  On Windows it has no assembler sources
# and historically accepted cl.exe as the ASM compiler.  CMake 4.1 introduced
# CMP0194 and warns unless that legacy third-party behavior is selected.
set(_avemotion_cmp0194_default_was_defined FALSE)
if(POLICY CMP0194)
    if(DEFINED CMAKE_POLICY_DEFAULT_CMP0194)
        set(_avemotion_cmp0194_default_was_defined TRUE)
        set(_avemotion_saved_cmp0194_default "${CMAKE_POLICY_DEFAULT_CMP0194}")
    endif()
    set(CMAKE_POLICY_DEFAULT_CMP0194 OLD)
endif()

add_subdirectory("${_avemotion_rlottie_source}"
    "${_avemotion_rlottie_binary_dir}"
    EXCLUDE_FROM_ALL)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    if(_avemotion_policy_minimum_was_defined)
        set(CMAKE_POLICY_VERSION_MINIMUM "${_avemotion_saved_policy_minimum}")
    else()
        unset(CMAKE_POLICY_VERSION_MINIMUM)
    endif()
endif()
if(POLICY CMP0194)
    if(_avemotion_cmp0194_default_was_defined)
        set(CMAKE_POLICY_DEFAULT_CMP0194 "${_avemotion_saved_cmp0194_default}")
    else()
        unset(CMAKE_POLICY_DEFAULT_CMP0194)
    endif()
endif()

if(NOT TARGET rlottie::rlottie)
    message(FATAL_ERROR "The selected upstream did not define rlottie::rlottie")
endif()

# Both upstream projects attach a shared-library ELF version script through
# target_link_libraries(). When rlottie is forced to STATIC, CMake propagates
# that item to final executables as a LINK_ONLY dependency. Applying a library
# export map to an executable breaks sanitizer/runtime symbol interposition and
# is not meaningful for a static archive. Keep the upstream target unchanged
# otherwise, but remove only the propagated version-script item from its static
# consumer interface.
if(NOT BUILD_SHARED_LIBS)
    get_target_property(_avemotion_rlottie_interface rlottie INTERFACE_LINK_LIBRARIES)
    if(_avemotion_rlottie_interface)
        set(_avemotion_rlottie_filtered_interface "")
        foreach(_avemotion_item IN LISTS _avemotion_rlottie_interface)
            if(NOT _avemotion_item MATCHES "version-script")
                list(APPEND _avemotion_rlottie_filtered_interface "${_avemotion_item}")
            endif()
        endforeach()
        set_property(TARGET rlottie PROPERTY INTERFACE_LINK_LIBRARIES
            "${_avemotion_rlottie_filtered_interface}")
    endif()
endif()

set_target_properties(rlottie PROPERTIES POSITION_INDEPENDENT_CODE ON)
set(AVEMOTION_RLOTTIE_INCLUDE_DIR "${_avemotion_rlottie_source}/inc")
set(AVEMOTION_RLOTTIE_INTERNAL_LOTTIE_DIR "${_avemotion_rlottie_source}/src/lottie")
set(AVEMOTION_RLOTTIE_INTERNAL_VECTOR_DIR "${_avemotion_rlottie_source}/src/vector")
set(AVEMOTION_RLOTTIE_INTERNAL_PIXMAN_DIR "${_avemotion_rlottie_source}/src/vector/pixman")
set(AVEMOTION_RLOTTIE_GENERATED_INCLUDE_DIR "${_avemotion_rlottie_binary_dir}")
set(AVEMOTION_RLOTTIE_AVAILABLE TRUE)
message(STATUS
    "AveMotion reference variant: ${AVEMOTION_RLOTTIE_VARIANT} "
    "(${AVEMOTION_RLOTTIE_COMMIT})")
