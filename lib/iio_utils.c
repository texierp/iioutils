/*
 * Industrial I/O utilities - iio_utils.c
 *
 * Copyright (c) 2010 Manuel Stahl <manuel.stahl@iis.fraunhofer.de>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "iio.h"

const char* sensor_prefix[SENSOR_UNKOWN] = {
		"accel",
		"gyro",
		"magn",
		"temp",
		"pressure",
		"in",
};

#define fail_return(msg...) { fprintf(stderr, msg); return -1; }
#define error_return(msg...) { fprintf(stderr, msg); goto err_ret; }

static inline int check_prefix(const char *str, const char *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

static inline int check_postfix(const char *str, const char *postfix) {
	return strncmp(str + strlen(str) - strlen(postfix), postfix, strlen(postfix)) == 0;
}

static inline void strip_postfix(char *str)
{
	char *tok = strrchr(str, '_');
	if (tok) *tok = '\0';
}


int iio_posint_from_path(const char *path)
{
	int ret;
	FILE  *sysfsfp;
	sysfsfp = fopen(path, "r");
	if (!sysfsfp)
		return -1;
	if (fscanf(sysfsfp, "%d\n", &ret) != 1)
		ret = -1;
	fclose(sysfsfp);
	return ret;
}

int iio_read_posint(const char *basedir, const char *filename)
{
	char path[SYSFS_PATH_MAX];
	sprintf(path, "%s/%s", basedir, filename);
	return iio_posint_from_path(path);
}

int iio_read_int_with_postfix(const char *basedir, const char *name, const char *postfix)
{
	char path[SYSFS_PATH_MAX];
	sprintf(path, "%s/%s_%s", basedir, name, postfix);
	return iio_posint_from_path(path);
}


float iio_float_from_path(const char *path)
{
	float ret;
	FILE  *sysfsfp;
	sysfsfp = fopen(path, "r");
	if (!sysfsfp)
		return NAN;
	if (fscanf(sysfsfp, "%f\n", &ret) != 1)
		ret = NAN;
	fclose(sysfsfp);
	return ret;
}

float iio_read_float(const char *basedir, const char *filename)
{
	char path[SYSFS_PATH_MAX];
	sprintf(path, "%s%s", basedir, filename);
	return iio_float_from_path(path);
}

float iio_read_float_with_postfix(const char *basedir, const char *name, const char *postfix)
{
	char path[SYSFS_PATH_MAX];
	sprintf(path, "%s/%s_%s", basedir, name, postfix);
	return iio_float_from_path(path);
}


int iio_string_from_path(char *dest, const char *path)
{
	int ret = 0;
	FILE  *sysfsfp;
	sysfsfp = fopen(path, "r");
	if (!sysfsfp)
		return -1;
	if (fscanf(sysfsfp, "%s\n", dest) != 1)
		ret = -1;
	fclose(sysfsfp);
	return ret;
}

int iio_read_string(char *dest, const char *basedir, const char *filename)
{
	char path[SYSFS_PATH_MAX];
	sprintf(path, "%s%s", basedir, filename);
	return iio_string_from_path(dest, path);
}

int iio_read_string_with_postfix(char *dest, const char *basedir, const char *name, const char *postfix)
{
	char path[SYSFS_PATH_MAX];
	sprintf(path, "%s/%s_%s", basedir, name, postfix);
	return iio_string_from_path(dest, path);
}




void iio_close_device(struct iio_device *iio_dev)
{
	if (iio_dev) {
		if (iio_dev->buffer)
			free(iio_dev->buffer);
		if (iio_dev->channellist)
			dlist_destroy(iio_dev->channellist);
		free(iio_dev);
	}
}

struct iio_device *iio_open_device_from_sysfs(struct sysfs_device *sysfs_dev)
{
	struct iio_device *iio_dev;
	struct sysfs_attribute *attr;
	iio_dev = calloc(1, sizeof(struct iio_device));
	sscanf(sysfs_dev->name, "device%d", &(iio_dev->number));
	attr = sysfs_get_device_attr(sysfs_dev, "name");
	if (!attr || sysfs_read_attribute(attr)) {
		fprintf(stderr, "Read industrial I/O device name failed\n");
		free(iio_dev);
		return NULL;
	}

	sscanf(attr->value, "%s", iio_dev->name);
	strncpy(iio_dev->path, sysfs_dev->path, SYSFS_PATH_MAX);
	return iio_dev;
}

struct iio_device *iio_open_device_by_name(const char *name)
{
	struct iio_device *iio_dev = NULL;
	struct sysfs_device *sysfs_dev;
	struct sysfs_bus *iio_bus = NULL;
	struct dlist *sysfs_dev_list = NULL;

	if (!name) {
		errno = EINVAL;
		return NULL;
	}

	iio_bus = sysfs_open_bus("iio");
	if (iio_bus == NULL) {
		fprintf(stderr, "No industrial I/O devices available\n");
		return NULL;
	}

	sysfs_dev_list = sysfs_get_bus_devices(iio_bus);
	if (sysfs_dev_list == NULL) {
		fprintf(stderr, "No industrial I/O devices available\n");
		return NULL;
	}

	dlist_for_each_data(sysfs_dev_list, sysfs_dev, struct sysfs_device) {
		if (strchr(sysfs_dev->name, ':') == NULL) {
			iio_dev = iio_open_device_from_sysfs(sysfs_dev);
			break;
		}
	}

	sysfs_close_bus(iio_bus);
	return iio_dev;
}

struct iio_device *iio_open_device_path(const char *path)
{
	struct iio_device *iio_dev = NULL;
	struct sysfs_device *sysfs_dev;

	sysfs_dev = sysfs_open_device_path(path);
	if (sysfs_dev == NULL) {
		fprintf(stderr, "No such industrial I/O device: %s\n", path);
		return NULL;
	}

	iio_dev = iio_open_device_from_sysfs(sysfs_dev);
	sysfs_close_device(sysfs_dev);
	return iio_dev;
}


float iio_get_channel_modifier(struct iio_device *dev, const char *chan_name, const char *mod_name, float def_value)
{
	char *end;
	float mod_value = def_value;

	mod_value = iio_read_float_with_postfix(dev->path, chan_name, mod_name);
	if (isnan(mod_value)) {
		/* search for global modifier */
		if ((end = strchr(chan_name, '_')) != NULL) {
			char prefix[SYSFS_NAME_LEN];
			strncpy(prefix, chan_name, end - chan_name);
			prefix[end - chan_name] = '\0';
			return iio_get_channel_modifier(dev, prefix, mod_name, def_value);
		} else {
			mod_value = def_value;
		}
	}
	return mod_value;
}

