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

#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_EXAMPLES_CHARGER_DEVNAME
#  define DEVPATH CONFIG_EXAMPLES_CHARGER_DEVNAME
#else
#  define DEVPATH "/dev/batt0"
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int show_charge_setting(int fd)
{
  int curr;
  int vol;
  int temp;
  int ret;

  ret = ioctl(fd, BATIOC_VOLTAGE, (unsigned long)(uintptr_t)&vol);
  if (ret < 0)
    {
      printf("ioctl BATIOC_VOLTAGE failed. %d\n", errno);
      return -1;
    }

  ret = ioctl(fd, BATIOC_CURRENT, (unsigned long)(uintptr_t)&curr);
  if (ret < 0)
    {
      printf("ioctl GET_CHGCURRENT failed. %d\n", errno);
      return -1;
    }

  ret = ioctl(fd, BATIOC_TEMPERATURE, (unsigned long)(uintptr_t)&temp);
  if (ret < 0)
    {
      printf("ioctl BATIOC_TEMPERATURE failed. %d\n", errno);
      return -1;
    }

  printf("Charge voltage: %d\n", vol);
  printf("Charge current: %d\n", curr);
  printf("Temperature   : %d\n", temp);

  return 0;
}

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
  int opt, verbose = 0;
  struct timeval tv;
  int current;
  int voltage;
  int fd, ret;
  int op;

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

  fd = open(DEVPATH, O_RDWR);
  if (fd < 0)
    {
      printf("Device open error.\n");
      return 0;
    }

  /* Show the battery status */

  show_bat_status(fd);

  /* Set the input current limit (mA) */

  current = 1000;
  ret = ioctl(fd, BATIOC_INPUT_CURRENT, (unsigned long)(uintptr_t)&current);
  if (ret < 0)
    {
      printf("ioctl BATIOC_INPUT_CURRENT failed. %d\n", errno);
      return 1;
    }

  /* Set the charge current (mA) */

  current = 500;
  ret = ioctl(fd, BATIOC_CURRENT, (unsigned long)(uintptr_t)&current);
  if (ret < 0)
    {
      printf("ioctl BATIOC_CURRENT failed. %d\n", errno);
      return 1;
    }

  /* Set the charge voltage (mV) */

  voltage = 4200;
  ret = ioctl(fd, BATIOC_VOLTAGE, (unsigned long)(uintptr_t)&voltage);
  if (ret < 0)
    {
      printf("ioctl BATIOC_VOLTAGE failed. %d\n", errno);
      return 1;
    }

  /* Set charging mode */

  op = BATIO_OPRTN_CHARGE;
  ret = ioctl(fd, BATIOC_OPERATE, (unsigned long)(uintptr_t)&op);
  if (ret < 0)
    {
      printf("ioctl BATIOC_OPERATE failed. %d\n", errno);
      return 1;
    }

  /* Set system off mode */

  op = BATIO_OPRTN_SYSOFF;
  ret = ioctl(fd, BATIOC_OPERATE, (unsigned long)(uintptr_t)&op);
  if (ret < 0)
    {
      printf("ioctl BATIOC_OPERATE failed. %d\n", errno);
      return 1;
    }

  /* Set system on mode */

  op = BATIO_OPRTN_SYSON;
  ret = ioctl(fd, BATIOC_OPERATE, (unsigned long)(uintptr_t)&op);
  if (ret < 0)
    {
      printf("ioctl BATIOC_OPERATE failed. %d\n", errno);
      return 1;
    }

  gettimeofday(&tv, NULL);
  printf("%ju.%06ld: %d mV, %d mA\n",
         (uintmax_t)tv.tv_sec, tv.tv_usec, voltage, current);

  /* Show the battery status */

  show_bat_status(fd);

  close(fd);

  return 0;
}
