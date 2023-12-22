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

#include "bmh-private-config.h"

//
// This file contains general helper functions
//

//  BEGIN time helpers

// This routine calculates the current time in milliseconds returning its value in long long format.
long long current_time_millis(void)
{
    long long t;
    struct timeval tv;

    gettimeofday(&tv, 0);

    t = tv.tv_sec;
    t = (t * 1000) + (tv.tv_usec/1000);

    return (t);
} // end current_time_millis()


// This routine calculates the current time in microseconds returning its value in long long format.
long long current_time_micros(void)
{
    long long t;
    struct timeval tv;

    gettimeofday(&tv, 0);

    t = tv.tv_sec;
    t = (t * 1000000) + (tv.tv_usec);

    return (t);
} // end current_time_micros()

//  END time helpers

//  BEGIN A few string helpers

#ifndef HAVE_STRLCAT

// strlcat() - Safely concatenate two strings.
size_t                             // O - Length of string
strlcat(char *restrict dst,        // O - Destination string
        const char *restrict src,  // I - Source string
        size_t size)               // I - Size of destination string buffer
{
    char       *d = dst;
    const char *s = src;
    size_t      n = size;
    size_t      dlen;

    // Find the end of dst and adjust bytes left but don't go past end.
    while (n-- != 0 && *d != '\0')
        d++;
    dlen = d - dst;
    n = size - dlen;

    if (n == 0)
        return (dlen + strlen(s));
    while (*s != '\0')
    {
        if (n != 1)
        {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';

    return (dlen + (s - src));  // count does not include NULL
} // end strlcat()

#endif // !HAVE_STRLCAT

#ifndef HAVE_STRLCPY

// strlcpy() - Safely copy one string to another.
size_t                             // O - Length of string
strlcpy(char *restrict dst,        // O - Destination string
        const char *restrict src,  // I - Source string
        size_t size)               // I - Size of destination string buffer
{
    char       *d = dst;
    const char *s = src;
    size_t      n = size;

    // Copy as many bytes as will fit
    if (n != 0)
    {
         while (--n != 0)
         {
             if ((*d++ = *s++) == '\0')
                 break;
         }
    }

    // Not enough room in dst, add NULL and traverse rest of src
    if (n == 0)
    {
        if (size != 0)
            *d = '\0';          // NULL-terminate dst
        while (*s++)
            ;
    }

    return (s - src - 1);   // count does not include NULL
} // end strlcpy()

#endif // !HAVE_STRLCPY

// The 2 guys below are from the K&R "C Programming Language" 2nd Edition.
// reverse:  reverse string s in place
void reverse(char s[])
{
    uint_fast32_t i, j;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--)
    {
        unsigned int c;
        c = (unsigned char) s[i];
        s[i] = s[j];
        s[j] = (char) c;
    }
} // end reverse()


// itoa:  convert n to characters in s
void itoa(int n, char s[])
{
    int i, sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do
    {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    reverse(s);
}  // end itoa()



//  END String helpers

//  BEGIN file helpers

// Check to see if passed-in dir exists. If it doesn't, make it. If there's a problem, return false.
// NOTE that this function can only create one new dir level, by design.
bool check_make_dir(char *dirname)
{
    int e;
    struct stat sb;

    e = stat(dirname, &sb);
    if (e == 0)
    {
        if (sb.st_mode & S_IFREG)
        {
            syslog(LOG_ERR, "%s is a regular file so can't create a directory.", dirname);
            return false;
        }
    }
    else
    {
        if ((errno = ENOENT))
        {
            syslog(LOG_INFO, "The %s directory does not exist. Creating new directory...", dirname);
            e = mkdir(dirname, 0755);
            if (e != 0)
            {
                // note: errno 13 is permission denied
                syslog(LOG_ERR, "mkdir failed; errno=%d. Errno 13 is permission denied.", errno);
                perror("Error when trying to create dir");
                return false;
            }
            else
            {
                syslog(LOG_INFO, "created the directory %s",dirname);
            }
        }
    }
    return (true);
} // end check_make_dir()

//  END file helpers

//  BEGIN math helpers

// Round isn't really included anywhere convenient. input: double, output long
long bmh_round(double x)
{
    return (x > 0) ? (long)(x + 0.5) : (long)(x - 0.5);
}

//  END math helpers






