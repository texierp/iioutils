/*
 * Industrial I/O utilities - iio_ring.c
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

#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "iio.h"

#define DEFAULT_RING_LENGTH 64

#define fail_return(msg...) { fprintf(stderr, msg); return -1; }

static enum verbosity {
	VERBLEVEL_DEFAULT,
} verblevel = VERBLEVEL_DEFAULT;

static enum output_type {
	OUTPUT_TABLE, OUTPUT_CVS, OUTPUT_XML,
} out_type = OUTPUT_TABLE;

static volatile enum { PROG_QUIT, PROG_RUN } run = PROG_RUN;

FILE *fp_ev;

int write_sysfs_int(char *filename, char *basedir, int val)
{
	FILE  *sysfsfp;
	char temp[SYSFS_PATH_MAX];
	sprintf(temp, "%s/%s", basedir, filename);
	sysfsfp = fopen(temp, "w");
	if (!sysfsfp)
		fail_return("%s: %s\n", temp, strerror(errno));

	fprintf(sysfsfp, "%d", val);
	fclose(sysfsfp);
	return 0;
}

int write_verify_sysfs_int(char *filename, char *basedir, int val)
{
	int ref;
	FILE  *sysfsfp;
	char temp[SYSFS_PATH_MAX];
	sprintf(temp, "%s/%s", basedir, filename);
	sysfsfp = fopen(temp, "rw");
	if (!sysfsfp)
		fail_return("%s: %s\n", temp, strerror(errno));

	fprintf(sysfsfp, "%d", val);
	if (fscanf(sysfsfp, "%d", &ref) != 1)
		fail_return("verification of %s failed\n", temp);
	fclose(sysfsfp);
	return val == ref;
}

int next_power_of_two(int x)
{
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >>16);
	return x + 1;
}

void quit(/* int sig */) {
    run = PROG_QUIT;
    fclose(fp_ev);
}

/*
static void print_sample_set(char *data, struct dlist *scan_el_list)
{
	struct iio_channel *channel;
	struct iio_scan_element *scan_el;
	char *addr = data;

	dlist_for_each_data(scan_el_list, scan_el, struct iio_channel) {
		if (scan_el->enabled) {
			int32_t sample = *((int32_t*)addr);
			sample <<= 32 - scan_el->bits;
			sample >>= 32 - scan_el->bits;
			addr += next_power_of_two(scan_el->bits);

			printf("%+5.3f ", (float)sample * channel->scale + channel->offset);
	//		printf("%02x %02x  ", (sample >> 8) & 0xff, sample & 0xff);
		}
	}
	// TODO print timestamp
//	printf(" %lld\n",
//	       *(__s64 *)(&data[(i+1)*size_from_scanmode(NumVals, scan_ts)
//				- sizeof(__s64)]));
	printf("\n");
}
*/