/**
 * iio_get_device_channels: gets list of channels that are part of a device
 * @dev: iio_device whose channel list is needed
 * Returns dlist of struct iio_channel on success and NULL on failure
 */
struct dlist *iio_get_device_channels(struct iio_device *dev)
{
	struct sysfs_device *sysfs_dev;
	struct dlist *attr_list = NULL;
	struct sysfs_attribute *attr;
	struct iio_channel *channel;

	if (!dev) {
		errno = EINVAL;
		return NULL;
	}

	sysfs_dev = sysfs_open_device_path(dev->path);
	attr_list = sysfs_get_device_attributes(sysfs_dev);
	if (!attr_list)
		error_return("Could not open device");

	dlist_for_each_data(attr_list, attr, struct sysfs_attribute) {
		if (check_postfix(attr->name, IIO_MOD_RAW)) {
			if (!dev->channellist) {
				dev->channellist = dlist_new(sizeof(struct iio_channel));
				if (!dev->channellist)
					error_return("Error creating channel list\n");
			}
			channel = (struct iio_channel *)calloc(1, sizeof(struct iio_channel));
			if (!channel) {
				fprintf(stderr, "Could not allocate channel\n");
				continue;
			}

			channel->dev = dev;
			iio_name_from_attribute(channel->name, attr->name);
			channel->type = 0;
			while (!check_prefix(channel->name, sensor_prefix[channel->type]))
					channel->type++;
			channel->raw = iio_get_channel_modifier(dev, channel->name, IIO_MOD_RAW, 1.0f);
			channel->scale = iio_get_channel_modifier(dev, channel->name, IIO_MOD_SCALE, 1.0f);
			channel->offset = iio_get_channel_modifier(dev, channel->name, IIO_MOD_OFFSET, 0.0f);
			dlist_unshift_sorted(dev->channellist, channel, sort_list);
		}
	}

err_ret:
	sysfs_close_device(sysfs_dev);
	return dev->channellist;
}


