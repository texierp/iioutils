# Process this file with 'automake' to create 'Makefile.in'

AM_CPPFLAGS = 

AM_CFLAGS = -Wall -W -Wunused -std=c99

sbin_PROGRAMS = lsiio iio_ring

lsiio_SOURCES = lsiio.c lib/iio_utils.c iio.h
lsiio_LDADD = -lm

iio_ring_SOURCES = iio_ring.c lib/iio_utils.c iio.h
iio_ring_LDADD = -lm

man_MANS = lsiio.8

EXTRA_DIST = $(man_MANS)
