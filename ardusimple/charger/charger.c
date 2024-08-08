/****************************************************************************
 * apps/ardusimple/charger/charger_main.c
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

#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include <nuttx/power/battery_charger.h>
#include <nuttx/power/battery_ioctl.h>
#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>

#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_EXAMPLES_CHARGER_DEVNAME
#  define CHARGER_DEVPATH CONFIG_EXAMPLES_CHARGER_DEVNAME
#else
#  define CHARGER_DEVPATH "/dev/batt0"
#endif

#ifdef CONFIG_EXAMPLES_ADC_DEVNAME
#  define ADC_DEVPATH CONFIG_EXAMPLES_ADC_DEVNAME
#else
#  define ADC_DEVPATH "/dev/adc0"
#endif

#ifndef CONFIG_ADC_GROUPSIZE
#  define CONFIG_ADC_GROUPSIZE 2
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * show_bat_status
 ****************************************************************************/

static int show_bat_status(int fd)
{
  int status;
  int health;

  const char *statestr[] =
    {
      "UNKNOWN",
      "FAULT",
      "IDLE",
      "FULL",
      "CHARGING",
      "DISCHARGING"
    };

  const char *healthstr[] =
    {
      "UNKNOWN",
      "GOOD",
      "DEAD",
      "OVERHEAT",
      "OVERVOLTAGE",
      "UNSPEC_FAIL",
      "COLD",
      "WD_TMR_EXP",
      "SAFE_TMR_EXP",
      "DISCONNECTED"
    };

  int ret;

  ret = ioctl(fd, BATIOC_STATE, (unsigned long)(uintptr_t)&status);
  if (ret < 0)
    {
      printf("ioctl BATIOC_STATE failed. %d\n", errno);
      return -1;
    }

  ret = ioctl(fd, BATIOC_HEALTH, (unsigned long)(uintptr_t)&health);
  if (ret < 0)
    {
      printf("ioctl BATIOC_HEALTH failed. %d\n", errno);
      return -1;
    }

  printf("State: %s, Health: %s\n",
         statestr[status], healthstr[health]);

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct adc_msg_s sample[CONFIG_ADC_GROUPSIZE];
  int opt, verbose = 0;
  size_t readsize;
  ssize_t nbytes;
  int current;
  int voltage;
  int fd_conf;
  int fd_meas;
  int ret, op;
  int errval = 0;

  while ((opt = getopt(argc, argv, "v")) != -1)
    {
      switch (opt)
        {
          case 'v':
            verbose = 1;
            break;

          default:
            printf("Usage: %s [-v]\n", argv[0]);
            return 1;
        }
    }

  /* open the battery charger device */

  fd_conf = open(CHARGER_DEVPATH, O_RDWR);
  if (fd_conf < 0)
    {
      errval = errno;
      printf("Charger device open error.\n");
      return errval;
    }

  /* Show the battery status */

  if (verbose)
    {
      show_bat_status(fd_conf);
    }

  /* Set the input current limit (mA) */

  current = 1000;
  ret = ioctl(fd_conf, BATIOC_INPUT_CURRENT, (unsigned long)(uintptr_t)&current);
  if (ret < 0)
    {
      errval = errno;
      printf("ioctl BATIOC_INPUT_CURRENT failed. %d\n", errval);
      goto errout_conf;
    }

  /* Set the charge current (mA) */

  current = 500;
  ret = ioctl(fd_conf, BATIOC_CURRENT, (unsigned long)(uintptr_t)&current);
  if (ret < 0)
    {
      errval = errno;
      printf("ioctl BATIOC_CURRENT failed. %d\n", errval);
      goto errout_conf;
    }

  /* Set the charge voltage (mV) */

  voltage = 4200;
  ret = ioctl(fd_conf, BATIOC_VOLTAGE, (unsigned long)(uintptr_t)&voltage);
  if (ret < 0)
    {
      errval = errno;
      printf("ioctl BATIOC_VOLTAGE failed. %d\n", errval);
      goto errout_conf;
    }

  /* Set charging mode */

  op = BATIO_OPRTN_CHARGE;
  ret = ioctl(fd_conf, BATIOC_OPERATE, (unsigned long)(uintptr_t)&op);
  if (ret < 0)
    {
      errval = errno;
      printf("ioctl BATIOC_OPERATE failed. %d\n", errval);
      goto errout_conf;
    }

  /* Set system on mode (BATFET enable) */

  op = BATIO_OPRTN_SYSON;
  ret = ioctl(fd_conf, BATIOC_OPERATE, (unsigned long)(uintptr_t)&op);
  if (ret < 0)
    {
      errval = errno;
      printf("ioctl BATIOC_OPERATE failed. %d\n", errval);
      goto errout_conf;
    }

  /* Show the battery status */

  if (verbose)
    {
      struct timeval tv;

      gettimeofday(&tv, NULL);
      printf("%ju.%06ld: %d mV, %d mA\n",
            (uintmax_t)tv.tv_sec, tv.tv_usec, voltage, current);

      show_bat_status(fd_conf);
    }

  /* open the adc device */

  fd_meas = open(ADC_DEVPATH, O_RDWR);
  if (fd_meas < 0)
    {
      errval = errno;
      printf("ADC device open error.\n");
      goto errout_conf;
    }

/* Measure the battery voltage forever*/

while (1)
  {
    /* Issue the software trigger to start ADC conversion */

    ret = ioctl(fd_meas, ANIOC_TRIGGER, 0);
    if (ret < 0)
      {
          break;
      }

    /* Read up to CONFIG_ADC_GROUPSIZE samples */

    readsize = CONFIG_ADC_GROUPSIZE * sizeof(struct adc_msg_s);
    nbytes = read(fd_meas, sample, readsize);

    /* Handle unexpected return values */

    if (nbytes < 0)
      {
        errval = errno;
        if (errval != EINTR)
          {
            errval = 3;
            goto errout_meas;
          }
      }

    /* Print the sample data on successful return */

    else if (nbytes > 0)
      {
        int nsamples = nbytes / sizeof(struct adc_msg_s);
        if (nsamples * sizeof(struct adc_msg_s) == nbytes)
          {

          }
      }

    /* Wait for a while */

    sleep(1);
  }

errout_meas:
  close(fd_meas);

errout_conf:
  close(fd_conf);

  return errval;
}
