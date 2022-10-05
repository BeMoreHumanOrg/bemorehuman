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

#include "ratgen.h"

//
//  This is the bemorehuman ratings generator (ratgen) which takes event/listen/click data and translates them into implicit ratings.
//
//  Implicit ratings are simply precursors to recommendations. Part of a chain or pipeline. The output of ratgen are "ratings" which
//  are used as inputs to the next part of the pipeline: valgen (valence generation).
//
//  If you have explicit ratings such as from a 5-star rating system those can be used as inputs to valgen, and you don't need ratgen.
//  2 usage models:
//
//    - events file -----> ratgen --> valgen --> recgen --> recommendations to end users  OR
//    - existing ratings -----------> valgen --> recgen --> recommendations to end users
//
// NOTE: We're using the term "event" to mean purchase, or click, or listen, etc. Use ratgen for when you don't have explicit ratings
// data, but do have implicit ratings data expressed through purchases, clicks, listens, etc.
//
// NOTE: We use the term "element" to mean a product, a video, a song, or anything that can be recommended. These are the things for
// which we create implicit ratings in ratgen.
//
//
// Input to ratgen is an events.txt file where each line is of the form
//    <integer personid>   <integer elementid>
//
// NOTE: The two fields in this file need to be tab separated.
//
// Example:
//  3    100
//  341  3872
//  341  12
//   ...
//
// General plan:
// - Begin reading events file.
// -- Find the (max number of events of any particular elt for each person: max_event_count).
// -- iterate over all distinct elts evented by this person.
// --- find the (event_count / max_event_count) for each plu
// --- put that ratio into lookup table for the 32 implicit ratings bands
// --- we now have an implicit rating for that person / product
// --- persist the rating info
 
// To compute a person's rating for a particular element, take event_count / max_event_count. Note that max_event_count will be
// different for each person. Where does that percentage fall? The majority of hits will likely be in the early buckets below, so apply
// these rules in order. For example if ratio < 0.03125, that means a rating of 1.
//
// NOTE: final entry is just something > 1 so that we can satisfy "1 < something" in the event_ratio check.
static double g_ratio_thresh[32] = {0.03125, 0.0625, 0.09375, 0.125, 0.15625, 0.1875, 0.21875, 0.25, 0.28125, 0.3125,
                                   0.34375, 0.375, 0.40625, 0.4375, 0.46875, 0.5, 0.53125, 0.5625, 0.59375, 0.625,
                                   0.65625, 0.6875, 0.71875, 0.75, 0.78125, 0.8152, 0.84375, 0.875, 0.90625, 0.9375,
                                   0.96875, 1.01};

//
// Compare any two events for use as a callback function by qsort.
// We're sorting by personid, then eltid
//
// Returns -1 if the first personid is less than the second || same person but first elt is less than second
//          0 if the two personids are the same AND eltids are equal
//          1 if the first personid is greater than the second || same person but first elt is greater than second
//
int eventcmp(const void* p1, const void* p2)
{
    event_t x = *(const event_t *) p1, y = *(const event_t *) p2;

    // This test is to see if p1 < p2.
    if ((x.personid < y.personid) || ((x.personid == y.personid) && (x.eltid < y.eltid)))
    {
        return (-1);
    }
    else
    {
        // This test is to see if p1 > p2.
        if ((x.personid > y.personid) || ((x.personid == y.personid) && (x.eltid > y.eltid)))
        {
            return (1);
        }
        else
        {
            // They're the same (x.user = y.user AND x.element = y.element).
            return (0);
        }
    }
} // end eventcmp()


