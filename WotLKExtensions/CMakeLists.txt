# Collect source and header files separately

FetchContent_Declare(
  detours
  GIT_REPOSITORY https://github.com/microsoft/Detours.git
  GIT_TAG        v4.0.1
)
FetchContent_MakeAvailable(detours)
FILE(GLOB detours_sources
    ${detours_SOURCE_DIR}/src/*.h
    ${detours_SOURCE_DIR}/src/*.cpp
)
set_property (
    SOURCE ${detours_SOURCE_DIR}/src/uimports.cpp
    APPEND PROPERTY HEADER_FILE_ONLY true
)
add_library(detours STATIC ${detours_sources})
target_compile_options(detours PRIVATE /W4 /WX /Zi /MT /Gy /Gm- /Zl /Od)
target_include_directories(detours PUBLIC ${detours_SOURCE_DIR})

file(GLOB_RECURSE WOTLK_EXTENSIONS_SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE WOTLK_EXTENSIONS_HEADERS CONFIGURE_DEPENDS "*.h")

option(ENABLE_DISCORD "Build Discord Rich Presence integration" ON)
if(NOT ENABLE_DISCORD)
    list(FILTER WOTLK_EXTENSIONS_SOURCES EXCLUDE REGEX "/(discord/|Impl/DiscordRPC\\.)")
    list(FILTER WOTLK_EXTENSIONS_HEADERS EXCLUDE REGEX "/(discord/|Impl/DiscordRPC\\.)")
endif()

set(MSDF_LIBS_DIR "${CMAKE_SOURCE_DIR}/libs")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
add_subdirectory("${MSDF_LIBS_DIR}/freetype-2.14.1" "${CMAKE_CURRENT_BINARY_DIR}/freetype" EXCLUDE_FROM_ALL)
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

set(MSDFGEN_CORE_ONLY OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(MSDFGEN_INSTALL OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DYNAMIC_RUNTIME OFF CACHE BOOL "" FORCE)
add_subdirectory("${MSDF_LIBS_DIR}/msdfgen" "${CMAKE_CURRENT_BINARY_DIR}/msdfgen" EXCLUDE_FROM_ALL)
foreach(_msdf_target IN ITEMS msdfgen-core msdfgen-ext freetype)
    if(TARGET ${_msdf_target})
        set_property(TARGET ${_msdf_target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
endforeach()

if(NOT TARGET ankerl::unordered_dense)
    add_library(ankerl_unordered_dense INTERFACE)
    target_include_directories(ankerl_unordered_dense INTERFACE "${MSDF_LIBS_DIR}/unordered_dense/include")
    add_library(ankerl::unordered_dense ALIAS ankerl_unordered_dense)
endif()

add_library(WotLKExtensions SHARED ${WOTLK_EXTENSIONS_SOURCES} ${WOTLK_EXTENSIONS_HEADERS})
set_property(TARGET WotLKExtensions PROPERTY CXX_STANDARD 20)
set_property(TARGET WotLKExtensions PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

set_property(TARGET WotLKExtensions PROPERTY USE_FOLDERS true)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${WOTLK_EXTENSIONS_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${WOTLK_EXTENSIONS_HEADERS})

target_link_libraries(WotLKExtensions PRIVATE
	detours
	msdfgen::msdfgen-core
	msdfgen::msdfgen-ext
	freetype
	ankerl::unordered_dense
	winhttp
	bcrypt
)
target_include_directories(WotLKExtensions PUBLIC
    ${detours_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/Helpers
	${CMAKE_CURRENT_SOURCE_DIR}/Detours
	${CMAKE_CURRENT_SOURCE_DIR}/Spells
	${CMAKE_CURRENT_SOURCE_DIR}/Entities
	${CMAKE_CURRENT_SOURCE_DIR}/Lua
	${CMAKE_CURRENT_SOURCE_DIR}/CDBCMgr
	${CMAKE_CURRENT_SOURCE_DIR}/Impl
	${CMAKE_CURRENT_SOURCE_DIR}/Cvars
	${CMAKE_CURRENT_SOURCE_DIR}/Packets
	${CMAKE_CURRENT_SOURCE_DIR}/TalentFrame
	${MSDF_LIBS_DIR}/freetype-2.14.1/include
	${MSDF_LIBS_DIR}/msdfgen
	${CMAKE_CURRENT_BINARY_DIR}/msdfgen
	${MSDF_LIBS_DIR}/unordered_dense/include
)
if(ENABLE_DISCORD)
    target_include_directories(WotLKExtensions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/discord)
    target_compile_definitions(WotLKExtensions PRIVATE ENABLE_DISCORD)
endif()

option(AUTO_UPDATER_IGNORES_DLL "Background downloader ignores WotLKExtensions.dll in the manifest" OFF)
if(AUTO_UPDATER_IGNORES_DLL)
    target_compile_definitions(WotLKExtensions PRIVATE AUTO_UPDATER_IGNORES_DLL)
endif()

option(ENABLE_MONK_UNARMED "Build unarmed animation detours" OFF)
if(ENABLE_MONK_UNARMED)
    target_compile_definitions(WotLKExtensions PRIVATE ENABLE_MONK_UNARMED)
endif()

# ---- Logging configuration ----
set(LOG_LEVELS DEBUG INFO WARN ERROR NONE)
set(LOG_LEVEL DEBUG CACHE STRING "Logging level")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS ${LOG_LEVELS})

if(LOG_LEVEL STREQUAL "DEBUG")
    target_compile_definitions(WotLKExtensions PRIVATE LOG_LEVEL=4)
elseif(LOG_LEVEL STREQUAL "INFO")
    target_compile_definitions(WotLKExtensions PRIVATE LOG_LEVEL=3)
elseif(LOG_LEVEL STREQUAL "WARN")
    target_compile_definitions(WotLKExtensions PRIVATE LOG_LEVEL=2)
elseif(LOG_LEVEL STREQUAL "ERROR")
    target_compile_definitions(WotLKExtensions PRIVATE LOG_LEVEL=1)
elseif(LOG_LEVEL STREQUAL "NONE")
    target_compile_definitions(WotLKExtensions PRIVATE LOG_LEVEL=0)
else()
    message(FATAL_ERROR "Invalid LOG_LEVEL ${LOG_LEVEL}")
endif()

include(local.cmake OPTIONAL)
