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
project(bmh C)

set(CMAKE_INSTALL_PREFIX "/usr")

include_directories(.)

add_library(
    bmh
    SHARED
    config.c
    helpers.c
    bmh-config.h
    bmh-private-config.h
)

set_target_properties(
    bmh
    PROPERTIES
    PUBLIC_HEADER bmh-config.h
    OUTPUT_NAME "bmh"
)

target_include_directories(bmh PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

install(TARGETS bmh)

# don't overwrite the bemorehuman.conf if it already exists b/c it's likely been customised by user
install(CODE "if (NOT EXISTS \"/etc/bemorehuman/bemorehuman.conf\")
       file(COPY ../config/bemorehuman.conf DESTINATION /etc/bemorehuman)
       endif()
 ")
