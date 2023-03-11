// SPDX-FileCopyrightText: 2023 Brian Calhoun <brian@bemorehuman.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <bmh-config.h>
#include "recgen.h"

//
//
// Hum server
//
// This code listens for HTTP POST requests, then translates those to hum protocol and passes the
// request on to the recgen server on the same machine. Hum sends a request as a hum_record with a
// type, content_length, and content. It then reads the response from the recgen server by reading
// the reply hum_record's status, content_length, and content.
//
//

static int g_port = HUM_DEFAULT_PORT;

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


int main(int argc, char *argv[])
{
    // Set up logging.
    openlog(LOG_HUM_STRING, LOG_PID, LOG_LOCAL0);
    setlogmask(LOG_UPTO (HUM_LOG_MASK));
    syslog(LOG_INFO, "*** Begin hum invocation.");

    // Use getopt to help manage the options on the command line.
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
            case 'p':   // for "port number"
                if (strtol(optarg, NULL, 10) < 0 || strtol(optarg, NULL, 10) > 65535)
                {
                    printf("Error: the argument for -p should be > 0 and < 65536 instead of %s. Exiting. ***\n", optarg);
                    syslog(LOG_ERR, "The argument for -b should be > 0 and < 65536, instead of %s. Exiting. ***\n",
                           optarg);
                    exit(EXIT_FAILURE);
                }
                g_port = (int) strtol(optarg, NULL, 10);
                break;
            default:
                printf("Don't understand. Check args. Can only accept -p at the mo' \n");
                fprintf(stderr, "Usage: %s [-p portnum]\n", argv[0]);
                exit(EXIT_FAILURE);
        } // end switch across command-line args
    } // end while we have more command-line args to process


    int server_fd, client_fd;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_size;
    char buffer[HUM_BUFFER_SIZE];

    // Create a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(g_port);

    // Bind the socket to a port
    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error binding socket to port");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) < 0) {
        perror("Error listening for incoming connections");
        exit(EXIT_FAILURE);
    }

    printf("Hum is listening on port %d...\n", g_port);

    while (1) {
        // Accept an incoming connection
        client_address_size = sizeof(client_address);
        client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_size);
        if (client_fd < 0) {
            perror("Error accepting incoming connection");
            continue;
        }

        // Read the request from the client
        int total = 0;
        ssize_t bytes_read;

        // Read data from the socket into the buffer
        bytes_read = read(client_fd, buffer, HUM_BUFFER_SIZE - 1);
        if (bytes_read < 0) {
            perror("read client request from socket");
            exit(1);
        }

        buffer[bytes_read] = '\0';  // Null-terminate the buffer

        // Parse the request to determine the method and URI
        char method[HUM_BUFFER_SIZE];
        char uri[HUM_BUFFER_SIZE];

        sscanf(buffer, "%s %s", method, uri);

        // If the method is "POST", handle the request
        if (strcmp(method, "POST") == 0)
        {
            // Here is the general request message we can expect at this point:

            // POST /path HTTP/1.1\r\n
            // Host: 127.0.0.1
            // User-Agent: curl/7.87.0
            // Content-Type: application/octet-stream\r\n
            // accept: application/octet-stream
            // Content-Length: 5\r\n
            // \r\n
            // post_data

            // Read the POST data from the client. We want the Content-Length & data
            char post_data[HUM_BUFFER_SIZE];
            memset(post_data, 0, HUM_BUFFER_SIZE);

            // Extract the HTTP Content-Length
            int content_length = 0;
            char *p = strstr(buffer, "Content-Length:");
            if (p)
            {
                p += strlen("Content-Length: ");
                sscanf(p, "%d", &content_length);
            }

            // Find where the POST data starts and get rid of everything
            // else prior to that. Nice.
            char *post_start = strstr(p, "\r\n\r\n") ;
            if (post_start)
            {
                post_start += 4;  // skip over the end of header
                total = bytes_read - (post_start - buffer);  // total bytes of post data we've read
            }
            else
            {
                // invalid http request
                exit(-1);
            }

            // This bit was from ChatGPT. I asked it for the fastest way to remove a substring from
            // the beginning of the string.
            //
            size_t sub_len = post_start - buffer;
            memmove(buffer, post_start, bytes_read - sub_len + 1);

            // Read more POST data if we need to
            while (total < content_length)
            {
                /* Read more data from the socket */
                bytes_read = read(client_fd, buffer + total, content_length - total);
                if (bytes_read < 0)
                {
                    perror("read from client socket");
                    exit(1);
                }
                total += bytes_read;
            }

            // If the number of received bytes is the total size of the
            // array then we have run out of space to store the response
            // and it hasn't all arrived yet - so that's a bad thing
            if (total >= HUM_BUFFER_SIZE - 1)
            {
                perror("ERROR storing complete response from socket: out of space in buffer");
                close(client_fd);
                exit(-1);
            }

            char content_len_c[6];
            itoa(content_length, content_len_c);

            // Construct server address and make socket
            int sockfd;
            struct sockaddr_un process_address = {AF_UNIX, "/tmp/bemorehuman/recgen.sock"};
            sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("Error creating socket for connecting to recgen.");
                close(client_fd);
                continue;
            }

            // Connect to the recgen process via a socket
            if (connect(sockfd, (struct sockaddr *)&process_address, sizeof(process_address)) < 0)
            {
                perror("Error connecting to recgen process.");
                close(sockfd);
                close(client_fd);
                continue;
            }

            // Send a hum request
            hum_record record;

            // Send a request along with POST data.

            // First send the REQUEST_URI
            record.type_status = HUM_REQUEST_URI;
            record.content_length = strlen(uri);
            strcpy((char *) &record.content, uri);
            write(sockfd, &record.type_status, sizeof(uint8_t));
            write(sockfd, &record.content_length, sizeof(uint32_t));
            write(sockfd, &record.content, record.content_length);

            // Next send the POST_DATA
            record.type_status = HUM_POST_DATA;
            record.content_length = content_length;
            write(sockfd, &record.type_status, sizeof(uint8_t));
            write(sockfd, &record.content_length, sizeof(uint32_t));
            write(sockfd, &buffer, record.content_length);

            // Read the response from the recgen server
            char response[1024] = "";
            char stderr_resp[1024] = "";
            uint32_t res_content_length = 0;

            while (1)
            {
                res_content_length = 0;

                // Get the type.
                bytes_read = read(sockfd, &record.type_status, sizeof(uint8_t));
                if (bytes_read < 0)
                {
                    perror("Error reading response from recgen process.");
                    close(sockfd);
                    close(client_fd);
                    continue;
                }
                if (bytes_read == 0)
                {
                    // Hmmm. We're done reading. But shouldn't we have stopped with reading
                    // some bytes and a header.type of 3?
                    // xxx Duh. I have to parse the stream to separate the messages.
                    break;
                }
                if (record.type_status == HUM_RESPONSE_ERROR)
                {
                    // Error from recgen
                    perror("Error from inside recgen for some reason.");
                    if (record.content_length > 0)
                    {
                        strncat(response, (char*) record.content, record.content_length);
                        res_content_length += record.content_length;
                    }
                    break;
                }
                else if (record.type_status == HUM_RESPONSE_OK)
                {
                    // Everything's ok and we need to send the response data to the client.
                    // Check for content-length > 0 first!
                    bytes_read = read(sockfd, &record.content_length, sizeof(uint32_t));
                    if (record.content_length > 0)
                    {
                        bytes_read = read(sockfd, &record.content, record.content_length);
                        strncat(response, (char*) record.content, record.content_length);
                        res_content_length += record.content_length;
                    }
                    break;
                }
            } // end while(1)

            // can't print application/octet-stream! if (strlen(response) > 0)
            if (strlen(stderr_resp) > 0)
            {
                fprintf(stderr, "Received stderr response: %s\n", stderr_resp);
            }
            // Send the response from the recgen process back to the client
            char response_header[] = "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: application/octet-stream\r\n"
                                     "Content-Length: ";

            // Coalescing...
            char output[1024];
            sprintf(output, "%s%d\r\n\r\n%s", response_header, res_content_length, response);
            // Get the number of digits in res_content_length.
            int numdigits = 0;
            uint32_t tmp_rcl;
            tmp_rcl = res_content_length;
            while (tmp_rcl != 0)
            {
                tmp_rcl /= 10;
                numdigits++;
            }
            write(client_fd, output, strlen(response_header) + numdigits + 4 + res_content_length);

            // Close the connection with the recgen process' socket
            close(sockfd);

        } // end if method is POST

        // Close the connection with the client
        close(client_fd);
    } // end while(1)

    // Close the server socket
    close(server_fd);

    return 0;
} // end main()
