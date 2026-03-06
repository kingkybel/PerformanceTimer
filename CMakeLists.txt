cmake_minimum_required(VERSION 3.26)
project(PerfomanceTimer
        VERSION 1.0.0
        DESCRIPTION "A header only repo containing a performance - timer wrapper"
        HOMEPAGE_URL "https://github.com/kingkybel/PerfomanceTimer")

include(GNUInstallDirs)
include(CTest)

# Use shared settings from cmake-common submodule
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/cmake-common/cmake/DkybBuildSettings.cmake")
    message(FATAL_ERROR
        "Missing cmake-common submodule. Run:\n"
        "  git submodule update --init --recursive")
endif ()
include(cmake-common/cmake/DkybBuildSettings.cmake)
dkyb_apply_common_settings(CXX_STANDARD 23)

option(PERFORMANCETIMER_FETCH_GOOGLETEST "Fetch GoogleTest with CMake" ON)
if(BUILD_TESTING)
    include(FetchContent)
    if(PERFORMANCETIMER_FETCH_GOOGLETEST)
        set(INSTALL_GTEST ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
                googletest
                URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
        )
        FetchContent_MakeAvailable(googletest)
    else()
        find_package(GTest REQUIRED)
    endif()
endif()

set(CMAKE_FIND_LIBRARY_PREFIXES lib)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

include_directories(
        BEFORE SYSTEM ${CMAKE_SOURCE_DIR}/include
        system
)
link_directories(
        /usr/local/lib
        system
)

add_library(PerformanceTimer INTERFACE)
# Adding the install interface generator expression makes sure that the include
# files are installed to the proper location (provided by GNUInstallDirs)
target_include_directories(
        PerformanceTimer
        INTERFACE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(PerformanceTimer INTERFACE cxx_std_23)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=* -p ${CMAKE_SOURCE_DIR}/build")

file(GLOB HEADER_FILES ${CMAKE_SOURCE_DIR}/include/*.h)

# Install the header files directly under 'include/dkyb'
install(FILES ${HEADER_FILES}
        DESTINATION include/dkyb)
