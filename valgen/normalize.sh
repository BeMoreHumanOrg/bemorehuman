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
# This script normalizes the movie ids to make sure they're in sequence.
# 

# Make an ordered list of the unique eltids
os_name=$(uname)
numeric_opt="-g"

#if [ "$os_name" = "NetBSD" ] || [ "$os_name" = "Darwin" ] ; then
#    numeric_opt="-n"
#fi

cut -f 2,3 ratings.out | cut -d, -f 2,3 | sort "$numeric_opt" | cut -f 1 | cut -d, -f 1 | uniq > tmp1.out

# Number each row, and that's the bmhid
awk '{printf("%d,%s\n", NR, $0)}' tmp1.out > bmhid-glid.out
rm tmp1.out

# Now update the ratings.out file to use the bmhid.
sort "$numeric_opt" -t, -k 2 ratings.out > ratings_k2_sort.out
sort "$numeric_opt" -t, -k 2 bmhid-glid.out > bg_k2_sort.out

join -t, -1 2 -2 2 -o 2.1,1.1,2.3 bg_k2_sort.out ratings_k2_sort.out > normalized.out

# final sort to make it look nice if we need to debug by hand
if [ "$os_name" = "Linux" ] ; then
    sort -t, -k 1V,2V normalized.out > ratings.out
fi
if [ "$os_name" = "NetBSD" ] || [ "$os_name" = "Darwin" ] || [ "$os_name" = "FreeBSD" ]; then
    # claude  sort -t, -k1n -k2n normalized.out > ratings.out
    sort -t, -k1g -k2g normalized.out > ratings.out
fi

# remove temp files
rm normalized.out
rm bg_k2_sort.out
rm ratings_k2_sort.out

# Now we want to create a file that has bmhid and movie name.

# File names
file1="bmhid-glid.out"
file2="movies.csv"

# Temporary file for output
temp_file="temp_output.csv"
: > "$temp_file"

# Use awk to process the files
awk -F, 'BEGIN { OFS="," }
    # Load file1 into an array
    NR==FNR { ids[$2]=$1; next }

    # Process file2
    { 
        if ($1 in ids) {
            # Replace the first column of file2 with the matching id from file1
            $1 = ids[$1]
            # Output the modified line
            print
        }
    }
' "$file1" "$file2" > "$temp_file"

# Move the temporary file to replace file2 (if required)
mv "$temp_file" elements.out




