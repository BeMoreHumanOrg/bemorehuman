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

project(valgen C)

set(MY_SOURCE_FILES
        big_rat.c
        main.c
        precursors.c
        valgen.h)

set(SOURCE_FILES ${MY_SOURCE_FILES})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    link_directories(/usr/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    include_directories(/usr/pkg/include)
    link_directories(/usr/pkg/lib /usr/local/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    link_directories(/usr/local/lib)
endif()

add_executable(valgen ${SOURCE_FILES})

# set compile flags for my source file only
# -O3 below is for valgrind debugging
set(MY_COMPILE_FLAGS "${COMPILE_FLAGS} -Wno-vla -Wno-unsafe-buffer-usage -O3")
#set(MY_COMPILE_FLAGS "${COMPILE_FLAGS} -Wno-vla -g")

# The "-Wno-declarartion-after-statement" is b/c of a bug in clangd that gives a false warning when using C99 and above
set(MY_COMPILE_FLAGS "${MY_COMPILE_FLAGS} -Wall -Wextra")

set_source_files_properties(${MY_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "${MY_COMPILE_FLAGS} ${APPFLAGS}")

# set max log level
set_target_properties(valgen PROPERTIES COMPILE_DEFINITIONS "MAX_LOG_LEVEL=LOG_LEVEL_INFO")

if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
   # set rpath
   set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

target_link_libraries(${PROJECT_NAME} m pthread bmh)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        )
