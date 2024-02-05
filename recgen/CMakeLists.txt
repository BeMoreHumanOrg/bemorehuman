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

project(recgen C)

# Add an option to control whether to build with FastCGI support.
option(USE_FCGI "Build with FastCGI support" OFF)

# Add an option to control whether to build with protobuf support.
option(USE_PROTOBUF "Build with protobuf support" OFF)

if (USE_PROTOBUF)
    # OTHER_SOURCE_FILES are things I didn't write and so I want to relax a bit with compilation flags
    set(OTHER_SOURCE_FILES
            recgen.pb-c.c
            recgen.pb-c.h)
endif()

# MY_SOURCE_FILES are things I wrote and so I want to be stricter about compilation flags
set(MY_SOURCE_FILES
        main.c
        big_mem.c
        predictions.c
        recgen.h)

set(SOURCE_FILES ${MY_SOURCE_FILES} ${OTHER_SOURCE_FILES})

include_directories(vendor/yyjson/src)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include_directories(/opt/pkg/include)
    link_directories(/opt/pkg/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    link_directories(/usr/lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    include_directories(/usr/pkg/include)
    link_directories(/usr/pkg/lib /usr/local/lib)
endif()



add_executable(recgen ${SOURCE_FILES})
add_executable(hum hum.c)

# set compile flags for my source file only
# can add "-fsanitize=address -fno-omit-frame-pointer" if I want to incur overhead of mem leak checking at runtime. Must add link flag -fsanitize....
# orig set(MY_COMPILE_FLAGS "${COMPILE_FLAGS} -std=c11 -Wno-vla  -Wno-reserved-id-macro -O3")
set(MY_COMPILE_FLAGS "${COMPILE_FLAGS} -Wno-vla -Wno-documentation-unknown-command ")

# The "-Wno-declaration-after-statement is b/c of a bug in clangd that gives a false warning when using C99 and above
# clang: set(MY_COMPILE_FLAGS "${MY_COMPILE_FLAGS} -Wno-declaration-after-statement -Wno-reserved-id-macro -Weverything")
set(MY_COMPILE_FLAGS "${MY_COMPILE_FLAGS} -Wno-declaration-after-statement -Wall ")

set_source_files_properties(${MY_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "${MY_COMPILE_FLAGS} ${APPFLAGS}")
set_source_files_properties(hum.c PROPERTIES COMPILE_FLAGS "${MY_COMPILE_FLAGS} ${APPFLAGS}")

# set max log level
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS "MAX_LOG_LEVEL=LOG_LEVEL_INFO")
set_target_properties(hum PROPERTIES COMPILE_DEFINITIONS "MAX_LOG_LEVEL=LOG_LEVEL_INFO")

if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
   # set rpath
   set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
   set_target_properties(hum PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# begin yyjson stuff

# Add some options (optional)
set(YYJSON_DISABLE_NON_STANDARD ON CACHE INTERNAL "")

# Add the `yyjson` subdirectory
add_subdirectory(vendor/yyjson)

# end yyjson stuff

# uncomment line below to link in the allocation sanitizer for memory debugging
#target_link_options(${PROJECT_NAME} PUBLIC "-fsanitize=address")

target_link_libraries(${PROJECT_NAME} m pthread bmh yyjson)

# Check if FastCGI support is enabled.
if(USE_FCGI)
    # Link the FastCGI library and include the header files
    target_link_libraries(${PROJECT_NAME} fcgi)

    # Add a preprocessor definition to enable FastCGI-specific code
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_FCGI)
endif()

# Check if protobuf support is enabled.
if(USE_PROTOBUF)
    # Link the protobuf library and include the header files
    target_link_libraries(${PROJECT_NAME} protobuf-c)

    # Add a preprocessor definition to enable FastCGI-specific code
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_PROTOBUF)
endif()



target_link_libraries(hum bmh)

install(TARGETS ${PROJECT_NAME} hum
        RUNTIME DESTINATION bin
        )
