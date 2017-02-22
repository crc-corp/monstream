/*
 * Copyright (C) 2017  Minnesota Department of Transportation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <gtk/gtk.h>
#include <netdb.h>		/* for socket stuff */
#define _MULTI_THREADED
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>		/* for memset */


int32_t mongrid_init(uint32_t rows, uint32_t cols);
int32_t mongrid_play_stream(uint32_t row, uint32_t col, const char *loc,
	const char *desc, const char *stype);


int open_bind(const char *service) {
	struct addrinfo hints;
	struct addrinfo *ai;
	struct addrinfo *rai = NULL;
	int rc;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	rc = getaddrinfo(NULL, service, &hints, &rai);
	if (rc)
		return -1;

	for (ai = rai; ai; ai = ai->ai_next) {
		int fd = socket(ai->ai_family, ai->ai_socktype,
			ai->ai_protocol);
		if (fd >= 0) {
			if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
				freeaddrinfo(rai);
				return fd;
			} else
				close(fd);
		}
	}
	freeaddrinfo(rai);
	return -1;
}

static const char *param_end(const char *buf, const char *end) {
	const char *b = buf;
	while (b < end) {
		if ('\x1F' == *b)
			return b;
		++b;
	}
	return end;
}

static const char *param_next(const char *buf, const char *end) {
	const char *pe = param_end(buf, end);
	return (pe < end) ? pe + 1 : end;
}

static bool param_check(const char *val, const char *buf, const char *pe) {
	return memcmp(val, buf, pe - buf) == 0;
}

static char *nstr_cpy(char *dst, const char *dend, const char *src,
	const char *send)
{
	const char *se = param_end(src, send);
	size_t sn = se - src;
	size_t dn = dend - dst;
	size_t n = (sn < dn) ? sn : dn;
	memcpy(dst, src, n);
	return dst + n;
}

static char *nstr_cat(char *dst, const char *dend, const char *src) {
	const char *s = src;
	char *d = dst;
	while (d < dend) {
		if (0 == *s)
			break;
		*d = *s;
		d++;
		s++;
	}
	return d;
}

static void nstr_end(char *dst, const char *end) {
	if (dst >= end) {
		size_t n = 1 + (dst - end);
		dst -= n;
	}
	*dst = '\0';
}

static void process_play(const char *p2, const char *end) {
	const char *p3 = param_next(p2, end);	// stream URI
	const char *p4 = param_next(p3, end);	// stream type
	const char *p5 = param_next(p4, end);	// title
	char desc[128];
	char uri[128];
	char stype[16];
	const char *dend = desc + 128;
	const char *uend = uri + 128;
	const char *tend = stype + 16;
	char *u, *s;
	char *d = nstr_cpy(desc, dend, p2, end);
	if (p5 < end) {
		d = nstr_cat(d, dend, " --- ");
		d = nstr_cpy(d, dend, p5, end);
	}
//	d = nstr_cat(d, dend, "\n");
//	d = nstr_cpy(d, dend, p4, end);
	nstr_end(d, dend);
	u = nstr_cpy(uri, uend, p3, end);
	nstr_end(u, uend);
	s = nstr_cpy(stype, tend, p4, end);
	nstr_end(s, tend);
	mongrid_play_stream(0, 0, uri, desc, stype);
}

static void process_stop(const char *p2, const char *end) {
	printf("stop!\n");
}

static void process_monitor(const char *p2, const char *end) {
	printf("monitor!\n");
}

static void process_config(const char *p2, const char *end) {
	printf("config!\n");
}

static void process_command(const char *buf, size_t n) {
	const char *end = buf + n;
	const char *pe = param_end(buf, end);
	const char *p2 = param_next(pe, end);
	if (param_check("play", buf, pe))
		process_play(p2, end);
	else if (param_check("stop", buf, pe))
		process_stop(p2, end);
	else if (param_check("monitor", buf, pe))
		process_monitor(p2, end);
	else if (param_check("config", buf, pe))
		process_config(p2, end);
	else
		fprintf(stderr, "Invalid command: %s\n", buf);
}

static void *command_thread(void *data) {
	char buf[1024];
	int fd = open_bind("7001");
	while (true) {
		ssize_t n = read(fd, buf, 1023);
		if (n < 0) {
			fprintf(stderr, "Read error: %d\n", n);
			break;
		}
		buf[n] = '\0';
		process_command(buf, n);
	}
	close(fd);
}

int main(void) {
	pthread_t thread;
	int rc;

	if (mongrid_init(2, 2))
		return -1;
	rc = pthread_create(&thread, NULL, command_thread, NULL);
	mongrid_play_stream(0, 0, "rtsp://10.80.88.29/axis-media/media.amp",
		"C123 - Location A", "H264");
//	mongrid_play_stream(0, 1, "rtsp://10.80.88.30/axis-media/media.amp",
//		"C234 - Location B", "H264");
//	mongrid_play_stream(1, 0, "rtsp://10.80.88.31/axis-media/media.amp",
//		"C345 - Location C", "H264");
//	mongrid_play_stream(1, 1, "rtsp://10.80.88.32/axis-media/media.amp",
//		"C456 - Location D", "H264");
	gtk_main();

	return 0;
}