static int read_ring(struct iio_device *iio_dev, unsigned ring_length)
{
	const char *ring_access = iio_dev->buffer->access;
	const char *ring_event = iio_dev->buffer->event;

	int fp_ring, samples = 24, bps = 2;
	char *data;

	/* Setup ring buffer parameters */
	if (write_sysfs_int("length", iio_dev->buffer->path, ring_length) < 0)
		fail_return("Failed to set the ring buffer length\n");

	/* Enable the ring buffer */
	if (write_verify_sysfs_int("ring_enable", iio_dev->buffer->path, 1) < 0)
		fail_return("Failed to enable the ring buffer\n");

	data = malloc(bps*samples*ring_length);
	if (!data)
		fail_return("Could not allocate space for buffer data store\n");

	/* Attempt to open non blocking the access dev */
	fp_ring = open(ring_access, O_RDONLY | O_SYNC | O_NONBLOCK);
	if (fp_ring == -1) { /* If it isn't there make the node */
		fprintf(stderr, "Failed to open %s\n", ring_access);
		goto err_ret;
	}

	/* Attempt to open the event access dev (blocking this time) */
	fp_ev = fopen(ring_event, "rb");
	if (fp_ev == NULL) {
		fprintf(stderr, "Failed to open %s\n", ring_event);
		goto err_ret;
	}

	/* Wait for SIGINT */
	while (run == PROG_RUN) {
		int toread, i;
		struct iio_event_data dat;
		int read_size = fread(&dat, 1, sizeof(struct iio_event_data), fp_ev);
		switch (dat.id) {
		case IIO_EVENT_CODE_RING_100_FULL:
			toread = ring_length;
			break;
		case IIO_EVENT_CODE_RING_75_FULL:
			toread = ring_length*3/4;
			break;
		case IIO_EVENT_CODE_RING_50_FULL:
			toread = ring_length/2;
			break;
		default:
			fprintf(stderr, "Unexpected event code 0x%0x\n", dat.id);
			continue;
		}

		read_size = read(fp_ring, data, toread * samples * bps);
		if (read_size == -EAGAIN) {
			fprintf(stderr, "nothing available\n");
			continue;
		}

//		for (i = 0; i < read_size / (samples*bps); i++)
//			print_sample_set(data + i*samples*bps, 12, 14);

	}

err_ret:
	/* Stop the ring buffer */
	if (write_sysfs_int("ring_enable", iio_dev->buffer->path, 0) < 0)
		fail_return("Failed to open the ring buffer control file\n");

	free(data);

	return 0;
}

int main(int argc, char **argv)
{
	struct iio_device *iio_dev;
	struct iio_ring_buffer *ring_buffer;
	static const struct option long_options[] = {
		{ "version", 0, 0, 'V' },
		{ "verbose", 0, 0, 'v' },
		{ "csv", 0, 0, 'c' },
		{ "xml", 0, 0, 'x' },
		{ 0, 0, 0, 0 }
	};

	int c, err = 0;

	const char *path = NULL;
	char trigger_name[SYSFS_NAME_LEN];

    signal(SIGTERM, &quit);
    signal(SIGABRT, &quit);
    signal(SIGINT, &quit);

	while ((c = getopt_long(argc, argv, "D:vV",
			long_options, NULL)) != EOF) {
		switch(c) {
		case 'V':
			printf("iio_ring (" PACKAGE ") " VERSION "\n");
			exit(0);

		case 'v':
			verblevel++;
			break;

		case 'D':
			path = optarg;
			break;

		case 'c':
			out_type = OUTPUT_CVS;
			break;

		case 'x':
			out_type = OUTPUT_XML;
			break;

		case '?':
		default:
			err++;
			break;
		}
	}
	if (err || argc > optind || !path) {
		fprintf(stderr, "Usage: iio_ring [options] -D <device>\n"
			"Access industrial I/O ring buffers\n"
			"  -v, --verbose\n"
			"      Increase verbosity\n"
			"  -D <device>\n"
			"      Selects which device iio_ring will work on\n"
			"  -c, --csv\n"
			"      Output CSV formatted data\n"
			"  -x, --xml\n"
			"      Output XML formatted data\n"
			"  -V, --version\n"
			"      Show version of program\n"
			);
		exit(1);
	}

	iio_dev = iio_open_device_by_name(path);
	if (!iio_dev) {
		fprintf(stderr, "No industrial I/O device named %s!\n", path);
		exit(1);
	}
	printf(	"Device\n"
			"  path: %s\n"
			"  name: %s\n"
			"  number: %d\n", iio_dev->path, iio_dev->name, iio_dev->number);

	ring_buffer = iio_get_ring_buffer(iio_dev);
	if (!ring_buffer) {
		fprintf(stderr, "Industrial I/O device has no ring buffer!\n");
		exit(1);
	}

	printf(	"Buffer\n"
			"  path: %s\n"
			"  event: %s\n"
			"  access: %s\n", ring_buffer->path, ring_buffer->event, ring_buffer->access);

	iio_get_trigger(iio_dev, trigger_name);
	printf( "Trigger: %s\n", trigger_name);
	read_ring(iio_dev, DEFAULT_RING_LENGTH);
	/* Disconnect from the trigger - writing something that doesn't exist.*/
//	iio_set_trigger(iio_dev, "NULL");

	return 0;
}
