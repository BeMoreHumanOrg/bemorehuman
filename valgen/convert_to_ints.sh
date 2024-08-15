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
# This script takes in a GroupLens ratings.csv file, doubles the rating
# to get rid of the possible '.5', then outputs a new ratings.out file.
# Ratings scale in the ratings.out file is now 1-10, integers only.
cut -d ',' -f 1,2 ratings.csv > tmp1.txt
cut -d ',' -f 3 ratings.csv | awk '{print $1*2}' > tmp2.txt
paste -d ',' tmp1.txt tmp2.txt > ratings.out
rm tmp1.txt tmp2.txt

# get rid of header line (do it this way to make Mac happy)
sed '1d' ratings.out > r.tmp; mv r.tmp ratings.out

