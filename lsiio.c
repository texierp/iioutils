/*
 * Industrial I/O utilities - lsiio.c
 *
 * Copyright (c) 2010 Manuel Stahl <manuel.stahl@iis.fraunhofer.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "iio.h"

static enum verbosity {
	VERBLEVEL_DEFAULT,	/* 0 gives lspci behaviour */
	VERBLEVEL_SENSORS,	/* 1 lists sensors */
	VERBLEVEL_VALUES,	/* 2 output sensor values */
	VERBLEVEL_DEBUG,	/* 3 output debug messages */
} verblevel = VERBLEVEL_DEFAULT;

static const char* attribute_header[] = {
		"Accelerometers",
		"Gyroscopes",
		"Magnetometers",
		"Temperatures",
		"Barometers",
		"Voltages",
};

static const char* sensor_unit[] = {
		"g", "rad/s", "Gs", "°C", "hPa", "V",
};

static inline int check_prefix(const char *str, const char *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

static int dump_one_device(struct iio_device * iio_dev)
{
	struct dlist * channel_list;
	struct iio_channel * chan;
	struct iio_ring_buffer * ring;
//	struct sysfs_device * sysfs_dev;
	char * indent = "  ";
	enum sensor_type cur_type = SENSOR_UNKOWN;

	if (verblevel >= VERBLEVEL_SENSORS)
		printf("\n");

	printf("Device %03d: %s\n", iio_dev->number, iio_dev->name);

	if (verblevel >= VERBLEVEL_SENSORS) {
		channel_list = iio_get_device_channels(iio_dev);
		dlist_for_each_data(channel_list, chan, struct iio_channel) {
			if (cur_type != chan->type && chan->type < SENSOR_UNKOWN) {
				printf("%s%s:\n", indent, attribute_header[chan->type]);
				cur_type = chan->type;
			}
			printf("%s%-10s", indent, chan->name);
			if (verblevel >= VERBLEVEL_VALUES) {
				printf(": %f %s", (chan->raw + chan->offset) * chan->scale,
						sensor_unit[chan->type]);
				if (verblevel >= VERBLEVEL_DEBUG)
					printf(" = (%f + %f) * %f", chan->raw, chan->offset,
							chan->scale);
			}
			printf("\n");
		}

		ring = iio_get_ring_buffer(iio_dev);
		if (ring) {
			printf("\n%sring_buffer%d:\n", indent, ring->number);
			printf("%s  bps: %d,\t", indent, iio_get_ring_buffer_bps(ring));
			printf("%s  length: %d\n", indent, iio_get_ring_buffer_length(ring));
			printf("%s  event:  %s\n", indent, ring->event);
			printf("%s  access: %s\n", indent, ring->access);
		}

//		sysfs_dev = sysfs_get_device_device(sysfs_dev);
//		if (sysfs_dev != NULL)
//			printf("%sKernel module: %s\n", indent, sysfs_dev->driver_name);
	}
	return 0;
}

static int dump_one_device_path(const char *path)
{
	int ret;
	struct iio_device * iio_dev;
	iio_dev = iio_open_device_path(path);

	if (!iio_dev) {
		printf("%s is no industrial I/O device\n", path);
		return -1;
	}
	ret = dump_one_device(iio_dev);
	iio_close_device(iio_dev);
	return ret;
}

static void dump_devices_with_name(const char *name)
{
	struct iio_device * iio_dev;
	struct sysfs_device * sysfs_dev;
	struct sysfs_bus * iio_bus = NULL;
	struct dlist * sysfs_dev_list = NULL;

	iio_bus = sysfs_open_bus("iio");
	if (iio_bus == NULL) {
		printf("No industrial I/O devices available\n");
		return;
	}

	sysfs_dev_list = sysfs_get_bus_devices(iio_bus);
	if (sysfs_dev_list == NULL) {
		printf("No industrial I/O devices available\n");
		return;
	}

	dlist_for_each_data(sysfs_dev_list, sysfs_dev, struct sysfs_device) {
		if (strchr(sysfs_dev->name, ':') == NULL) {
			iio_dev = iio_open_device_from_sysfs(sysfs_dev);
			if (iio_dev && strcmp(iio_dev->name, name) == 0)
				dump_one_device(iio_dev);
			iio_close_device(iio_dev);
		}
	}
	sysfs_close_bus(iio_bus);
}

static void dump_devices(void)
{
	struct iio_device * iio_dev;
	struct sysfs_device * sysfs_dev;
	struct sysfs_bus * iio_bus = NULL;
	struct dlist * sysfs_dev_list = NULL;

	iio_bus = sysfs_open_bus("iio");
	if (!iio_bus) {
		printf("No industrial I/O devices available\n");
		return;
	}

	sysfs_dev_list = sysfs_get_bus_devices(iio_bus);
	if (!sysfs_dev_list) {
		printf("No industrial I/O devices available\n");
		return;
	}

	dlist_for_each_data(sysfs_dev_list, sysfs_dev, struct sysfs_device) {
		if (strchr(sysfs_dev->name, ':') == NULL) {
			iio_dev = iio_open_device_from_sysfs(sysfs_dev);
			dump_one_device(iio_dev);
			iio_close_device(iio_dev);
		}
	}
}

int main(int argc, char **argv)
{
	static const struct option long_options[] = {
		{ "version", 0, 0, 'V' },
		{ "verbose", 0, 0, 'v' },
		{ 0, 0, 0, 0 }
	};

	int c, err = 0;

	const char *devdump = NULL;
	const char *devname = NULL;

	while ((c = getopt_long(argc, argv, "d:D:vV",
			long_options, NULL)) != EOF) {
		switch(c) {
		case 'V':
			printf("lsiio (" PACKAGE ") " VERSION "\n");
			exit(0);

		case 'v':
			verblevel++;
			break;

		case 'd':
			devname = optarg;
			break;

		case 'D':
			devdump = optarg;
			break;

		case '?':
		default:
			err++;
			break;
		}
	}
	if (err || argc > optind) {
		fprintf(stderr, "Usage: lsiio [options]...\n"
			"List industrial I/O devices\n"
			"  -v, --verbose\n"
			"      Increase verbosity (may be given multiple times)\n"
			"  -d <name>\n"
			"      Show only devices with specified name\n"
			"  -D <device_path>\n"
			"      Selects which device lsiio will examine\n"
			"  -V, --version\n"
			"      Show version of program\n"
			);
		exit(1);
	}

	if (devdump)
		dump_one_device_path(devdump);
	else if (devname)
		dump_devices_with_name(devname);
	else
		dump_devices();

	return 0;
}
