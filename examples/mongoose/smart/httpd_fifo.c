/****************************************************************************
 * apps/examples/mongoose/smart/httpd_fifo.c
 *
 *   Copyright (C) 2007, 2009-2012, 2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Based on uIP which also has a BSD style license:
 *
 *   Copyright (c) 2001, Adam Dunkels.
 *   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Adam Dunkels.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <debug.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <netutils/netlib.h>
#include <mongoose.h>

/****************************************************************************
 * External Functions
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ROOT_FS	 		(&mg_fs_posix)
#define ROOT_DIR    "/data0/www"

#define FIFO_PATH   "/var/gps0"

#define NPOLLFDS    1
#define GPSFIFODX   0

#define POLL_DELAY  1000   /* 1 seconds */

#define NMEA_MAXLEN 128

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Global server options */

struct mg_http_serve_opts g_httpd_opts;

/* Global event manager */

static struct mg_mgr g_evt_mgr;

/* Root directory */

static char g_root_dir[32];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: send_to_ws
 ****************************************************************************/

static size_t send_to_ws(FAR const void *buf, size_t len, int op)
{
  struct mg_mgr *mgr = (struct mg_mgr *) &g_evt_mgr;
  
  /* Traverse over all connections */

  for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) 
    {
      /* Send only to marked connections */

      if (c->data[0] == 'W') 
        {
          return mg_ws_send(c, buf, len, op);
        }
    }

    return 0;
}

/****************************************************************************
 * Name: gpoll_thread
 ****************************************************************************/

static void *gpoll_thread(pthread_addr_t pvarg)
{
  static char buffer[CONFIG_DEV_FIFO_SIZE];
  struct pollfd fds[NPOLLFDS];
  char line[NMEA_MAXLEN];
  int i, fd, cnt, ret;
  ssize_t nbytes;
  bool timeout;
  bool pollin;
  int nevents;
  char ch;

  /* Open the FIFO for non-blocking read */

  fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      printf("gpoll_thread: ERROR Failed to open FIFO %s: %d\n",
             FIFO_PATH, errno);
      close(fd);
      return (FAR void *)-1;
    }

  /* Loop forever */

  while (1)
    {
      memset(fds, 0, sizeof(struct pollfd)*NPOLLFDS);
      fds[GPSFIFODX].fd      = fd;
      fds[GPSFIFODX].events  = POLLIN;
      fds[GPSFIFODX].revents = 0;

      timeout = false;
      pollin  = false;

      /* poll the FIFO */

      ret = poll(fds, NPOLLFDS, POLL_DELAY);

      if (ret < 0)
        {
          printf("gpoll_thread: ERROR poll failed: %d\n", errno);
        }
      else if (ret == 0)
        {
          printf("gpoll_thread: Timeout\n");
          timeout = true;
        }
      else if (ret > NPOLLFDS)
        {
          printf("gpoll_thread: ERROR poll reported: %d\n", errno);
        }
      else
        {
          pollin = true;
        }

      nevents = 0;
      for (i = 0; i < NPOLLFDS; i++)
        {
          if (timeout)
            {
              if (fds[i].revents != 0)
                {
                  printf("gpoll_thread: ERROR expected revents=00, "
                         "received revents[%d]=%08" PRIx32 "\n",
                         i, fds[i].revents);
                }
            }
          else if (pollin)
            {
              if (fds[i].revents == POLLIN)
                {
                  nevents++;
                }
              else if (fds[i].revents != 0)
                {
                  /*
                  printf("gpoll_thread: ERROR unexpected revents[%d]="
                         "%08" PRIx32 "\n", i, fds[i].revents);
                  */
                }
            }
        }

      if (pollin && nevents != ret)
        {
          /*
          printf("gpoll_thread: ERROR found %d events, "
                  "poll reported %d\n", nevents, ret);
          */
        }

      /* In any event, read until the pipe/serial  is empty */

      for (i = 0; i < NPOLLFDS; i++)
        {
          do
            {
              /* The pipe works differently, it returns whatever data
                * it has available without blocking.
                */

              nbytes = read(fds[i].fd, buffer, sizeof(buffer)-1);

              if (nbytes <= 0)
                {
                  if (nbytes == 0 || errno == EAGAIN)
                    {
                      if ((fds[i].revents & POLLIN) != 0)
                        {
                          printf("gpoll_thread: ERROR no read"
                                 " data[%d]\n", i);
                        }
                    }
                  else if (errno != EINTR)
                    {
                      printf("gpoll_thread: read[%d] failed: %d\n",
                             i, errno);
                    }

                  nbytes = 0;
                }
              else
                {
                  if (timeout)
                    {
                      printf("gpoll_thread: ERROR? Poll timeout, "
                              "but data read[%d]\n", i);
                      printf("               (might just be "
                             "a race condition)\n");
                    }

                  /* Read until we complete a line */
                  cnt = 0;
                  for (int j = 0; j < nbytes; j++)
                    {
                      ch = buffer[j];
                      if (ch != '\r' && ch != '\n' && cnt < NMEA_MAXLEN)
                        {
                          line[cnt++] = ch;
                        }

                      if ((ch == '\n') && (cnt > 2))
                        {
                          line[cnt] = '\0';
                          send_to_ws(line, cnt, WEBSOCKET_OP_TEXT);
                          cnt = 0;      
                        }
                    }
                }

              /* Suppress error report if no read data on the next
               * time through
               */

              fds[i].revents = 0;
            }
          while (nbytes > 0);
        }
    }

  /* Won't get here */

  close(fd);
  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/*
 * This RESTful server implements the following endpoints:
 *  /websocket - upgrade to Websocket, and implement websocket echo server
 *  /rest      - respond with JSON string {"result": 123}
 *
 * any other URI serves static files from s_web_root
 */

