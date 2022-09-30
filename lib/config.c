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

    // Ok, now we have bool rmeta_exists to tell us if the rmetacache.txt exists.
    // If there's no cache, we should skip the next popen and set use_cache to false
    if (rmeta_exists == false)
    {
        use_cache = false;
        goto cache_check;
    }

    // Check if the ratings file's timestamp is different to what we might already know about.
    // Get 2 things and compare them here in our new C-land.
    // head -n 1 rmetacache.txt; stat ratings.txt | grep 'Modify: ' | cut -d ' ' -f 2,3,4

    strlcpy(shell_cmds, "head -n 1 ", sizeof(shell_cmds));
    strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
    strlcat(shell_cmds, "/rmetacache.txt; stat ", sizeof(shell_cmds));
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
            // bfr should have the string of possible cachesd timestamp
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

cache_check:

    // if we can use the cache, read the values into the BE.
    // tail -3 rmetacache.txt
    if (use_cache)
    {
        strlcpy(shell_cmds, "tail -3 ", sizeof(shell_cmds));
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
        int counter = 0;
        while (fgets(bfr,BUFSIZ,fp) != NULL)
        {
            switch (counter)
            {
                case 0:
                    // bfr should have the number of people
                    BE.num_people = atoi(bfr);
                    printf("number of people: %lu\n", BE.num_people);
                    syslog(LOG_INFO, "BE.num_people is %lu", BE.num_people);
                break;
                case 1:
                    // bfr should have the number of elts
                    BE.num_elts = atoi(bfr);
                    printf("number of elements: %lu\n", BE.num_elts);
                    syslog(LOG_INFO, "BE.num_elts is %lu", BE.num_elts);
                break;
                case 2:
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
    else
    {
        // We can't use cached metadata because it's not there or out of date so create the metadata and the cache.
        // Use popen to get the results of shell commands to get num_people, num_elts, num_ratings.
        // Get num_people, num_elts, num_ratings from ratings file.
        // Try to allow for comma delimited, too.
        // orig sort -g ratings.txt | cut -f 1 | uniq | wc -l; cut -f 2,3 ratings.txt | sort -g | cut -f 1 | uniq | wc -l; wc ratings.txt -l | cut -d ' ' -f 1
        //      sort -g ratings.out | cut -f 1 | cut -d, -f 1 | uniq | wc -l;
        //      cut -f 2,3 ratings.out | cut -d, -f 2,3 | sort -g | cut -f 1 | cut -d, -f 1 | uniq | wc -l;
        //      wc ratings.out -l | cut -d ' ' -f 1

        // In this situation we need to generate new valences. But we don't know who called us.
        // We could be in a bad place if recgen called us and we set num_elts based on new ratings file, but valences are old.
        // Maybe delete the valences.out file?
        // Invalidate (remove) valences.out, valence_cache, num_confident_valences.out

        // Here we're creating the timestamp & cache.
        strlcpy(shell_cmds, "stat ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " | grep 'Modify: ' | cut -d' ' -f2,3,4 > ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt; sort -g ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " | cut -f 1 | cut -d, -f 1 | uniq | wc -l | tee -a ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt; cut -f 2,3 ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " | cut -d, -f 2,3 | sort -g | cut -f 1 | cut -d, -f 1 | uniq | wc -l | tee -a ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt; wc ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.ratings_file, sizeof(shell_cmds));
        strlcat(shell_cmds, " -l | cut -d ' ' -f 1 | tee -a ", sizeof(shell_cmds));
        strlcat(shell_cmds, BE.working_dir, sizeof(shell_cmds));
        strlcat(shell_cmds, "/rmetacache.txt", sizeof(shell_cmds));

        if ((fp = popen(shell_cmds, "r")) == NULL)
        {
            // There was an error on popen.
            perror("Some problem opening popen stream 3. Bailing in bmh-config");
            exit(1);
        }

        // Read 3 lines of results
        counter = 0;
        while (fgets(bfr,BUFSIZ,fp) != NULL)
        {
            switch (counter)
            {
                case 0:
                    // bfr should have the number of people
                    BE.num_people = atoi(bfr);
                    printf("number of people: %lu\n", BE.num_people);
                    syslog(LOG_INFO, "BE.num_people is %lu", BE.num_people);
                break;
                case 1:
                    // bfr should have the number of elts
                    BE.num_elts = atoi(bfr);
                    printf("number of elements: %lu\n", BE.num_elts);
                    syslog(LOG_INFO, "BE.num_elts is %lu", BE.num_elts);
                break;
                case 2:
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
                        syslog(LOG_INFO, "BE.num_people is %lu", BE.num_people);
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
