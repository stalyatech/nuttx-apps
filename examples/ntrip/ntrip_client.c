/****************************************************************************
 * apps/examples/ntrip/ntrip_client.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ntrip.h"

/****************************************************************************
 * Private macros
 ****************************************************************************/

/* 
 * Sample usage
* 
 * ntripc -s <server> -p <port> -m <mount_point> -i 1 -u <user> -x <pass> -d /dev/ttyS5 -v & 
 * 
 */

const char help[] =
"ntripc -s SERVER -p PORT -m MOUNT ..." \
"  -s SERVER   the IP of the NTRIP server" \
"  -p PORT     the port of the NTRIP server" \
"  -m MOUNT    the mount point or stream of the desired data set" \
"  -d DEVICE   the path of the nmea device" \
"  -u USER     the username used for authenticatation to the NTRIP server" \
"  -x PASSWORD the password used for authenticatation to the NTRIP server" \
"  -i INTERVAL the number of seconds to delay between readings of new NTRIP data" \
"  -v          enable verbose output" \
"  -h          prints this help text";

#define min(a, b) \
  ({ \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; \
  })

/****************************************************************************
 * Private types
 ****************************************************************************/

struct Args 
{
  const char *server;
  const char *device;
  const char *user;
  const char *password;
  const char *mount;
  int  port;
  int  interval;
  bool verbose;
  bool error;
  bool help;
};

struct FD 
{
  const int fd;
  char data[100];
  size_t data_start;
  size_t data_end;
};

enum ReadStatus 
{
  READ_SUCCESS, READ_ERROR, READ_OVERFLOW, READ_EOS
};

