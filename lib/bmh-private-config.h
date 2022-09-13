// SPDX-FileCopyrightText: 2022 Brian Calhoun <brian@bemorehuman.org>
//
// SPDX-License-Identifier: MIT
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// This file is part of bemorehuman. See https://bemorehuman.org

#ifndef BMH_PRIVATE_CONFIG_H
#define BMH_PRIVATE_CONFIG_H


#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <syslog.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__BSD_VISIBLE)
#include <stdarg.h>
#endif

#include "bmh-config.h"


#define DEFAULT_CONFIG_FILE "/etc/bemorehuman/bemorehuman.conf"
#define MAX_CONFIG_LINE_SIZE 1024





#endif // BMH_PRIVATE_CONFIG_H
