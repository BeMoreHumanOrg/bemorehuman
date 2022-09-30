#!/bin/sh

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

#
# This script assumes:
#   you've installed bemorehuman in ~/src
#   you want the build output dirs at ~/build-xxx and
#   you want symlinks in ~/bin to the output binaries
#
mkdir ~/bin

mkdir ~/build-bmhlib
cd ~/build-bmhlib || exit
cmake ~/src/bemorehuman/lib  
cmake --build . --target all
sudo make install 	 # to install the libbmh.so in /usr/lib, the config, and header files 

mkdir ~/build-ratgen
cd ~/build-ratgen || exit
cmake ~/src/bemorehuman/ratgen  
cmake --build . --target all
ln -s  ~/build-ratgen/ratgen  ~/bin/ratgen   # to make a link to ratgen in a convenient place

mkdir ~/build-valgen
cd ~/build-valgen || exit
cmake ~/src/bemorehuman/valgen  
cmake --build . --target all
ln -s  ~/build-valgen/valgen  ~/bin/valgen

mkdir ~/build-recgen
cd ~/build-recgen || exit
cmake ~/src/bemorehuman/recgen  
cmake --build . --target all
ln -s  ~/build-recgen/recgen  ~/bin/recgen

# The test-accuracy binary needs to be in the ..bemorehuman/test-accuracy directory.
# cmake will put it there and you can make a link to a friendlier place.
mkdir ~/build-test-accuracy
cd ~/build-test-accuracy || exit
cmake ~/src/bemorehuman/test-accuracy  
cmake --build . --target all
ln -s ~/src/bemorehuman/test-accuracy/test-accuracy ~/bin/test-accuracy   
