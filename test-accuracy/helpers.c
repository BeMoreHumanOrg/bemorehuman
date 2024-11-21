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
    struct timeval tv;

    gettimeofday(&tv, 0);

    long long t = tv.tv_sec;
    t = (t * 1000000) + (tv.tv_usec);

    return (t);
} // end current_time_micros()

bool client_initialized = false;  // have we initialized the http client yet?
http_client *client;              // this is the structure for communicating w/server
#define MAX_BUFFER_SIZE 65536

// One-time setup function
void setup_http_client(const char *host, int port)
{
    client = malloc(sizeof(http_client));
    if (client == NULL)
    {
        perror("Failed to allocate memory for http_client. Exiting.");
        exit(1);
    }

    // Create socket
    if ((client->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error. Exiting.");
        free(client);
        exit(1);
    }

    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, host, &client->server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported. Exiting.");
        free(client);
        exit(1);
    }
    printf("host: ---%s---, port ---%d---\n", host, port);
    client_initialized = true;
} // end setup_http_client()


// Cleanup function
void cleanup_http_client()
{
    if (client)
    {
        close(client->socket);
        free(client);
    }
} // end cleanup_http_client()


// Find needle in haystack, in a case-independent manner
char* b_stristr(const char* haystack, const char* needle)
{
    do
    {
        const char* h = haystack;
        const char* n = needle;
        while (tolower((unsigned char) *h) == tolower((unsigned char ) *n) && *n)
        {
            h++;
            n++;
        }
        if (*n == 0)
        {
            return (char *) haystack;
        }
    } while (*haystack++);
    return 0;
} // end b_stristr()


// Content-Length extraction function (from request header)
int extract_content_length(const char *response)
{
    const char *header = "content-length:";
    char *start = b_stristr(response, header);
    if (start)
    {
        start += strlen(header);
        while (isspace(*start)) start++;
        return atoi(start);
    }
    return -1;
} // end extract_content_length()


// This routine calls the bemorehuman server and returns the response.
//
// inputs: int protocol, int scenario, const char *req_body, const size_t req_body_len
// outputs: char **raw_response;
// return: unsigned long number of bytes read in response
unsigned long contact_bemorehuman_server(int protocol, int scenario, const char *req_body,
                                         const size_t req_body_len, char **raw_response)
{
    int port;

    // default is dev
    char server_loc[32] = DEV_SERVER;
    port = DEV_SERVER_PORT;

    // 3 options for server machine: localhost, stage, prod
    if (TEST_LOC_STAGE == g_server_location)
    {
        strcpy(server_loc, STAGE_SERVER);
        port = STAGE_SERVER_PORT;
    }
    if (TEST_LOC_PROD == g_server_location)
    {
        strcpy(server_loc, PROD_SERVER);
        port = PROD_SERVER_PORT;
    }

    if (NULL == req_body)
    {
        syslog(LOG_CRIT, "request body is null. Exiting.");
        exit(1);
    }

    char protocol_string[20];
    if (protocol == PROTOCOL_JSON)
        strcpy(protocol_string, "json");
    else
        strcpy(protocol_string, "octet-stream");

    char path[32] = "/bmh/";
    switch (scenario)
    {
        case SCENARIO_RECS:  strcat(path, "recs");
            break;
        case SCENARIO_EVENT:  strcat(path, "event");
            break;
        case SCENARIO_SINGLEREC:  strcat(path, "internal-singlerec");
            break;
        default: printf("*** Broken scenario value in call_bemorehuman_server! Bad.\n");
            break;
    }

    // Initialize http client if we haven't yet
    if (!client_initialized) setup_http_client(server_loc, port);
    if (client == NULL)
    {
        syslog(LOG_CRIT, "http client is null. Exiting.");
        exit(1);
    }

    // Connect to server
    if (connect(client->socket, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr)) < 0)
    {
        perror("Connection Failed");
        return -1;
    }

    // Prepare HTTP POST request with specific path
    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/%s\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s", path, server_loc, protocol_string, req_body_len, req_body);

    // Send HTTP request
    send(client->socket, request, strlen(request), 0);
    // printf("HTTP request sent:\n%s\n", request);

    // Receive server response
    char buffer[MAX_BUFFER_SIZE] = {0};
    int total_bytes_read = 0;
    int bytes_read;
    char *header_end = NULL;

    // Read response
    while ((bytes_read = read(client->socket, buffer + total_bytes_read, sizeof(buffer) - total_bytes_read - 1)) > 0)
    {
        total_bytes_read += bytes_read;
        buffer[total_bytes_read] = '\0';

        header_end = strstr(buffer, "\r\n\r\n");
        if (header_end)
        {
            break;
        }
    }

    if (header_end == NULL)
    {
        printf("Failed to receive complete headers\n");
        return -1;
    }

    // Calculate header length
    int header_length = header_end - buffer + 4;

    // Extract and print headers
    char headers[MAX_BUFFER_SIZE] = {0};
    strncpy(headers, buffer, header_length);

    // Extract Content-Length
    int content_length_response = extract_content_length(headers);
    if (content_length_response < 0)
    {
        printf("Content-Length header not found\n");
        return -1;
    }

    // Process response body
    char *response_body = malloc(content_length_response + 1);
    if (response_body == NULL)
    {
        perror("Failed to allocate memory for body");
        return -1;
    }

    int body_bytes_read = total_bytes_read - header_length;
    if (body_bytes_read > 0)
    {
        memcpy(response_body, header_end + 4, body_bytes_read);
    }

    while (body_bytes_read < content_length_response)
    {
        bytes_read = read(client->socket, response_body + body_bytes_read, content_length_response - body_bytes_read);
        if (bytes_read <= 0) break;
        body_bytes_read += bytes_read;
    }

    response_body[body_bytes_read] = '\0';

    // printf("Response body:\n%s\n", response_body);

    *raw_response = response_body;

    // Close the connection (important for keeping the server happy).
    close(client->socket);

    // Recreate the socket for the next request
    client->socket = socket(AF_INET, SOCK_STREAM, 0);

    //cleanup
    return (body_bytes_read);
} // end contact_bemorehuman_server()



// end helpers.c
