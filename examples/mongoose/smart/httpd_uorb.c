/****************************************************************************
 * apps/examples/mongoose/smart/httpd.c
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
#include <debug.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <netutils/netlib.h>
#include <uORB/uORB.h>
#include <mongoose.h>

/****************************************************************************
 * External Functions
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ROOT_FS	 		(&mg_fs_posix)
#define ROOT_DIR    "/data0/www"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Global server options */

struct mg_http_serve_opts g_httpd_opts;

/* Global event manager */

static struct mg_mgr g_evt_mgr;

/* Global uORB object */

static struct orb_object g_uorb_obj;

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
 * Name: uorb_ondata
 *
 * Description:
 *   Print topic data by its print_message callback.
 *
 * Input Parameters:
 *   meta         The uORB metadata.
 *   fd           Subscriber handle.
 *
 * Returned Value:
 *   0 on success copy, otherwise -1
 ****************************************************************************/

static int uorb_ondata(FAR const struct orb_metadata *meta, int fd)
{
  char buffer[meta->o_size];
  FAR struct sensor_gps_raw *gps = (FAR struct sensor_gps_raw *)buffer;
  int ret;

  ret = orb_copy(meta, fd, buffer);
  if (ret == OK)
    {
      /* Send the received data to the client */

      send_to_ws(gps->buf, gps->len, WEBSOCKET_OP_TEXT);
    }

  return ret;
}

/****************************************************************************
 * Name: sensor_subscribe
 ****************************************************************************/

static struct pollfd *sensor_subscribe(FAR const char *topic_name,
                                       float topic_rate,
                                       int topic_latency)
{
  FAR struct pollfd *fds;
  size_t len;
  int fd;

  /* Allocate poll data */

  fds = malloc(sizeof(struct pollfd));
  if (!fds)
    {
      return NULL;
    }

  /* calculate the inverval */

  float interval = topic_rate ? (1000000 / topic_rate) : 0;

  /* get the object meta */

  len = strlen(topic_name) - 1;
  g_uorb_obj.instance = topic_name[len] - '0';
  g_uorb_obj.meta     = orb_get_meta(topic_name);

  /* subscribe to the topic */

  fd = orb_subscribe_multi(g_uorb_obj.meta, g_uorb_obj.instance);
  if (fd < 0)
    {
      free(fds);
      return NULL;
    }

  /* set the interval */

  if (interval != 0)
    {
      orb_set_interval(fd, (unsigned)interval);

      if (topic_latency != 0)
        {
          orb_set_batch_interval(fd, topic_latency);
        }
    }

  fds->fd     = fd;
  fds->events = POLLIN;

  return fds;
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
  struct pollfd *fds = NULL;
  bool gps_enab = false;
  int port = 8001;
  char hosturl[32];
  int option;

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

  /* Register the event topic(s) */

  if (gps_enab)
    {
      fds = sensor_subscribe("sensor_gps_raw0", 10000, 0);
    }

  /* Initialize the options */

  memset(&g_httpd_opts, 0, sizeof(g_httpd_opts));
  g_httpd_opts.fs = ROOT_FS;
  g_httpd_opts.root_dir = g_root_dir;

  /* Set log level */

  mg_log_set(MG_LL_NONE);

  /* Initialise event manager */

  mg_mgr_init(&g_evt_mgr);

  /* Create HTTP listener */

  snprintf(hosturl, sizeof(hosturl), "http://0.0.0.0:%d", port);
  mg_http_listen(&g_evt_mgr, hosturl, ev_handler, NULL);

  /* Infinite event loop */

  while (1)
    {
      /* Event manager poll */

      mg_mgr_poll(&g_evt_mgr, 10);

      /* Check the uORB topic event */

      if ((fds != NULL) && (poll(fds, 1, 0) > 0))
        {
          if (fds->revents & POLLIN)
            {
              if (uorb_ondata(g_uorb_obj.meta, fds->fd) != 0)
                {
                    
                }
            }
        }
    }

errout:

  /* Unregister the event topic(s) */

  if (fds != NULL)
    {
      orb_unsubscribe(fds->fd);
      free(fds);
    }

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
  return 0;
}
