# SPDX-FileCopyrightText: 2022 Brian Calhoun <brian@bemorehuman.org>
#
# SPDX-License-Identifier: MIT
#
# MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# This file is part of bemorehuman. See https://bemorehuman.org

cmake_minimum_required(VERSION 3.10)

project(test-accuracy C)

# Add an option to control whether to build with protobuf support.
option(USE_PROTOBUF "Build with protobuf support" OFF)

if (USE_PROTOBUF)
    # OTHER_SOURCE_FILES are things I didn't write and so I want to relax a bit with compilation flags
    set(OTHER_SOURCE_FILES
            ../recgen/recgen.pb-c.c
            ../recgen/recgen.pb-c.h)

    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        include_directories(/opt/homebrew/include)
        link_directories(/opt/homebrew/lib)
    endif()
endif()

set(MY_SOURCE_FILES
        main.c
        accuracy.h
        helpers.c
        )

set(SOURCE_FILES ${MY_SOURCE_FILES} ${OTHER_SOURCE_FILES})

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include_directories(/opt/homebrew/opt/openssl/include)
    link_directories(/opt/homebrew/opt/openssl/lib /usr/local/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    link_directories(/usr/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    include_directories(/usr/pkg/include)
    link_directories(/usr/pkg/lib /usr/local/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif()

add_executable(test-accuracy ${SOURCE_FILES})

# set compile flags for my source file only
set(MY_COMPILE_FLAGS "${COMPILE_FLAGS} -Wno-padded -Wno-vla  -Wno-declaration-after-statement -Wno-reserved-id-macro ")

# -O1, -g for valgrind
# set(MY_COMPILE_FLAGS "${MY_COMPILE_FLAGS} -Wall -Wextra -O1 -g")
set(MY_COMPILE_FLAGS "${MY_COMPILE_FLAGS} -Wall -Wextra -O3")

set_source_files_properties(${MY_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "${MY_COMPILE_FLAGS} ${APPFLAGS}")

# set max log level
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS "MAX_LOG_LEVEL=LOG_LEVEL_INFO")

if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
   # set rpath
   set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

target_link_libraries(${PROJECT_NAME} m pthread bmh crypto yyjson)

# Check if protobuf support is enabled.
if(USE_PROTOBUF)
    # Link the protobuf library and include the header files
    target_link_libraries(${PROJECT_NAME} protobuf-c)

    # Add a preprocessor definition to enable FastCGI-specific code
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PROTOBUF)
endif()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        )
