cmake_minimum_required(VERSION 3.16)

project(gtxt)

################################################################################
# Source groups
################################################################################

set(inc
    "gtxt_freetype.h"
    "gtxt_glyph.h"
    "gtxt_label.h"
    "gtxt_layout.h"
    "gtxt_richtext.h"
    "gtxt_typedef.h"
    "gtxt_util.h"
)
source_group("inc" FILES ${inc})

set(src
    "gtxt_freetype.c"
    "gtxt_glyph.c"
    "gtxt_label.c"
    "gtxt_layout.c"
    "gtxt_richtext.c"
    "gtxt_util.c"
)
source_group("src" FILES ${src})

set(ALL_FILES
    ${inc}
    ${src}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(TARGET fs)
    target_link_libraries(${PROJECT_NAME} PRIVATE fs)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE external/fs)
endif()
if(TARGET ds)
    target_link_libraries(${PROJECT_NAME} PRIVATE ds)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE external/ds)
endif()
if(TARGET freetype)
    target_link_libraries(${PROJECT_NAME} PRIVATE freetype)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE external/freetype/include)
endif()