int main()
{
    // Set up logging.
    openlog("ratgen", LOG_PID, LOG_LOCAL1);
    setlogmask(LOG_UPTO (LOG_INFO));

    // Declare time variables:
    long long start, finish;

    syslog(LOG_INFO, "***Starting ratgen main");

    // Record the start time:
    start = current_time_millis();
        
    load_config_file();

    int i, j, event_count, max_event_count, rating = 0;
    double event_ratio;
    int personid;
        
    FILE *fp;
    char fname[255];

    // Do a wc on the input file events.txt to get number of lines
    char shell_cmds[4096];
    char bfr[BUFSIZ];
    uint32_t num_lines = 0;
    uint32_t num_found = 0;

    //  wc -l  events.txt | cut -d ' ' -f 1
    strlcpy(shell_cmds, "wc -l ", sizeof(shell_cmds));
    strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
    strlcat(shell_cmds, "/events.txt | cut -d ' ' -f 1", sizeof(shell_cmds));
    syslog(LOG_INFO, "shell_cmds is ---%s---", shell_cmds);
    if ((fp = popen(shell_cmds, "r")) == NULL)
    {
        // There was an error on popen.
        perror("Some problem opening popen stream 0. Bailing in ratgen.");
        exit(1);
    }

    // Read 1 line of results.
    while (fgets(bfr, BUFSIZ, fp) != NULL)
    {
        num_lines = atoi(bfr);
    } // end while

    syslog(LOG_INFO, "num_lines is %d", num_lines);
    if (0 == num_lines)
    {
        syslog(LOG_ERR, "Couldn't get the num_lines of input file %s. Exiting.", fname);
        exit (-1);
    }

    pclose(fp);

    // Set up events input file.
    strlcpy(fname, BE.events_file, sizeof(fname));
    fp = fopen(fname,"r");

    if (NULL == fp)
    {
        syslog(LOG_ERR, "ERROR: cannot open input file %s", fname);
        exit(1);
    }

    // Malloc num_lines event_t's
    event_t *events;
    events = (event_t *) malloc(num_lines * sizeof(event_t));
    if (events == NULL)
    {
        syslog(LOG_ERR, "Ran out of RAM when allocating events structure with %d elts.", num_lines);
        exit(-1);
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    const char delimiter[2] = "\t";
    char *token;

    // Read the input file and populate the event_t's.
    while ((line_length = getline(&line, &line_capacity, fp)) != -1)
    {
        // Is the file coming from linux/unix?
        if (line_length && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
            line[--line_length] = '\0';
        // Is the file coming from windows?
        if (line_length && (line[line_length - 1] == '\r'))
            line[--line_length] = '\0';

        int token_number = 0;

        // Get the first token.
        token = strtok(line, delimiter);

        // Walk through other tokens.
        while( token != NULL )
        {
            switch (token_number)
            {
                case 0:   // personid
                    events[num_found].personid = atoi(token);
                    break;
                case 1:   // eltid
                    events[num_found].eltid = atoi(token);
                    break;
                default:
                    syslog(LOG_ERR, "ERROR: hit the default case situation when parsing tokens in a line in %s. Why?", fname);
            }
            token = strtok(NULL, delimiter);
            token_number++;
        } // end while() parsing tokens

        num_found++;
        if (num_found > num_lines)
        {
            syslog(LOG_ERR, "Found more events than we allocated. %d > %d. Exiting.", num_found, num_lines);
            exit(-1);
        }
    } // end while more lines to get

    syslog(LOG_INFO, "We loaded %u events and were expecting %u", num_found, num_lines);

    if (fclose(fp) != 0)
    {
        syslog(LOG_ERR, "Error closing events input file. Exiting.");
        exit(-1);
    }

    // Sort events by personid, then productid
    qsort(events, num_found, sizeof(event_t), eventcmp);

    // Set up ratings output file.
    strlcpy(fname, BE.ratings_file, sizeof(fname));
    fp = fopen(fname,"w");

    if (NULL == fp)
    {
        syslog(LOG_ERR, "ERROR: cannot open output file %s", fname);
        exit(1);
    }

    // Walk the events and count the quantities of items for each person.
    i = 0;
    while (i < num_found)
    {
        // Find out how many freq_t guys to allocate for this person.
        // NOTE: This allocation will be larger that what's necessary in every case except when all eltids are unique for this person
        personid = events[i].personid;
        int walker = i;
        int num_to_allocate = 0;
        while (events[walker++].personid == personid)
            num_to_allocate++;

        // Set up the frequency structure for this person.
        freq_t *freqs;
        freqs = (freq_t *) calloc(num_to_allocate, sizeof(freq_t));

        // Populate freqs for this person. eltids are sorted.
        int freq_ind = -1;
        int curr_elt = 0;
        for (j = 0; j < num_to_allocate; j++)
        {
            // Do we have a new elt?
            if (curr_elt != events[i + j].eltid)
            {
                freq_ind++;
                curr_elt = events[i + j].eltid;

                // New elt, so set the eltid at the new freq location
                freqs[freq_ind].eltid = events[i + j].eltid;
            }
            // Bump the freq count
            freqs[freq_ind].freq++;
        } // end for loop across events for this person

        i += j;
        max_event_count = 0;

        // 1b. Iterate over all distinct eltids evented by this person to get the max_event_count.
        for (j = 0; j < freq_ind + 1; j++)
        {
            if (freqs[j].freq > max_event_count)
                max_event_count = freqs[j].freq;
        }

        // 1b. Iterate over all distinct eltids evented by this person again.
        for (j = 0; j < freq_ind + 1; j++)
        {
            event_count = freqs[j].freq;

            // 1.b.1 find the (event_count / max_event_count) for each plu
            event_ratio = (float) event_count / (float) max_event_count;

            // 1.b.2 put that ratio into lookup table for the 32 ratings bands
            for (int k = 0; k < 32; k++)
            {
                if (event_ratio < g_ratio_thresh[k])
                {
                    rating = k + 1;
                    break;
                }
            }

            // 1.b.3 We now have a rating for that person / element

            // 1.b.4 Persist the rating info.
            // Export to flat file: tab-delimited personid, productid, rating
            char out_buffer[256];
            sprintf(out_buffer, "%d\t%u\t%d\n", personid, freqs[j].eltid, rating);
            fwrite(out_buffer, strlen(out_buffer), 1, fp);

        } // end for loop across distinct elements for this person

        if (NULL != freqs)
            free(freqs);

    } // end for loop across distinct people

    if (NULL != events)
        free(events);

    if (fclose(fp) != 0)
    {
        syslog(LOG_ERR, "Error closing ratings output file.");
        exit(1);
    }

    // Record the end time:
    finish = current_time_millis();
        
    syslog(LOG_INFO, "ratgen took %lld millis", finish - start);
    syslog(LOG_INFO, "***Done with ratgen!");

    return (0);
} // end ratgen main()

