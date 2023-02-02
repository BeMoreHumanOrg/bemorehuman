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

cmake_minimum_required(VERSION 3.7)

set(CMAKE_C_STANDARD 99)

project(test-accuracy)

# OTHER_SOURCE_FILES are things I didn't write and so I want to relax a bit with compilation flags
set(OTHER_SOURCE_FILES
        ../recgen/recgen.pb-c.c
        ../recgen/recgen.pb-c.h
        )

set(MY_SOURCE_FILES
        main.c
        accuracy.h
        helpers.c
        )

set(SOURCE_FILES ${MY_SOURCE_FILES} ${OTHER_SOURCE_FILES})

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include_directories(/opt/pkg/include)
    link_directories(/opt/pkg/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    link_directories(/usr/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    include_directories(/usr/pkg/include)
    link_directories(/usr/pkg/lib)
endif()

add_executable(test-accuracy ${SOURCE_FILES})

# set compile flags for my source file only
# orig set(MY_COMPILE_FLAGS "${COMPILE_FLAGS} -Werror -Wno-vla")
# -O3 below is for valgrind debugging and for good performance
set(MY_COMPILE_FLAGS "${COMPILE_FLAGS} -Wno-padded -Wno-vla  -Wno-declaration-after-statement -Wno-reserved-id-macro -Wno-documentation-unknown-command")

if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(MY_COMPILE_FLAGS "${MY_COMPILE_FLAGS} -Weverything")
else()
    set(MY_COMPILE_FLAGS "${MY_COMPILE_FLAGS} -Wall")
endif()

set_source_files_properties(${MY_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "${MY_COMPILE_FLAGS} ${APPFLAGS}")

# set max log level
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS "MAX_LOG_LEVEL=LOG_LEVEL_INFO")

target_link_libraries(${PROJECT_NAME} m pthread protobuf-c bmh crypto)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        )
