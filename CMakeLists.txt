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

include_directories(${CMAKE_SOURCE_DIR}/external/fs)
include_directories(${CMAKE_SOURCE_DIR}/external/ds)
include_directories(${CMAKE_SOURCE_DIR}/external/freetype/include)

add_library(gtxt ${ALL_FILES})
