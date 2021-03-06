/* tcp6.c - KNoT Application Client */

/*
 * Copyright (c) 2018, CESAR. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
#include <errno.h>
#include <stdio.h>

#include <net/net_pkt.h>
#include <net/net_core.h>
#include <net/net_context.h>

#include <net/net_app.h>

#include "net.h"
#include "tcp6.h"

LOG_MODULE_DECLARE(knot, LOG_LEVEL_DBG);

#define PEER_IPV6_PORT 8886

#define BUF_TIMEOUT	K_MSEC(100)
#define WAIT_TIME	K_SECONDS(10)
#define CONNECT_TIME	K_SECONDS(10)

static struct net_app_ctx tcp6;
static net_recv_t recv;

static void tcp6_close(struct net_app_ctx *ctx,
			 int status,
			 void *user_data)
{
	net_close_t net_close_cb = user_data;

	LOG_DBG("TCP(%p) disconnected", ctx);
	net_close_cb();
}

static void tcp6_received(struct net_app_ctx *ctx,
			 struct net_pkt *ipkt,
			 int status,
			 void *user_data)
{
	struct net_buf *netbuf;
	size_t ilen;
	size_t dlen;
	size_t hlen;

	ilen = net_pkt_get_len(ipkt);
	dlen = net_pkt_appdatalen(ipkt);

	if (ilen != dlen) {
		netbuf = ipkt->frags;
		hlen = net_pkt_appdata(ipkt) - netbuf->data;
		net_buf_pull(netbuf, hlen);
		recv(netbuf);
	}

	net_pkt_unref(ipkt);
}

static void tcp6_connected(struct net_app_ctx *ctx,
			  int status,
			  void *user_data)
{
	LOG_DBG("TCP(%p) connected", ctx);
}

static int connect_tcp6(struct net_app_ctx *ctx,
			const char *peer,
			int port,
			void *user_data)
{
	int ret;

	LOG_DBG("TCP(%p) Connecting to %s %d ...", ctx, peer, port);

	ret = net_app_init_tcp_client(ctx, NULL, NULL, peer, port,
				      WAIT_TIME, user_data);
	if (ret < 0) {
		LOG_ERR("Cannot init %s TCP client (%d)", peer, ret);
		goto fail;
	}

	ret = net_app_set_cb(ctx, tcp6_connected, tcp6_received, NULL,
								tcp6_close);
	if (ret < 0) {
		LOG_ERR("Cannot set callbacks (%d)", ret);
		goto fail;
	}

	ret = net_app_connect(ctx, CONNECT_TIME);
	if (ret < 0) {
		LOG_ERR("Cannot connect TCP (%d)", ret);
		goto fail;
	}

fail:
	return ret;
}

int tcp6_start(net_recv_t recv_cb, net_close_t close_cb)
{
	int ret = -EPERM;

	LOG_DBG("Starting TCP IPv6(%p) ...", &tcp6);

	recv = recv_cb;

	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		ret = connect_tcp6(&tcp6, CONFIG_NET_CONFIG_PEER_IPV6_ADDR,
				  PEER_IPV6_PORT, close_cb);
		if (ret < 0)
			LOG_ERR("Cannot init IPv6 TCP client (%d)", ret);
	}

	return ret;
}

void tcp6_stop(void)
{
	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		net_app_close(&tcp6);
		net_app_release(&tcp6);
	}
}

int tcp6_send(const u8_t *opdu, size_t olen)
{
	struct net_pkt *opkt;
	int ret;

	opkt = net_app_get_net_pkt(&tcp6, AF_UNSPEC, BUF_TIMEOUT);
	if (!opkt)
		return -EIO;

	olen = net_pkt_append(opkt, olen, opdu, K_FOREVER);

	ret = net_app_send_pkt(&tcp6, opkt, NULL, 0, K_FOREVER, NULL);
	if (ret < 0)
		net_pkt_unref(opkt);

	return 0;
}