struct iio_ring_buffer *iio_get_ring_buffer(struct iio_device * iio_dev)
{
	struct dlist *dir_list = NULL;
	char *dir;

	if (!iio_dev)
		return NULL;

	dir_list = sysfs_open_directory_list(iio_dev->path);
	if (dir_list == NULL) return NULL;

	dlist_for_each_data(dir_list, dir, char) {
		if (strstr(dir, ":buffer")) {
			struct iio_ring_buffer *buf;
			buf = calloc(1, sizeof(struct iio_ring_buffer));
			if (!buf)
				return NULL;

			buf->device = iio_dev;
			sscanf(dir, "device%*d:buffer%d", &(buf->number));
			snprintf(buf->path, SYSFS_PATH_MAX, "%s/%s", iio_dev->path, dir);
			snprintf(buf->event, SYSFS_PATH_MAX,
					IIO_DEV_DIR"ring_event_line%d", buf->number);
			snprintf(buf->access, SYSFS_PATH_MAX,
					IIO_DEV_DIR"ring_access%d", buf->number);

			iio_dev->buffer = buf;
			break;
		}
	}
	sysfs_close_list(dir_list);
	return iio_dev->buffer;
}

int iio_get_ring_buffer_bps(struct iio_ring_buffer *buf)
{
	return iio_read_posint(buf->path, "bps");
}

int iio_get_ring_buffer_length(struct iio_ring_buffer *buf)
{
	return iio_read_posint(buf->path, "length");
}

int iio_is_ring_buffer_enabled(struct iio_ring_buffer *buf)
{
	return iio_read_posint(buf->path, "ring_enable");
}

struct dlist *iio_get_ring_buffer_scan_elements(struct iio_ring_buffer *buffer)
{
	struct dlist *scan_elements = NULL;
	struct dlist *dir_list = NULL;
	char path[SYSFS_PATH_MAX];
	char *dir;

	if (!buffer || !buffer->device)
		return NULL;

	snprintf(path, SYSFS_PATH_MAX, "%s/scan_elements", buffer->device->path);
	dir_list = sysfs_open_directory_list(path);
	if (!dir_list)
		return NULL;

	scan_elements = dlist_new(sizeof(struct iio_scan_element));

	dlist_for_each_data(dir_list, dir, char) {
		if (check_postfix(dir, "en")) {
			struct iio_scan_element *elem = calloc(1, sizeof(struct iio_scan_element));
			if (!elem)
				continue;

			strncpy(elem->name, dir, SYSFS_NAME_LEN);
			strip_postfix(elem->name);
			sscanf(dir, "%dscan_", &(elem->index));
			elem->bits = iio_read_int_with_postfix(path, elem->name, "bits");
			elem->enabled = iio_read_int_with_postfix(path, elem->name, "en");

			// TODO add channel

			dlist_unshift_sorted(scan_elements, elem, sort_list);
		}
	}
	sysfs_close_list(dir_list);

	return scan_elements;
}


int iio_get_trigger(struct iio_device *iio_dev, char *trigger_name)
{
	return iio_read_string(trigger_name, "/trigger/current_trigger", iio_dev->path);
}

int iio_set_trigger(struct iio_device *iio_dev, const char *trigger_name)
{
	struct sysfs_attribute *trigger = NULL;
	char path[SYSFS_PATH_MAX];
	snprintf(path, SYSFS_PATH_MAX, "%s/trigger/current_trigger",
			iio_dev->path);
	trigger = sysfs_open_attribute(path);
	if (trigger == NULL)
		fail_return("Failed to open current_trigger file\n");

	sysfs_write_attribute(trigger, trigger_name, strlen(trigger_name));
	if ( sysfs_read_attribute(trigger) || strcmp(trigger->value, trigger_name) )
		fail_return("Failed to set trigger\n");

	sysfs_close_attribute(trigger);
	return 0;
}

