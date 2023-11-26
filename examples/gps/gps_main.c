/****************************************************************************
 * apps/examples/gps/gps_main.c
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <wchar.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>

#include <minmea/minmea.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MINMEA_MAX_LENGTH    256

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_gps_daemon_started;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sigterm_action
 ****************************************************************************/

static void sigterm_action(int signo, siginfo_t *siginfo, void *arg)
{
  if (signo == SIGTERM)
    {
      printf("SIGTERM received\n");
      g_gps_daemon_started = false;
      printf("gps_daemon: Terminated.\n");
    }
  else
    {
      printf("\nsigterm_action: Received signo=%d siginfo=%p arg=%p\n",
             signo, siginfo, arg);
    }
}


/****************************************************************************
 * gps_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int fd, ret;
  int cnt;
  char ch;
  char line[MINMEA_MAX_LENGTH];
  struct sigaction act;
  pid_t mypid;

  /* SIGTERM handler */

  memset(&act, 0, sizeof(struct sigaction));
  act.sa_sigaction = sigterm_action;
  act.sa_flags     = SA_SIGINFO;

  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGTERM);

  ret = sigaction(SIGTERM, &act, NULL);
  if (ret != 0)
    {
      fprintf(stderr, "Failed to install SIGTERM handler, errno=%d\n",
              errno);
      return (EXIT_FAILURE + 1);
    }

  /* Parse command line */

  if (argc < 2)
    {
      fprintf(stderr, "ERROR: Missing required arguments\n");
      return EXIT_FAILURE;
    }

  if (strncmp(argv[1], "/dev/ttyS", 9) != 0)
    {
      fprintf(stderr, "ERROR: Invalid device name\n");
      return EXIT_FAILURE;
    }

  /* Open the GPS serial port */

  fd = open(argv[1], O_RDONLY);
  if (fd < 0)
    {
      printf("Unable to open file %s\n", argv[1]);
      return EXIT_FAILURE;
    }

  /* Indicate that we are running */

  mypid = getpid();

  g_gps_daemon_started = true;
  printf("\gps_daemon (pid# %d): Running\n", mypid);

  /* Run forever */

  while (g_gps_daemon_started == true)
    {
      /* Read until we complete a line */

      cnt = 0;
      do
        {
          ret = read(fd, &ch, 1);
          if (ret == 1 && ch != '\r' && ch != '\n')
            {
              line[cnt++] = ch;
            }
        }
      while (ch != '\r' && ch != '\n');

      line[cnt] = '\0';

      switch (minmea_sentence_id(line, false))
        {
          case MINMEA_SENTENCE_RMC:
            {
              struct minmea_sentence_rmc frame;

              if (minmea_parse_rmc(&frame, line))
                {
                  printf("Fixed-point Latitude...........: %d\n",
                         minmea_rescale(&frame.latitude, 1000));
                  printf("Fixed-point Longitude..........: %d\n",
                         minmea_rescale(&frame.longitude, 1000));
                  printf("Fixed-point Speed..............: %d\n",
                         minmea_rescale(&frame.speed, 1000));
                  printf("Floating point degree latitude.: %2.6f\n",
                         minmea_tocoord(&frame.latitude));
                  printf("Floating point degree longitude: %2.6f\n",
                         minmea_tocoord(&frame.longitude));
                  printf("Floating point speed...........: %2.6f\n",
                         minmea_tocoord(&frame.speed));
                }
              else
                {
                    printf("$xxRMC sentence is not parsed\n");
                }
            }
            break;

          case MINMEA_SENTENCE_GGA:
            {
              struct minmea_sentence_gga frame;

              if (minmea_parse_gga(&frame, line))
                {
                  printf("Fix quality....................: %d\n",
                         frame.fix_quality);
                  printf("Altitude.......................: %d\n",
                         frame.altitude.value);
                  printf("Tracked satellites.............: %d\n",
                         frame.satellites_tracked);
                }
              else
                {
                  printf("$xxGGA sentence is not parsed\n");
                }
            }
            break;

          case MINMEA_INVALID:
          case MINMEA_UNKNOWN:
          case MINMEA_SENTENCE_GSA:
          case MINMEA_SENTENCE_GLL:
          case MINMEA_SENTENCE_GST:
          case MINMEA_SENTENCE_GSV:
          case MINMEA_SENTENCE_GBS:
          case MINMEA_SENTENCE_VTG:
          case MINMEA_SENTENCE_ZDA:
            {
            }
            break;
        }

        usleep(1 * 1000L);
    }

  /* treats signal termination of the task
   * task terminated by a SIGTERM
   */
  exit(EXIT_SUCCESS);
  close(fd);

  return 0;
}
