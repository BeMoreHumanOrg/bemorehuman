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

bemorehumanConfig_t BE;                       // global storage for the bemorehuman config variables

// begin rmetacache creation helpers
#define INITIAL_CAPACITY 1000
#define MAX_LINE 1000

int compare(const void *a, const void *b)
{
    return (*(int*)a - *(int*)b);
} // end compare()

unsigned long count_unique(int *arr, unsigned long size)
{
    unsigned long unique_count = 0;
    for (unsigned long i = 0; i < size; i++)
        if (i == 0 || arr[i] != arr[i-1])
            unique_count++;
    return unique_count;
} // end count_unique()

void *safe_realloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL)
    {
        perror("Memory reallocation failed. Exiting.");
        free(ptr);
        exit(1);
    }
    return new_ptr;
} // end safe_realloc()

// Custom function to parse integers from a string
int parse_integers(const char* line, int* first_integer, int* second_integer, int location)
{
    int count = 0;
    const char* p = line;

    while (count < 2)
    {
        // Skip leading whitespace and commas
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }

        // Check if we've reached the end of the line
        if (*p == '\0' || *p == '\n') {
            break;
        }

        // Parse the integer
        int value = 0;
        while (*p >= '0' && *p <= '9')
        {
            value = value * 10 + (*p - '0');
            p++;
        }

        if (count == 0)
            first_integer[location] = value;
        else
            second_integer[location] = value;

        count++;
    }
    return count;
} // end parse_integers()

// end rmetacache creation helpers


