cmake_minimum_required(VERSION 3.22)
project(StructureLimitRemover LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -w")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -w")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--strip-all -s")

# preloader-android (the LeviLauncher mod SDK) is required and committed to the
# repo.  The remaining third-party deps are served either from the local
# vendor/ directory (offline builds) or fetched from the network by
# FetchContent (GitHub Actions / clean clones).
set(VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")

if(EXISTS "${VENDOR_DIR}/nlohmann_json/CMakeLists.txt")
    message(STATUS "StructureLimitRemover: using vendored dependencies (offline build)")
    set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${VENDOR_DIR}/nlohmann_json" CACHE PATH "" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_BOOST_PFR "${VENDOR_DIR}/boost_pfr" CACHE PATH "" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_MAGIC_ENUM "${VENDOR_DIR}/magic_enum" CACHE PATH "" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_FMT "${VENDOR_DIR}/fmt11" CACHE PATH "" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_LUNASVG "${VENDOR_DIR}/lunasvg" CACHE PATH "" FORCE)
else()
    message(STATUS "StructureLimitRemover: vendor/ not found, FetchContent will download deps")
endif()

add_subdirectory(${VENDOR_DIR}/preloader-android)

add_library(StructureLimitRemover SHARED src/main.cpp)
target_link_libraries(StructureLimitRemover PRIVATE preloader log)
target_include_directories(StructureLimitRemover PRIVATE ${VENDOR_DIR}/preloader-android/src)
