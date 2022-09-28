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

// This is test-accuracy's helpers file

#include "accuracy.h"

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


// This routine call the bemorehuman server and returns the response
//
// inputs: int scenario,
// outputs: char *raw_response;
// return: int status
int call_bemorehuman_server(int scenario, char *raw_response)
{
        char URL[RB_URL_LENGTH];
        FILE *fp;
        // default is dev
        char file_suffix[64] = "";
        
        // port 4567 is the port for rb_test_server
        char server_loc[32] = "127.0.0.1";

        // Usage:
        // test-accuracy -core [dev | stage | prod]   (default is dev)

        // 3 options for suffix for fname of testfile: "", "_prod",  or "_prod_pid"
        // 3 options for server machine: localhost, stage, jed.cfkprod.com
        
        if (TEST_LOC_STAGE == g_server_location)
        {
                strcpy(file_suffix, "_prod");
                strcpy(server_loc, "stage:4567");
        }
        
        if (TEST_LOC_PROD == g_server_location)
        {
                strcpy(file_suffix, "_prod");
                strcpy(server_loc, "jed.cfkprod.com:4567");
        }

        // we want to add the pid iff we're in -core testing mode
        if ((TEST_MODE_CORE == g_test_mode) && (1 != scenario) && (2 != scenario) && (4 != scenario))
        {
                // add the pid
                char old_fs[32];
                strcpy(old_fs, file_suffix);

                sprintf(file_suffix, "%s_%d", old_fs, getpid());
        }
        
        switch (scenario)
        {
                case 1:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "first_rating", 1, file_suffix);
                        break;
                case 2:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "recs", 2, file_suffix);
                        break;
                case 3:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "event", 3, file_suffix);
                        break;
                case 5:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "search", 5, file_suffix);
                        break;
                case 6:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "info", 6, file_suffix);
                        break;
                case 7:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "scan", 7, file_suffix);
                        break;
                case 8:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "purchase", 8, file_suffix);
                        break;
                case 9:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "will_i_like_this", 9, file_suffix);
                        break;
                case 10:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "imageurl", 10, file_suffix);
                        break;
                case 11:  sprintf(URL, RB_CURL_PB_PREFIX, server_loc, "internal-singlerec", 11, file_suffix);
                        break;
                case DYNAMIC_RATE: sprintf(URL, RB_LOCAL_CURL_PB_PREFIX, server_loc, "rate", DYNAMIC_RATE, file_suffix);
                        break;
                case DYNAMIC_SCAN: sprintf(URL, RB_LOCAL_CURL_PB_PREFIX, server_loc, "internal-singlerec", DYNAMIC_SCAN, file_suffix);
                        break;
                default: printf("*** Broken scenaraio value in call_bemorehumanapp_server! Bad.\n");
                        break;
        }
        
        // printf("URL for scenario %d is ---%s---\n", scenario, URL);
        fp = popen(URL, "r");
        assert(NULL != fp);
        
        /* build up the response, line by line, and let the caller deal with the contents */
        char line[RB_LINE];
        int num_chars_read = 0;
        while (NULL != fgets(line, RB_LINE, fp))
        {
            strcat(raw_response, line);
            num_chars_read += strlen(line);
        }
        //cleanup
        pclose(fp);

        return (num_chars_read);

} /* end call_bemorehumanapp_server() */

/* end helpers.c */
