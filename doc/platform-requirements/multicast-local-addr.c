// SPDX-License-Identifier: GPL-2.0-or-later

/* multicast-local-addr.c
 *
 * Check behaviour of local address assignment when sending to multicast
 * destinations.
 *
 * Copyright Red Hat
 * Author: David Gibson <david@gibson.dropbear.id.au>
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

static struct sockaddr_in6 sa6_from_str(const char *addr, in_port_t port)
{
	struct sockaddr_in6 sa = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(port),
	};

	if (!inet_pton(AF_INET6, addr, &sa.sin6_addr))
		die("Couldn't parse address %s\n", addr);

	return sa;
}

#define INET6_SADDRSTRLEN	(INET6_ADDRSTRLEN + 7)

static const char *str_from_sa6(const struct sockaddr_in6 *sa,
				char *buf, size_t size)
{
	char astr[INET6_ADDRSTRLEN];

	if (!sa) {
		if (snprintf(buf, size, "[*]:?") >= size)
			die("Failed to format address");
		return buf;
	}

	if (!inet_ntop(AF_INET6, &sa->sin6_addr, astr, sizeof(astr)))
		die("Failed to format address");

	if (snprintf(buf, size, "[%s]:%hu", astr, ntohs(sa->sin6_port)) >= size)
		die("Failed to format address");

	return buf;
}

/* Address used by bittorrent for local peer discovery
 * This is a site-local multicast address.
 */
#define BITTORRENT_PORT		6771
#define BITTORRENT_MC		sa6_from_str("ff15::efc0:988f", BITTORRENT_PORT)

static void test_one(const struct sockaddr_in6 local,
		     const struct sockaddr_in6 remote,
		     int do_connect)
{
	char lstr[INET6_SADDRSTRLEN], rstr[INET6_SADDRSTRLEN];
	struct sockaddr_in6 la;
	socklen_t addrlen = sizeof(la);
	const char data[] = "TEST";
	int s;

	printf("Testing %s -> %s %s\n", str_from_sa6(&local, lstr, sizeof(lstr)),
	       str_from_sa6(&remote, rstr, sizeof(rstr)),
	       do_connect ? "(connected)" : "");

	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		die("socket() failed: %s\n", strerror(errno));
	
	if (bind(s, (const struct sockaddr *)&local, sizeof(local)))
		die("bind() to %s failed: %s\n", lstr, strerror(errno));
	if (do_connect &&
	    connect(s, (const struct sockaddr *)&remote, sizeof(remote)))
		die("connect() to %s failed: %s\n", rstr, strerror(errno));

	if (do_connect) {
		if (send(s, data, sizeof(data), 0) < 0)
			die("send() failed: %s\n", strerror(errno));
	} else {
		if (sendto(s, data, sizeof(data), 0,
			   (const struct sockaddr *)&remote, sizeof(remote)) < 0)
			die("sendto() failed: %s\n", strerror(errno));
	}

	if (getsockname(s, (struct sockaddr *)&la, &addrlen))
		die("getsockname() failed: %s\n", strerror(errno));

	printf("Final local address: %s\n",
	       str_from_sa6(&la, lstr, sizeof(lstr)));
	close(s);
}

static void test_dest(const struct sockaddr_in6 remote)
{
	in_port_t rport = ntohs(remote.sin6_port);
	const char *srcaddr[] = {"::", "::1"};
	const in_port_t srcport[] = {0, rport};
	int i, j, k;

	for (i = 0; i < ARRAY_SIZE(srcaddr); i++) {
		for (j = 0; j < ARRAY_SIZE(srcport); j++) {
			struct sockaddr_in6 local;

			local = sa6_from_str(srcaddr[i], srcport[j]);

			for (k = 0; k <= 1; k++)
				test_one(local, remote, k);
		}
	}
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	test_dest(BITTORRENT_MC);

	printf("So far so good\n");
	exit(0);
}