struct ReadResult 
{
  const enum ReadStatus status;
  const size_t length;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static struct Args parse_args(int argc, const char **argv) 
{
  struct Args args = 
  {
    .interval = 10
  };

  for (int i = 1; i < argc; i++) 
    {
      if (strcmp(argv[i], "-v") == 0) 
        {
          args.verbose = true;
        } 
      else if (strcmp(argv[i], "-h") == 0) 
        {
          args.help = true;
          args.error = true;
        } 
      else if (i + 1 >= argc) 
        {
          fprintf(stderr, "Too few arguments for option: %s\n", argv[i]);
          args.error = 1;
        } 
      else if (strcmp(argv[i], "-s") == 0) 
        {
          args.server = argv[++i];
        } 
      else if (strcmp(argv[i], "-p") == 0) 
        {
          args.port = (int)strtol(argv[++i], NULL, 10);
          if (args.port <= 0) 
            {
              fprintf(stderr, "Invalid port: %s\n", argv[i]);
              args.error = 1;
            }
        } 
      else if (strcmp(argv[i], "-d") == 0) 
        {
          args.device = argv[++i];
        } 
      else if (strcmp(argv[i], "-u") == 0) 
        {
          args.user = argv[++i];
        } 
      else if (strcmp(argv[i], "-x") == 0) 
        {
          args.password = argv[++i];
        } 
      else if (strcmp(argv[i], "-m") == 0) 
        {
          args.mount = argv[++i];
        } 
      else if (strcmp(argv[i], "-i") == 0) 
        {
          args.interval = (int)strtol(argv[++i], NULL, 10);
          if (args.interval <= 0) 
            {
              fprintf(stderr, "Invalid interval: %s\n", argv[i]);
              args.error = 1;
            }
        } 
      else 
        {
          fprintf(stderr, "Unknown option: %s\n", argv[i]);
        }
    }

  if (!args.help) 
    {
      if (args.server == NULL) 
        {
          fprintf(stderr, "Missing required option: server\n");
          args.error = 1;
        }

      if (args.port <= 0) 
        {
          fprintf(stderr, "Missing required option: port\n");
          args.error = 1;
        }

      if ((args.password == NULL) != (args.user == NULL)) 
        {
          fprintf(stderr, "A username and password must both be specified\n");
          args.error = 1;
        }

      if (args.mount == NULL) 
        {
          fprintf(stderr, "Missing required option: mount\n");
          args.error = 1;
        }
    }

  return args;
}

static size_t base64len(const char input_length) 
{
  return 4 * ((input_length + 2) / 3);
}

static void base64(char *output, const char *input) 
{
  const size_t input_length = strlen(input);

  const char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                 'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                 '4', '5', '6', '7', '8', '9', '+', '/'};

  for (int i = 0, j = 0; i < input_length;) 
    {
      const uint32_t octet_a = i < input_length ? (unsigned char)input[i++] : 0;
      const uint32_t octet_b = i < input_length ? (unsigned char)input[i++] : 0;
      const uint32_t octet_c = i < input_length ? (unsigned char)input[i++] : 0;

      const uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

      output[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
      output[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
      output[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
      output[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

  const int mod_table[] = {0, 2, 1};
  const size_t output_length = base64len(input_length);
  for (int i = 0; i < mod_table[input_length % 3]; i++) 
    {
      output[output_length - 1 - i] = '=';
    }
  output[output_length] = 0;
}

static bool starts_with(const char *str, const char *prefix) 
{
  const size_t str_length = strlen(str);
  const size_t prefix_length = strlen(prefix);
  if (prefix_length > str_length) 
    {
      return false;
    }

  return strncmp(str, prefix, prefix_length) == 0;
}

static bool ends_with(const char *str, const char *suffix) 
{
  const size_t str_length = strlen(str);
  const size_t suffix_length = strlen(suffix);
  if (suffix_length > str_length) 
    {
      return false;
    }

  return strncmp(&str[str_length - suffix_length], suffix, suffix_length) == 0;
}

static bool _read_until_newline(char *charv, size_t *charv_length, bool empty) 
{
  if (charv[*charv_length - 1] == '\n') 
    {
      if (*charv_length >= 2 && charv[*charv_length - 2] == '\r') 
        {
          (*charv_length)--;
        }
      (*charv_length)--;

      return true;
    }

  return false;
}

static bool _read_until_empty(char *charv, size_t *charv_length, bool empty) 
{
  return empty;
}

static struct ReadResult read_until(struct FD *fd, char *charv, const size_t charc, const bool allow_read, bool (condition)(char *, size_t *, bool)) 
{
  struct pollfd pollfds[1] = 
  {
    (struct pollfd){
      .fd = fd->fd,
      .events = POLLIN,
    }
  };

  bool end_of_stream = false;
  size_t chari = 0;
  while (true) 
    {
      if (poll(pollfds, 1, 0) < 0) 
        {
          perror("poll");
          return (struct ReadResult) { .status = READ_ERROR };
        }
      end_of_stream = !(pollfds[0].revents & POLLIN);

      for (size_t i = fd->data_start; i < fd->data_end;) 
        {
          charv[chari++] = fd->data[i++];

          const bool overflow = chari >= charc - 1;
          const bool empty = (i >= fd->data_end) && end_of_stream;
          const bool cond = condition(charv, &chari, empty);
          if (cond || overflow) 
            {
              charv[chari] = 0;
              fd->data_start = i;
              return (struct ReadResult) 
                {
                  .status = !cond && overflow ? READ_OVERFLOW : READ_SUCCESS,
                  .length = chari
                };
            }
        }

      if (end_of_stream && !allow_read) 
        {
          charv[chari] = 0;
          return (struct ReadResult) { .status = READ_EOS, .length = chari };
        }

      fd->data_start = 0;
      fd->data_end = 0;

      ssize_t length = read(fd->fd, fd->data, sizeof(fd->data));
      if (length <= 0) 
        {
          if (length < 0) 
            {
              perror("read");
            }

          return (struct ReadResult) { .status = READ_ERROR };
        }
      fd->data_end = length;
    }
}

static struct ReadResult read_line(struct FD *fd, char *charv, size_t charc, const bool allow_read) 
{
  return read_until(fd, charv, charc, allow_read, _read_until_newline);
}

static struct ReadResult read_until_empty(struct FD *fd, char *charv, size_t charc, const bool allow_read) 
{
  return read_until(fd, charv, charc, allow_read, _read_until_empty);
}

static struct ReadResult read_bytes(struct FD *fd, char *charv, size_t charc) 
{
  size_t chari = 0;

  while (chari < charc && fd->data_start < fd->data_end) 
    {
      charv[chari++] = fd->data[fd->data_start++];
    }

  if (chari < charc) 
    {
      fd->data_start = 0;
      fd->data_end = 0;
    }

  while (chari < charc) 
    {
      ssize_t length = read(fd->fd, &charv[chari], charc - chari);
      if (length <= 0) 
        {
          if (length < 0) 
            {
              perror("read");
            }

          return (struct ReadResult) { .status = READ_ERROR };
        }
      chari += length;
    }
  
  return (struct ReadResult) { .status = READ_SUCCESS, .length = charc };
}

static int write_all(const struct FD *fd, const char *charv, size_t charc) 
{
  for (size_t i = 0; i < charc;) 
    {
      size_t length = write(fd->fd, &charv[i], charc - i);
      if (length <= 0) 
        {
          perror("write");
          return -1;
        }
      i += length;
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, const char **argv) 
{
  const struct Args args = parse_args(argc, argv);
  if (args.help) 
    {
      printf("%s\n", help);
      return 0;
    }

  if (args.error) 
    {
      return 1;
    }

  const struct sockaddr_in serveraddr = 
  {
    .sin_family = AF_INET,
    .sin_addr = { .s_addr = inet_addr(args.server) },
    .sin_port = htons(args.port)
  };

  char auth_header[200] = "";
  if (args.user != NULL && args.password != NULL) 
    {
      char userpass[100];
      snprintf(userpass, sizeof(userpass), "%s:%s", args.user, args.password);
      char b64userpass[base64len(sizeof(userpass) - 1) + 1];
      base64(b64userpass, userpass);
      if (snprintf(auth_header, sizeof(auth_header), "Authorization: Basic %s\r\n", b64userpass) < 0) 
        {
          fprintf(stderr, "User/password too long\n");
          return 2;
        }
    }

  static char request[1000];
  if (snprintf(request,
               sizeof(request),
               "GET /%s HTTP/1.1\r\n"
               "Host: %s:%d\r\n"
               "Ntrip-Version: Ntrip/2.0\r\n"
               "User-Agent: NTRIP tiny-ntrip/1.0.0\r\n"
               "%s"
               "\r\n",
               args.mount,
               args.server,
               args.port,
               auth_header) < 0) 
    {
      fprintf(stderr, "Request too long\n");
      return 2;
    }

  int devfd = -1;
  if (args.device) 
    {
      while (true) 
        {
          devfd = open(args.device, O_RDWR | O_NONBLOCK);
          if (devfd >= 0) 
            {
              break;
            }

          printf("Unable to open device: %s\n", args.device);
          
          const unsigned int delay = 2;
          fprintf(stderr, "Retrying in %u seconds...\n\n", delay);
          sleep(delay);
        }
    }

  struct FD devicefd = { .fd = devfd };

  while (true) 
  {
    if (args.verbose) 
      {
        printf("Connecting to %s:%d\n", args.server, args.port);
      }

    struct FD sockfd = { .fd = socket(AF_INET, SOCK_STREAM, 0) };
    if (sockfd.fd < 0) 
      {
        perror("socket");
        goto retry;
      }

    if (connect(sockfd.fd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr)) < 0) 
      {
        perror("connect");
        goto retry;
      }

    if (args.verbose) 
      {
        printf("Sending request headers\n");
      }

    if (write_all(&sockfd, request, strlen(request)) < 0) 
      {
        goto retry;
      }

    static char line[200];
    if (read_line(&sockfd, line, sizeof(line), true).status == READ_ERROR) 
      {
        goto retry;
      }

    if (!ends_with(line, "200 OK")) 
      {
        fprintf(stderr, "Unexpected response from server: '%s'\n", line);
        goto retry;
      }

    if (args.verbose) 
      {
        printf("Parsing response headers\n");
      }

    bool chunked = false;
    bool header_too_long = false;
    static char header[200];
    header[0] = 0;
    do 
    {
      const enum ReadStatus read_status = read_line(&sockfd, line, sizeof(line), false).status;
      if (read_status == READ_ERROR) 
        {
          goto retry;
        }

      if (!header_too_long && strncat(header, line, sizeof(header) - strlen(header) - 1) < 0) 
        {
          fprintf(stderr, "Header too long: %s\n", header);
          header_too_long = true;
        }

      if (read_status == READ_OVERFLOW) 
        {
          continue;
        }

      if (!ends_with(line, ",")) 
        {
          if (starts_with(header, "Transfer-Encoding: ") && strstr(header, "chunked")) 
            {
              chunked = true;
            }

          header[0] = 0;
          header_too_long = false;
        }
    } while (strlen(line) > 0);

    if (args.verbose && chunked) 
      {
        printf("Using chunked transfer encoding\n");
      }

    while (true) 
    {
      static char buf[1000];

      if (args.verbose) 
        {
          printf("Reading correction data from server\n");
        }

      if (chunked) 
      {
        while (true) 
          {
            do 
            {
              const enum ReadStatus read_status = read_line(&sockfd, buf, sizeof(buf), false).status;
              if (read_status == READ_ERROR) 
                {
                  goto retry;
                } 
              else if (read_status == READ_EOS) 
                {
                  goto chunked_read_complete;
                }
            } while (strlen(buf) == 0);

            int remaining_chunk_size = (int)strtol(buf, NULL, 16);
            if (remaining_chunk_size <= 0) 
              {
                fprintf(stderr, "Unexpected chunk size: %s\n", buf);
                goto retry;
              }

            if (args.verbose) 
              {
                printf("Read chunk of %d bytes%s\n", remaining_chunk_size, devicefd.fd >= 0 ? ", forwarding to device" : "");
              }

            while (remaining_chunk_size > 0) 
              {
                const size_t length = min(remaining_chunk_size, sizeof(buf));
                if (read_bytes(&sockfd, buf, length).status == READ_ERROR) 
                  {
                    goto retry;
                  }
                remaining_chunk_size -= sizeof(buf);

                if (devicefd.fd >= 0) 
                  {
                    write_all(&devicefd, buf, length);
                  }
              }
          }

        chunked_read_complete:;
      } 
      else 
        {
          size_t bytes_read = 0;
          while (true) 
            {
              const struct ReadResult read_result = read_until_empty(&sockfd, buf, sizeof(buf), true);
              if (read_result.status == READ_ERROR) 
                {
                  goto retry;
                }
              bytes_read += read_result.length;

              if (devicefd.fd >= 0) 
                {
                  write_all(&devicefd, buf, read_result.length);
                }

              if (read_result.status != READ_OVERFLOW) 
                {
                  break;
                }
            }

          if (args.verbose) 
            {
              printf("%u bytes were read%s\n", (unsigned int)bytes_read, devicefd.fd >= 0 ? " and forwarded to device" : "");
            }
        }

      if (args.interval >= 0) 
        {
          if (args.verbose) 
            {
              printf("Sleeping for %d seconds\n", args.interval);
            }
          sleep(args.interval);
        }

      if (args.verbose) 
        {
          printf("\n");
        }
    }

retry:
    if (sockfd.fd >= 0 && close(sockfd.fd) < 0) 
      {
        perror("close");
      }

    const unsigned int delay = 10;
    fprintf(stderr, "Retrying in %u seconds...\n\n", delay);
    sleep(delay);
  }

  if (devicefd.fd >= 0 && close(devicefd.fd) < 0) 
    {
      perror("close");
    }
}