static void ev_handler(struct mg_connection *c, int ev, void *ev_data) 
{
  /* Connection created  */
  if (ev == MG_EV_OPEN)
    {

		}
  /* Connection closed */
  else if (ev == MG_EV_CLOSE)
    {
      /* Clear the data  */
  
      c->data[0] = 0;
		}
  /* Websocket handshake done */
	else if (ev == MG_EV_WS_OPEN)
    {
      /* When WS handhake is done, mark us as WS client */
  
      c->data[0] = 'W';
    }
  /* Websocket msg, text or bin */
	else if (ev == MG_EV_WS_MSG) 
		{ 
    	/* Got websocket frame. Received data is wm->data. */

  	}
  /* Full HTTP request/response */
  else if (ev == MG_EV_HTTP_MSG)
    {
      struct mg_http_message *hm = ev_data;

      if (mg_match(hm->uri, mg_str("/websocket"), NULL))
	    	{
					/* Upgrade to websocket. From now on, a connection is a full-duplex */
					/* Websocket connection, which will receive MG_EV_WS_MSG events. */

      		mg_ws_upgrade(c, hm, NULL);
    		}
      else if (mg_match(hm->uri, mg_str("/api/login"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/logout"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/debug"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/stats/get"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/events/get"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/settings/get"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/settings/set"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/firmware/upload"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/firmware/commit"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/firmware/rollback"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/firmware/status"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/device/reset"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/device/eraselast"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/led/get"), NULL)) 
        {

        } 
      else if (mg_match(hm->uri, mg_str("/api/led/toggle"), NULL)) 
        {

        } 
      else 
        {
          struct mg_http_serve_opts *opts = &g_httpd_opts; 

          mg_http_serve_dir(c, ev_data, opts);
        }
    }
} 

/****************************************************************************
 * main program
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  bool gps_enab = false;
  int exitcode = 0;
  int port = 8001;
  char hosturl[32];
  int option, ret;
  struct sched_param sparam;
  pthread_attr_t attr;
  pthread_t tid;

#ifndef CONFIG_NSH_NETINIT
  /* We are running standalone (as opposed to a NSH built-in app). Therefore
   * we need to initialize the network before we start.
   */

  struct in_addr addr;

  /* Set up our host address */

  addr.s_addr = HTONL(CONFIG_EXAMPLES_MONGOOSE_IPADDR);
  netlib_set_ipv4addr("eth0", &addr);

  /* Set up the default router address */

  addr.s_addr = HTONL(CONFIG_EXAMPLES_MONGOOSE_DRIPADDR);
  netlib_set_dripv4addr("eth0", &addr);

  /* Setup the subnet mask */

  addr.s_addr = HTONL(CONFIG_EXAMPLES_MONGOOSE_NETMASK);
  netlib_set_ipv4netmask("eth0", &addr);

  /* New versions of netlib_set_ipvXaddr will not bring the network up,
   * So ensure the network is really up at this point.
   */

  netlib_ifup("eth0");
#endif /* CONFIG_NSH_NETINIT */

#if defined(CONFIG_NET_TCP)
  printf("Starting Smart Antenna Server\n");

  /* Default value */

  strncpy(g_root_dir, ROOT_DIR, sizeof(g_root_dir));

  /* Parse Argument */

  while ((option = getopt(argc, argv, "p:r:g")) != EOF)
    {
      switch (option)
      {
        case 'p':
          port = strtol(optarg, NULL, 0);
          if (port < 0)
            {
              goto errout;
            }
          break;

        case 'r':
          strncpy(g_root_dir, optarg, sizeof(g_root_dir));
          break;

        case 'g':
          gps_enab = true;
          break;

        default:
          goto errout;
        }
    }

  /* Initialize the options */

  memset(&g_httpd_opts, 0, sizeof(g_httpd_opts));
  g_httpd_opts.fs = ROOT_FS;
  g_httpd_opts.root_dir = g_root_dir;

  /* Open FIFOs */
  if (gps_enab)
    {
      if (mkfifo(FIFO_PATH, 0666) < 0)
        {
          if (EEXIST != errno)
            {
              printf("mongoose_main: mkfifo failed: %d\n", errno);
              exitcode = 1;
              goto errout;          
            }
        }
    }

  /* Set log level */

  mg_log_set(MG_LL_NONE);

  /* Initialise event manager */

  mg_mgr_init(&g_evt_mgr);

  /* Create HTTP listener */

  snprintf(hosturl, sizeof(hosturl), "http://0.0.0.0:%d", port);
  mg_http_listen(&g_evt_mgr, hosturl, ev_handler, NULL);

  /* Start the GPS message listeners */

  if (gps_enab)
    {
      /* Thread attributes */

      pthread_attr_init(&attr);
#ifdef CONFIG_NETINIT_THREAD
      sparam.sched_priority = CONFIG_NETINIT_THREAD_PRIORITY - 1;
#else
      sparam.sched_priority = 100;
#endif
      pthread_attr_setschedparam(&attr, &sparam);
      pthread_attr_setstacksize(&attr, 2048);

      /* Create the thread */

      ret = pthread_create(&tid, &attr, gpoll_thread, NULL);
      if (ret != 0)
        {
          printf("mongoose_main: Failed to create listener thread: %d\n", ret);
          exitcode = 2;
          goto errout;
        }
    }

  /* Infinite event loop */

  while (1)
    {
      /* Event manager poll */

      mg_mgr_poll(&g_evt_mgr, 100);
    }

errout:

  /* free the allocated resources */

  mg_mgr_free(&g_evt_mgr);
#endif /* CONFIG_NET_TCP & CONFIG_NET_UDP */

#ifndef CONFIG_NSH_NETINIT
  /* We are running standalone (as opposed to a NSH built-in app). Therefore
   * we should not exit after httpd failure.
   */

  while (1)
    {
      sleep(3);
      printf("mongoose_main: Still running\n");
      fflush(stdout);
    }

#else /* CONFIG_NSH_NETINIT */
  /* We are running as a NSH built-in app.  Therefore we should exit.  This
   * allows to 'kill -9' the mongoose app, assuming it was started as a
   * background process.  For example:
   *
   *    nsh> mongoose &
   *    mongoose [6:100]
   *    nsh> Starting mongoose
   *
   *    nsh> kill -9 6
   *    nsh> mongoose_main: Exiting
   */

  printf("mongoose_main: Exiting\n");

#endif /* CONFIG_NSH_NETINIT */

  fflush(stdout);
  return exitcode;
}
