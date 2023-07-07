/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * u7.c a program for controlling ALSA volume through Linux input device
 *
 * Copyright (C) 2015 Nedko Arnaudov <nedko@arnaudov.name>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "asoundlib.h"

const char * progname(const char * argv0)
{
  const char * name;

  name = strrchr(argv0, '/');
  if (name == NULL) return argv0;
  return name + 1;
}

struct mixer * u7_mixer_open(const char * name)
{
  int card;
  struct mixer * mixer;
  const char * tmp_name;

  for (card = 0; card < INT_MAX; card++)
  {
    mixer = mixer_open(card);
    if (mixer == NULL) return NULL;
    tmp_name = mixer_get_name(mixer);
    /* printf("mixer \"%s\"\n", tmp_name); */
    if (strcmp(tmp_name, name) == 0) return mixer;
    mixer_close(mixer);
  }

  return NULL;
}

int main(int argc, char ** argv)
{
  const char * device;
  int fd;
  struct input_event event;
  ssize_t sret;
  struct mixer * mixer;
  const char * mixer_name;
  const char * control;
  struct mixer_ctl * ctl;
  enum mixer_ctl_type type;
  int min, max;
  unsigned int num_ctl_values;
  unsigned int i;
  int v;
  int volume = 87;
  int step = 1;

  device = "/dev/input/by-id/usb-ASUS_Xonar_U7-event-if04";
  mixer_name = "Xonar U7";	       /* alsa mixer name */
  control = "PCM Playback Volume"; /* alsa mixer control name */

  if (argc == 4)
  {
    device = argv[1];
    mixer_name = argv[2];
    control = argv[3];
  }
  else if (argc != 1)
  {
    fprintf(
      stderr,
      "usage: %s [<input_device_path> <mixer_name> <mixer_control_name>]\n"
      "where:\n"
      " <input_device_path> is kernel input device path,\n"
      "   defaults to \"%s\"\n"
      " <mixer_name> is ALSA mixer device name,\n"
      "   defaults to \"%s\"\n"
      " <mixer_control_name> is ALSA mixer control name,\n"
      "   defaults to \"%s\"\n",
      progname(argv[0]),
      device,
      mixer_name,
      control);
    return EXIT_FAILURE;
  }

  mixer = u7_mixer_open(mixer_name);
  if (!mixer)
  {
    fprintf(stderr, "Failed to open mixer\n");
    return EXIT_FAILURE;
  }

  ctl = mixer_get_ctl_by_name(mixer, control);
  if (!ctl)
  {
    fprintf(stderr, "Invalid mixer control\n");
    return EXIT_FAILURE;
  }

  type = mixer_ctl_get_type(ctl);
  if (type != MIXER_CTL_TYPE_INT)
  {
    fprintf(stderr, "Invalid mixer control type\n");
    return EXIT_FAILURE;
  }

  min = mixer_ctl_get_range_min(ctl);
  max = mixer_ctl_get_range_max(ctl);
  /* printf("min %d, max %d\n", min, max); */

  num_ctl_values = mixer_ctl_get_num_values(ctl);
  for (i = 0; i < num_ctl_values; i++)
  {
    v = mixer_ctl_get_value(ctl, i);
    if (i == 0) volume = v;	/* get volume of the first channel */
    /* printf(" %d\n", v); */
  }

  fd = open(device, O_RDONLY);
  if (fd == -1)
  {
    fprintf(stderr, "Cannot open device \"%s\", error %d (%s)\n", device, errno, strerror(errno));
    return EXIT_FAILURE;
  }

  goto set_volume;		/* set volume to all channels */

next:
  sret = read(fd, &event, sizeof(event));
  if (sret == -1)
  {
    if (errno == EINTR) goto next;
    fprintf(stderr, "read error %d (%s)\n", errno, strerror(errno));
    return EXIT_FAILURE;
  }

  if (sret != sizeof(event))
  {
    /* unexpected, reading from input device descriptor
       should return multiple sizeof(event) */
    fprintf(stderr, "unexpected read size %zd\n", sret);
    return EXIT_FAILURE;
  }

  if (event.type == EV_KEY && event.value == 0)
  {
     switch (event.code)
     {
     case KEY_VOLUMEDOWN:
       if (volume > min)
       {
	 if (volume - min >= step)
	   volume -= step;
	 else
	   volume = min;
       }
       break;
     case KEY_VOLUMEUP:
       if (volume < max)
       {
	 if (max - volume >= step)
	   volume += step;
	 else
	   volume = max;
       }
       break;
     default:
       goto next;
     }

  set_volume:
     printf("%d", volume);
     if (volume == min) printf(" (min)");
     if (volume == max) printf(" (max)");
     printf("\n");

     for (i = 0; i < num_ctl_values; i++)
     {
       if (mixer_ctl_set_value(ctl, i, volume))
       {
	 fprintf(stderr, "error setting volume\n");
	 return EXIT_FAILURE;
       }
     }
  }
  goto next;

  mixer_close(mixer);
  close(fd);

  return 0;
}
