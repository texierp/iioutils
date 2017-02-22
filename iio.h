/*
 * Prototypes, structure definitions and macros.
 *
 * Copyright (c) 2009 Manuel Stahl <manuel.stahl@iis.fraunhofer.de>
 *
 * This library is covered by the LGPL, read LICENSE for details.
 *
 * This file (and only this file) may alternatively be licensed under the
 * BSD license as well, read LICENSE for details.
 */

#ifndef __IIO_H__
#define __IIO_H__

#include <limits.h>
#include <stdint.h>
#include <sysfs/libsysfs.h>
#include <sysfs/dlist.h>

#ifndef dlist_for_each_data
#define dlist_for_each_data(a,b,c)
#endif

#define IIO_DEV_DIR 	"/dev/iio/"

#define IIO_MOD_RAW 	"raw"
#define IIO_MOD_SCALE 	"scale"
#define IIO_MOD_OFFSET 	"offset"

#define IIO_EVENT_CODE_RING_50_FULL 200
#define IIO_EVENT_CODE_RING_75_FULL 201
#define IIO_EVENT_CODE_RING_100_FULL 202

enum sensor_type {
	SENSOR_ACCEL,
	SENSOR_GYRO,
	SENSOR_MAGN,
	SENSOR_TEMP,
	SENSOR_BARO,
	SENSOR_VOLT,
	SENSOR_UNKOWN
};
const char* sensor_prefix[SENSOR_UNKOWN];

struct iio_event_data {
	int id;
	int64_t timestamp;
};

/* name has to be the first element in all
 * structs to allow sorting.
 */
struct iio_device {
	char name[SYSFS_NAME_LEN];
	char path[SYSFS_PATH_MAX];
	unsigned number;
/*	char module[SYSFS_NAME_LEN]; */
	struct iio_ring_buffer *buffer;
	struct dlist *channellist;
};

struct iio_channel {
	char name[SYSFS_NAME_LEN];
	struct iio_device *dev;
	float raw;
	float scale;
	float offset;
	enum sensor_type type;
};

struct iio_ring_buffer {
	unsigned number;
	char path[SYSFS_PATH_MAX];
	char event[SYSFS_PATH_MAX];
	char access[SYSFS_PATH_MAX];
	struct iio_device *device;
};

struct iio_scan_element {
	char name[SYSFS_NAME_LEN];
	unsigned index;
	unsigned bits;
	int enabled;
	struct iio_channel *channel;
};

static inline void iio_name_from_attribute(char *name, const char *attr_name) {
	snprintf(name, strlen(attr_name) - strlen(IIO_MOD_RAW), "%s", attr_name);
}

void iio_close_device(struct iio_device *iio_dev);
struct iio_device *iio_open_device_from_sysfs(struct sysfs_device *sysfs_dev);
struct iio_device *iio_open_device_by_name(const char *name);
struct iio_device *iio_open_device_path(const char *path);

float iio_get_channel_modifier(struct iio_device *dev, const char *chan_name, const char *mod_name, float def_value);
struct dlist *iio_get_device_channels(struct iio_device *dev);

struct iio_ring_buffer *iio_get_ring_buffer(struct iio_device *iio_dev);
int iio_get_ring_buffer_bps(struct iio_ring_buffer *buf);
int iio_get_ring_buffer_length(struct iio_ring_buffer *buf);
int iio_is_ring_buffer_enabled(struct iio_ring_buffer *buf);

int iio_get_trigger(struct iio_device *iio_dev, char *trigger_name);
int iio_set_trigger(struct iio_device *dev, const char *trigger_name);

#endif /* __IIO_H__ */