void load_config_file()
{
    FILE *fp;
    char bufr[MAX_CONFIG_LINE_SIZE], item[ITEM_SIZE], value[VALUE_SIZE], conffile[PATH_SIZE];
    char *token;

    // set up logging
    openlog("bmh", LOG_PID | LOG_NDELAY, LOG_LOCAL0);
    setlogmask(LOG_UPTO (LOG_INFO));

    strcpy(conffile, DEFAULT_CONFIG_FILE);

    printf("Setting up environment...\n");

    if ((fp = fopen(conffile, "r")) != NULL)
    {
        while (fgets(bufr, sizeof(bufr), fp) != NULL)
        {
            token = strtok(bufr, "\t =\n\r");
            if (token != NULL && token[0] != '#')
            {
                // check the sizes of the item/value
                if (strlen(token) < (ITEM_SIZE - 1))
                    strlcpy(item, token, sizeof(item));
                else
                {
                    perror("Item token too big. Exiting");
                    exit(1);
                }
                token = strtok(NULL, "\t =\n\r");
                if (strlen(token) < (VALUE_SIZE - 1))
                    strlcpy(value, token, sizeof(value));
                else
                {
                    perror("Value token too big. Exiting");
                    exit(1);
                }

                // filesystem paths
                if (!strcmp(item, "events_file"))
                {
                    // Just get the string & don't validate it much.
                    if (strlen(value) >= PATH_SIZE)  // too big!
                    {
                        perror("events_file field needs to be max (PATH_SIZE - 1) characters. Exiting bmh-config");
                        exit(1);
                    }
                    strlcpy(BE.events_file, value, sizeof(BE.events_file));
                }
                if (!strcmp(item, "bmh_events_file"))
                {
                    // Just get the string & don't validate it much.
                    if (strlen(value) >= PATH_SIZE)  // too big!
                    {
                        perror("bmh_events_file field needs to be max (PATH_SIZE - 1) characters. Exiting bmh-config");
                        exit(1);
                    }
                    strlcpy(BE.bmh_events_file, value, sizeof(BE.bmh_events_file));
                }
                if (!strcmp(item, "ratings_file"))
                {
                    // just get the string & don't validate it much
                    if (strlen(value) >= PATH_SIZE)  // too big!
                    {
                        perror("ratings_file field needs to be max (PATH_SIZE - 1) characters. Exiting bmh-config");
                        exit(1);
                    }
                    strlcpy(BE.ratings_file, value, sizeof(BE.ratings_file));
                }
                if (!strcmp(item, "working_dir"))
                {
                    // just get the string & don't validate it much
                    if (strlen(value) >= PATH_SIZE)  // too big!
                    {
                        perror("working_dir field needs to be max (PATH_SIZE - 1) characters. Exiting bmh-config");
                        exit(1);
                    }
                    strlcpy(BE.working_dir, value, sizeof(BE.working_dir));
                }
                if (!strcmp(item, "valence_cache_dir"))
                {
                    // just get the string & don't validate it much
                    if (strlen(value) >= PATH_SIZE)  // too big!
                    {
                        perror("valence_cache_dir field needs to be max (PATH_SIZE - 1) characters. Exiting bmh-config");
                        exit(1);
                    }
                    strlcpy(BE.valence_cache_dir, value, sizeof(BE.valence_cache_dir));
                }
                if (!strcmp(item, "recgen_socket_location"))
                {
                    // just get the string & don't validate it much
                    if (strlen(value) >= PATH_SIZE)  // too big!
                    {
                        perror("recgen_socket_location field needs to be max (PATH_SIZE - 1) characters. Exiting bmh-config");
                        exit(1);
                    }
                    strlcpy(BE.recgen_socket_location, value, sizeof(BE.recgen_socket_location));
                }
            } // end if it's a token
        } // while more lines in config file
    } // end if we can open the config file
    else
    {
        // There was an error on config file open.
        perror("Some problem opening the config file. Bailing in bmh-config");
        exit(1);
    }

    fclose(fp);

    char rm_fname[256], rat_fname[256];
    sprintf(rm_fname, "%s/%s", BE.working_dir, "rmetacache.txt");
    sprintf(rat_fname, "%s/%s", BE.working_dir, "ratings.out");

    char shell_cmds[4096], cached_tstamp[1024], cur_tstamp[1024];
    char bfr[BUFSIZ];
    bool use_cache = false;

    // Does the metafile exist at all?
    bool rmeta_exists = false;
    strlcpy(shell_cmds, "test -f ", sizeof(shell_cmds));
    strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
    strlcat(shell_cmds, "/rmetacache.txt; echo $?", sizeof(shell_cmds));
    if ((fp = popen(shell_cmds, "r")) == NULL)
    {
        // There was an error on popen.
        perror("Some problem opening popen stream 0. Bailing in bmh-config");
        exit(1);
    }
    // Read 1 line of results.
    int counter = 0;
    while (fgets(bfr,BUFSIZ,fp) != NULL)
    {
        switch (counter)
        {
            case 0:
            // bfr should have a 1 if no cache file, and 0 if one exists
            rmeta_exists = 1 - atoi(bfr);
            syslog(LOG_INFO, "rmeta_exists is %d", rmeta_exists);
            break;

            default:
            break;
        } // end switch
        counter++;
    } // end while

    fclose(fp);

#ifdef __linux__
    // What is the numerical sort character option?
    char num_sort_char[2] = "g";

    // Do we need to add a flag to stat? On Linux no, on NetBSD yes.
    char stat_flag[3] = "";
#endif

#ifdef __NetBSD__
    // What is the numerical sort character option?
    char num_sort_char[2] = "n";

    // Do we need to add a flag to stat? On Linux no, on NetBSD yes.
    char stat_flag[4] = "-x ";
#endif

#ifdef __APPLE__
    // What is the numerical sort character option?
    char num_sort_char[2] = "g";

    // Do we need to add a flag to stat? On Linux no, on NetBSD yes.
    char stat_flag[4] = "-x ";
#endif

    // Ok, now we have bool rmeta_exists to tell us if the rmetacache.txt exists.
    // If there's no cache, we should skip the next popen and set use_cache to false
    if (rmeta_exists == false)
    {
        use_cache = false;
        goto cache_check;
    }

// If we're not on Apple, use the shell method. Hopefully we can get rid of shell method errywhere.
#ifndef __APPLE__

    // Check if the ratings file's timestamp is different to what we might already know about.
    // Get 2 things and compare them here in our new C-land.
    // head -n 1 rmetacache.txt; stat ratings.txt | grep 'Modify: ' | cut -d ' ' -f 2,3,4

    strlcpy(shell_cmds, "head -n 1 ", sizeof(shell_cmds));
    strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
    strlcat(shell_cmds, "/rmetacache.txt; stat ", sizeof(shell_cmds));
    strlcat(shell_cmds, stat_flag, sizeof(shell_cmds));
    strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
    strlcat(shell_cmds, " | grep 'Modify: ' | cut -d ' ' -f 2,3,4", sizeof(shell_cmds));

    if ((fp = popen(shell_cmds, "r")) == NULL)
    {
        // There was an error on popen.
        perror("Some problem opening popen stream 1. Bailing in bmh-config");
        exit(1);
    }

    // Read 2 lines of results
    counter = 0;
    while (fgets(bfr,BUFSIZ,fp) != NULL)
    {
        switch (counter)
        {
            case 0:
            // bfr should have the string of possible cached timestamp
            strlcpy(cached_tstamp, bfr, sizeof(cached_tstamp));
            break;

            case 1:
            // bfr should have the string of current ratings file timestamp
            strlcpy(cur_tstamp, bfr, sizeof(cur_tstamp));
            break;

            default:
            break;
        } // end switch
        counter++;
    } // end while

    // Now compare those 2 guys
    if ((!strcmp(cached_tstamp, cur_tstamp)) && strlen(cur_tstamp) > 0)
    {
        // They're equal so it's safe to read the cached values.
        use_cache = true;
    }

    fclose(fp);

#else

    // Do Apple-specific stuff here

    // Get first line from rmetacache.
    char line[128];
    FILE *fptr;
    if ((fptr = fopen(rm_fname, "r")) == NULL)
    {
        perror("Error: rmetacache file cannot be opened. Exiting.");
        exit(1);
    }
    fgets(line, 128, fptr);
    fclose(fptr);

    // Get last modified time from ratings.out
    struct stat file_stat;

    if (stat(rat_fname, &file_stat) == 0)
    {
        // Compare the two strings
        if (!strcmp(line, ctime(&file_stat.st_mtime)) && strlen(line) > 0)
            use_cache = true;
    }
    else
    {
        perror("Error getting file stats for ratings.out");
        exit(1);
    }

#endif // ifndef __APPLE__

cache_check:

#ifndef __APPLE__

    // if we can use the cache, read the values into the BE.
    // head -4 rmetacache.txt
    if (use_cache)
    {
        strlcpy(shell_cmds, "head -4 ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt", sizeof(shell_cmds));
        if ((fp = popen(shell_cmds, "r")) == NULL)
        {
            // There was an error on popen.
            perror("Some problem opening popen stream 2. Bailing in bmh-config");
            exit(1);
        }

        syslog(LOG_INFO, "Reading some BE vars from cache.");

        // Read 3 lines of results
        counter = 0;
        while (fgets(bfr,BUFSIZ,fp) != NULL)
        {
            switch (counter)
            {
                case 0:
                    // ignore the first line which is a timestamp
                    break;
                case 1:
                    // bfr should have the number of people
                    BE.num_people = atoi(bfr);
                    printf("number of people: %lu\n", BE.num_people);
                    syslog(LOG_INFO, "BE.num_people is %lu", BE.num_people);
                    break;
                case 2:
                    // bfr should have the number of elts
                    BE.num_elts = atoi(bfr);
                    printf("number of elements: %lu\n", BE.num_elts);
                    syslog(LOG_INFO, "BE.num_elts is %lu", BE.num_elts);
                    break;
                case 3:
                    // bfr should have the number of ratings
                    BE.num_ratings = atoi(bfr);
                    printf("number of ratings: %lu\n", BE.num_ratings);
                    syslog(LOG_INFO, "BE.num_ratings is %lu", BE.num_ratings);
                    break;

                default:
                    break;
            } // end switch
            counter++;
        } // end while

        fclose(fp);
    } // end if it's ok to use cache

#else

    // Do Apple-specific stuff here
    // if we can use the cache, read the values into the BE.
    // head -4 rmetacache.txt
    if (use_cache)
    {
        if ((fptr = fopen(rm_fname, "r")) == NULL)
        {
            perror("Error! rmetacache text file cannot be opened. Exiting.");
            exit(1);
        }

        syslog(LOG_INFO, "Reading some BE vars from cache file.");

        // Read text until newline is encountered and store it.
        counter = 0;
        while (fgets(line,128,fptr) != NULL)
        {
            switch (counter)
            {
                case 0:
                    // ignore the first line which is a timestamp
                    break;
                case 1:
                    // line should have the number of people
                    BE.num_people = atoi(line);
                    printf("number of people: %" PRIu64 "\n", BE.num_people);
                    syslog(LOG_INFO, "BE.num_people is %" PRIu64, BE.num_people);
                    break;
                case 2:
                    // line should have the number of elts
                    BE.num_elts = atoi(line);
                    printf("number of elements: %" PRIu64 "\n", BE.num_elts);
                    syslog(LOG_INFO, "BE.num_elts is %" PRIu64, BE.num_elts);
                    break;
                case 3:
                    // line should have the number of ratings
                    BE.num_ratings = atoi(line);
                    printf("number of ratings: %" PRIu64 "\n", BE.num_ratings);
                    syslog(LOG_INFO, "BE.num_ratings is %" PRIu64, BE.num_ratings);
                    break;
                default:
                    break;
            } // end switch
            counter++;
        } // end while

        fclose(fptr);
    } // end if use_cache

#endif // #ifndef __APPLE__

    else
    {
#ifndef __APPLE__
        // We can't use cached metadata because it's not there or out of date so create the metadata and the cache.
        // Use popen to get the results of shell commands to get num_people, num_elts, num_ratings.
        // Get num_people, num_elts, num_ratings from ratings file.
        // Try to allow for comma delimited, too.
        //      sort -g ratings.out | cut -f 1 | cut -d, -f 1 | uniq | wc -l;
        //      cut -f 2,3 ratings.out | cut -d, -f 2,3 | sort -g | cut -f 1 | cut -d, -f 1 | uniq | wc -l;
        //      wc -l ratings.out | cut -d ' ' -f 1

        // In this situation we need to generate new valences. But we don't know who called us.
        // We could be in a bad place if recgen called us, and we set num_elts based on new ratings file, but valences are old.
        // Invalidate (remove) valences.out, valence_cache, num_confident_valences.out


        // Here we're creating the timestamp & cache.
        strlcpy(shell_cmds, "stat ", sizeof(shell_cmds));
        strlcat(shell_cmds, stat_flag, sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " | grep 'Modify: ' | cut -d' ' -f2,3,4 > ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt; sort -", sizeof(shell_cmds));
        strlcat(shell_cmds, num_sort_char, sizeof(shell_cmds));
        strlcat(shell_cmds, " ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " | cut -f 1 | cut -d, -f 1 | uniq | wc -l | sed -e 's/^[ \t]*//' | tee -a ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt; cut -f 2,3 ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " | cut -d, -f 2,3 | sort -", sizeof(shell_cmds));
        strlcat(shell_cmds, num_sort_char, sizeof(shell_cmds));
        strlcat(shell_cmds, " | cut -f 1 | cut -d, -f 1 | uniq | wc -l | sed -e 's/^[ \t]*//' | tee -a ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt; wc -l ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " | sed -e 's/^[ \t]*//' | cut -d ' ' -f 1 | tee -a ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt", sizeof(shell_cmds));
        syslog(LOG_INFO, "shell_cmds is ---%s---", shell_cmds);

        if ((fp = popen(shell_cmds, "r")) == NULL)
        {
            // There was an error on popen.
            perror("Some problem opening popen stream 3. Bailing in bmh-config");
            printf("ERROR: FP is broken in config.c in bmhlib. Exiting.\n");
            exit(1);
        }
        // Read 3 lines of results
        counter = 1;
#else
        // Create the rmetacache.
        if ((fp = fopen(rm_fname, "w")) == NULL)
        {
            // There was an error on rmetacache file open.
            printf("Can't open %s for writing.\n", rm_fname);
            perror("Some problem creating the rmetacache file. Bailing in bmh-config.");
            exit(1);
        }

        // Do the stat of ratings.out, looking for Modify time. Put that in rmetacache.txt.
        if (stat(rat_fname, &file_stat) == 0)
        {
            fputs(ctime(&file_stat.st_mtime), fp);
        }
        else
        {
            perror("Error getting file stats for ratings.out. Exiting.");
            exit(1);
        }

        // begin better ratings file parser
        FILE *file = fopen(rat_fname, "r");
        if (file == NULL)
        {
            perror("Error opening ratigs file. Exiting.");
            exit(1);
        }

        unsigned long capacity = INITIAL_CAPACITY;
        unsigned long count = 0;
        int *first_column = malloc(capacity * sizeof(int));
        int *second_column = malloc(capacity * sizeof(int));

        if (first_column == NULL || second_column == NULL)
        {
            perror("Initial ratings file read memory allocation failed. Exiting.");
            free(first_column);
            free(second_column);
            fclose(file);
            exit(1);
        }

        while (fgets(line, sizeof(line), file))
        {
            if (count >= capacity)
            {
                capacity *= 2;
                first_column = safe_realloc(first_column, capacity * sizeof(int));
                second_column = safe_realloc(second_column, capacity * sizeof(int));
            }

            // Use a custom parser instead of strtok and atoi
            int num_int = parse_integers(line, first_column, second_column, count);
            count++;
        }

        fclose(file);

        qsort(first_column, count, sizeof(int), compare);
        qsort(second_column, count, sizeof(int), compare);

        unsigned long unique_first = count_unique(first_column, count);
        unsigned long unique_second = count_unique(second_column, count);

        printf("Unique people in first column: %lu\n", unique_first);
        printf("Unique items in second column: %lu\n", unique_second);
        printf("Total ratings read: %lu\n", count);

        free(first_column);
        free(second_column);

        // end better ratings file parser

        // Get the number of people and put in rmetacache.
        char result[10];
        sprintf(result, "%lu\n", unique_first);
        fputs(result, fp);

        // Get the number of elements and put in rmetacache.
        sprintf(result, "%lu\n", unique_second);
        fputs(result, fp);

        // Get the number of ratings and put in rmetacache.
        sprintf(result, "%lu\n", count);
        fputs(result, fp);

        // Close the rmetacache.
        fclose(fp);


        // Open the rmetacache
        if ((fp = fopen(rm_fname, "r")) == NULL)
        {
            // There was an error on rmetacache file open.
            perror("Some problem opening the rmetacache file. Bailing in bmh-config.");
            exit(1);
        }
        // Read 4 lines of results, throw away the first.
        counter = 0;
#endif

        while (fgets(bfr,BUFSIZ,fp) != NULL)
        {
            switch (counter)
            {
                case 0:
                    break;
                case 1:
                    // bfr should have the number of people
                    BE.num_people = atoi(bfr);
                    printf("number of people from rmc: %" PRIu64 "\n", BE.num_people);
                    syslog(LOG_INFO, "BE.num_people is %" PRIu64, BE.num_people);
                    break;
                case 2:
                    // bfr should have the number of elts
                    BE.num_elts = atoi(bfr);
                    printf("number of elements from rmc: %" PRIu64 "\n", BE.num_elts);
                    syslog(LOG_INFO, "BE.num_elts is %" PRIu64, BE.num_elts);
                    break;
                case 3:
                    // bfr should have the number of ratings
                    BE.num_ratings = atoi(bfr);
                    printf("number of ratings from rmc: %" PRIu64 "\n", BE.num_ratings);
                    syslog(LOG_INFO, "BE.num_ratings is %" PRIu64, BE.num_ratings);
                    break;

                default:
                    break;
            } // end switch
            counter++;
        } // end while

        fclose(fp);

        // Ok, now delete a bunch of stuff
        // cd valences_dir; rm valences.out valence_cache/* num_confident_valences.out; rmdir valence_cache
        strlcpy(shell_cmds, "cd ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "; rm valences.out valence_cache/* num_confident_valences.out; rmdir valence_cache", sizeof(shell_cmds));

        printf("Because there's a new ratings file, we need to invalidate (remove) the various valence files.\n");
        printf("Now removing old valences.out, num_confident_valences.out, and valence_cache dir.\n");
        syslog(LOG_INFO, "Because there's a new ratings file, we're removing the old valences.out, num_confident_valences.out, and valence cache dir.");

        if ((fp = popen(shell_cmds, "r")) == NULL)
        {
            // There was an error on popen.
            perror("Some problem opening popen stream 3. Bailing in bmh-config");
            exit(1);
        }

        // Read hopefully 0 lines of results
        counter = 0;
        int len;
        while (fgets(bfr,BUFSIZ,fp) != NULL)
        {
            switch (counter)
            {
                case 0:
                    // bfr should have hopefully nothing if the rm's went ok
                    len = strlen(bfr);
                    if (len > 0)
                    {
                        printf("output from valence files removal is: ---%s---\n", bfr);
                        syslog(LOG_INFO, "BE.num_people is %" PRIu64, BE.num_people);
                    }
                break;

                default:
                break;
            } // end switch
            counter++;
        } // end while

        fclose(fp);




    } // end else we can't use the cache

    // Try printing it out to verify that it loaded ok.
    /*
    char outline[512];
    sprintf(outline, "BE.num_elts is ---%d---\n", BE.num_elts);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.num_ratings is ---%d---\n", BE.num_ratings);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.num_people is ---%d---\n", BE.num_people);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.events_file is ---%s---\n", BE.events_file);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.bmh_events_file is ---%s---\n", BE.bmh_events_file);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.ratings_file is ---%s---\n", BE.ratings_file);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.working_dir is ---%s---\n", BE.working_dir);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.valence_cache_dir is ---%s---\n", BE.valence_cache_dir);
    fprintf(stdout, "%s", outline);
    sprintf(outline, "BE.recgen_socket_location is ---%s---\n", BE.recgen_socket_location);
    fprintf(stdout, "%s", outline);
    */

} // end load_config_file()

// end config.c
