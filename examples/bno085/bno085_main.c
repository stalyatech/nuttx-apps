/****************************************************************************
 * apps/examples/bno085/bno085_main.c
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
#include <nuttx/sensors/sensor.h>
#include <nuttx/sensors/bno085.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#define ARRAY_LEN(a)  (sizeof(a)/sizeof(a[0]))

/****************************************************************************
 * Private data
 ****************************************************************************/

/* Structure used when polling the sub-sensors */

static struct bno085_reports_s sensorReports[] = 
{
    // Accel, 100 Hz
    {SH2_ACCELEROMETER, {.reportInterval_us = 10000}},

    // Gyroscope calibrated , 100 Hz
    {SH2_GYROSCOPE_CALIBRATED, {.reportInterval_us = 10000}},

    // Magnetic field calibrated , 100 Hz
    {SH2_MAGNETIC_FIELD_CALIBRATED, {.reportInterval_us = 10000}},
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * bno085_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  sh2_SensorValue_t   sensor_data;
  sh2_MagneticField_t mag_data;
  sh2_Accelerometer_t acc_data;
  sh2_Gyroscope_t     gyr_data;

  int fd, ret, ready;

  /* This example works when all of the sub-sensors of
   * the BNO085 are enabled.
   */

  /* Open each lowerhalf file to be able to read the data.
   * When the pressure measurement is deactivated, sensor_temp0 should
   * be opened instead (to get the temperature measurement).
   */

  fd = open("/dev/sensor0", O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      printf("Failed to open smart sensor\n");
      return -1;
    }

  /* Perform a reset */

  ret = ioctl(fd, SNIOC_RESET, 0);
  if (ret != 0) 
    {
      printf("Failed to reset smart sensor\n");
    }

  /* Wait till reset compteled */
  do 
  {
    ret = ioctl(fd, SNIOC_STATUS, (unsigned long)&ready);
  } while ((ret == 0) && (!ready));

  /* Configure the sensor */

  for (int n = 0; n < ARRAY_LEN(sensorReports); n++)
  {
    ret = ioctl(fd, SNIOC_CONFIG, &sensorReports[n]);
    if (ret != 0) 
      {
        printf("Failed to config smart sensor: %d\n", sensorReports[n].sensorId);
      }
  }

  struct pollfd pfds =
  {
      .fd = fd, 
      .events = POLLIN
  };

  while (1)
    {
      ret = poll(&pfds, 1, -1);
      if (ret < 0)
        {
          perror("Could not poll sensor.");
          return ret;
        }

      if (pfds.revents & POLLIN)
        {
          ret = read(pfds.fd, &sensor_data, sizeof(sensor_data));
          if (ret != sizeof(sensor_data))
            {
              perror("Could not read from sub-sensor.");
              return ret;
            }

            switch (sensor_data.sensorId)
              {
                case SH2_ACCELEROMETER:
                  acc_data.x = sensor_data.un.accelerometer.x;
                  acc_data.y = sensor_data.un.accelerometer.y;
                  acc_data.z = sensor_data.un.accelerometer.z;
                  break;

                case SH2_GYROSCOPE_CALIBRATED:
                  gyr_data.x = sensor_data.un.gyroscope.x;
                  gyr_data.y = sensor_data.un.gyroscope.y;
                  gyr_data.z = sensor_data.un.gyroscope.z;
                  break;

                case SH2_MAGNETIC_FIELD_CALIBRATED:
                  mag_data.x = sensor_data.un.magneticField.x;
                  mag_data.y = sensor_data.un.magneticField.y;
                  mag_data.z = sensor_data.un.magneticField.z;
                  break;
              }

          printf("AX=%f AY=%f AZ=%f GX=%f GY=%f GZ=%f MX=%f MY=%f MZ=%f\n", acc_data.x, acc_data.y, acc_data.z,
                                                                            gyr_data.x, gyr_data.y, gyr_data.z,
                                                                            mag_data.x, mag_data.y, mag_data.z);
        }

      usleep(10000);
    }
  printf("Exit\n");

  close(fd);

  return 0;
}
