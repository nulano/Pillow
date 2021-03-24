cmake_minimum_required(VERSION 3.12)

project(libraqm)


find_library(fribidi NAMES fribidi)
find_library(harfbuzz NAMES harfbuzz)
find_library(freetype NAMES freetype)

add_definitions(-DFRIBIDI_LIB_STATIC)


set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(RAQM_SOURCES
	src/raqm.c)
set(RAQM_HEADERS
	src/raqm.h)

add_library(libraqm STATIC
	${RAQM_SOURCES}
	${RAQM_HEADERS})
target_link_libraries(libraqm
	${fribidi}
	${harfbuzz}
	${freetype})
