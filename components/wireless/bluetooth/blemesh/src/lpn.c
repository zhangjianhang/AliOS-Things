/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

#ifdef CONFIG_BT_MESH_LOW_POWER

#include <stdint.h>
#include <zephyr.h>
#include <misc/byteorder.h>
#include <toolchain.h>

#include <net/buf.h>
#include <blemesh.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_MESH_DEBUG_LOW_POWER)
#include "common/log.h"

#include "access_internal.h"
#include "mesh_internal.h"
#include "mesh_crypto.h"
#include "adv.h"
#include "net.h"
#include "transport.h"
#include "beacon.h"
#include "foundation.h"
#include "lpn.h"
#include <errno.h>
#include "bt_mesh_custom_log.h"

#if defined(CONFIG_BT_MESH_LPN_AUTO)
#define LPN_AUTO_TIMEOUT          K_SECONDS(CONFIG_BT_MESH_LPN_AUTO_TIMEOUT)
#else
#define LPN_AUTO_TIMEOUT          0
#endif

#define LPN_RECV_DELAY            CONFIG_BT_MESH_LPN_RECV_DELAY
#define SCAN_LATENCY              min(CONFIG_BT_MESH_LPN_SCAN_LATENCY, \
				      LPN_RECV_DELAY)

#define FRIEND_REQ_RETRY_TIMEOUT  K_SECONDS(CONFIG_BT_MESH_LPN_RETRY_TIMEOUT)

#define FRIEND_REQ_WAIT           K_MSEC(100)
#define FRIEND_REQ_SCAN           K_SECONDS(1)
#define FRIEND_REQ_TIMEOUT        (FRIEND_REQ_WAIT + FRIEND_REQ_SCAN)

#define REQ_RETRY_TIMES_MAX  12  // should not < 8

#define POLL_RETRY_TIMEOUT        K_MSEC(100)
#define POLL_RETRY_TIMEOUT_MAX    K_MSEC(800)

#define REQ_RETRY_DURATION(lpn)  (REQ_RETRY_TIMES_MAX * (LPN_RECV_DELAY + (lpn)->adv_duration + \
				  (lpn)->recv_win) + 36 * POLL_RETRY_TIMEOUT +  (REQ_RETRY_TIMES_MAX - 8) * POLL_RETRY_TIMEOUT_MAX)

#define POLL_TIMEOUT_INIT     (CONFIG_BT_MESH_LPN_INIT_POLL_TIMEOUT * 100)
#define POLL_TIMEOUT_MAX(lpn) ((CONFIG_BT_MESH_LPN_POLL_TIMEOUT * 100) - \
			       REQ_RETRY_DURATION(lpn))

#define REQ_ATTEMPTS(lpn)     (POLL_TIMEOUT_MAX(lpn) < K_SECONDS(3) ? 2 : REQ_RETRY_TIMES_MAX)

#define CLEAR_ATTEMPTS        16

#define LPN_CRITERIA ((CONFIG_BT_MESH_LPN_MIN_QUEUE_SIZE) | \
		      (CONFIG_BT_MESH_LPN_RSSI_FACTOR << 3) | \
		      (CONFIG_BT_MESH_LPN_RECV_WIN_FACTOR << 5))

#define POLL_TO(to) { (u8_t)((to) >> 16), (u8_t)((to) >> 8), (u8_t)(to) }
#define LPN_POLL_TO POLL_TO(CONFIG_BT_MESH_LPN_POLL_TIMEOUT)

/* 3 transmissions, 20ms interval */
#define POLL_XMIT BT_MESH_TRANSMIT(2, 20)

static void (*lpn_cb)(u16_t friend_addr, bool established);

#if defined(CONFIG_BT_MESH_DEBUG_LOW_POWER)
static const char *state2str(int state)
{
	switch (state) {
	case BT_MESH_LPN_DISABLED:
		return "disabled";
	case BT_MESH_LPN_CLEAR:
		return "clear";
	case BT_MESH_LPN_TIMER:
		return "timer";
	case BT_MESH_LPN_ENABLED:
		return "enabled";
	case BT_MESH_LPN_REQ_WAIT:
		return "req wait";
	case BT_MESH_LPN_WAIT_OFFER:
		return "wait offer";
	case BT_MESH_LPN_ESTABLISHED:
		return "established";
	case BT_MESH_LPN_RECV_DELAY:
		return "recv delay";
	case BT_MESH_LPN_WAIT_UPDATE:
		return "wait update";
	default:
		return "(unknown)";
	}
}
#endif /* CONFIG_BT_MESH_DEBUG_LPN */

static inline void lpn_set_state(int state)
{
#if defined(CONFIG_BT_MESH_DEBUG_LOW_POWER)
	BT_DBG("%s -> %s", state2str(bt_mesh.lpn.state), state2str(state));
#endif
	bt_mesh.lpn.state = state;
}

static inline void group_zero(atomic_t *target)
{
#if CONFIG_BT_MESH_LPN_GROUPS > 32
	int i;

	for (i = 0; i < ARRAY_SIZE(bt_mesh.lpn.added); i++) {
		atomic_set(&target[i], 0);
	}
#else
	atomic_set(target, 0);
#endif
}

static inline void group_set(atomic_t *target, atomic_t *source)
{
#if CONFIG_BT_MESH_LPN_GROUPS > 32
	int i;

	for (i = 0; i < ARRAY_SIZE(bt_mesh.lpn.added); i++) {
		atomic_or(&target[i], atomic_get(&source[i]));
	}
#else
	atomic_or(target, atomic_get(source));
#endif
}

static inline void group_clear(atomic_t *target, atomic_t *source)
{
#if CONFIG_BT_MESH_LPN_GROUPS > 32
	int i;

	for (i = 0; i < ARRAY_SIZE(bt_mesh.lpn.added); i++) {
		atomic_and(&target[i], ~atomic_get(&source[i]));
	}
#else
	atomic_and(target, ~atomic_get(source));
#endif
}

static void clear_friendship(bool force, bool disable);

static void friend_clear_sent(int err, void *user_data)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

	/* We're switching away from Low Power behavior, so permanently
	 * enable scanning.
	 */
	bt_mesh_scan_enable();

	lpn->req_attempts++;

	if (err) {
		BT_ERR("Sending Friend Request failed (err %d)", err);
		lpn_set_state(BT_MESH_LPN_ENABLED);
		clear_friendship(false, lpn->disable);
		return;
	}

	lpn_set_state(BT_MESH_LPN_CLEAR);
	k_delayed_work_submit(&lpn->timer, FRIEND_REQ_TIMEOUT);
}

static const struct bt_mesh_send_cb clear_sent_cb = {
	.end = friend_clear_sent,
};

static int send_friend_clear(void)
{
	struct bt_mesh_msg_ctx ctx = {
		.net_idx     = bt_mesh.sub[0].net_idx,
		.app_idx     = BT_MESH_KEY_UNUSED,
		.addr        = bt_mesh.lpn.frnd,
		.send_ttl    = 0,
	};
	struct bt_mesh_net_tx tx = {
		.sub = &bt_mesh.sub[0],
		.ctx = &ctx,
		.src = bt_mesh_primary_addr(),
		.xmit = bt_mesh_net_transmit_get(),
	};
	struct bt_mesh_ctl_friend_clear req = {
		.lpn_addr    = sys_cpu_to_be16(tx.src),
		.lpn_counter = sys_cpu_to_be16(bt_mesh.lpn.counter),
	};

	BT_DBG("");

	return bt_mesh_ctl_send(&tx, TRANS_CTL_OP_FRIEND_CLEAR, &req,
				sizeof(req), NULL, &clear_sent_cb, NULL);
}

static void clear_friendship(bool force, bool disable)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

	BT_DBG("force %u disable %u", force, disable);

	if (!force && lpn->established && !lpn->clear_success &&
	    lpn->req_attempts < CLEAR_ATTEMPTS) {
		send_friend_clear();
		lpn->disable = disable;
		return;
	}

	bt_mesh_rx_reset();

	k_delayed_work_cancel(&lpn->timer);

	friend_cred_del(bt_mesh.sub[0].net_idx, lpn->frnd);

	if (lpn->clear_success) {
		lpn->old_friend = BT_MESH_ADDR_UNASSIGNED;
	} else {
		lpn->old_friend = lpn->frnd;
	}

	if (lpn_cb && lpn->frnd != BT_MESH_ADDR_UNASSIGNED) {
		lpn_cb(lpn->frnd, false);
	}

	lpn->frnd = BT_MESH_ADDR_UNASSIGNED;
	lpn->fsn = 0;
	lpn->req_attempts = 0;
	lpn->recv_win = 0;
	lpn->queue_size = 0;
	lpn->disable = 0;
	lpn->sent_req = 0;
	lpn->established = 0;
	lpn->clear_success = 0;

	group_zero(lpn->added);
	group_zero(lpn->pending);
	group_zero(lpn->to_remove);

	/* Set this to 1 to force group subscription when the next
	 * Friendship is created, in case lpn->groups doesn't get
	 * modified meanwhile.
	 */
	lpn->groups_changed = 1;

	if (disable) {
		lpn_set_state(BT_MESH_LPN_DISABLED);
		return;
	}

	lpn_set_state(BT_MESH_LPN_ENABLED);
	k_delayed_work_submit(&lpn->timer, FRIEND_REQ_RETRY_TIMEOUT);
}

static void friend_req_sent(u16_t duration, int err, void *user_data)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

	if (err) {
		BT_ERR("Sending Friend Request failed (err %d)", err);
		return;
	}

	BT_DBG("Sending Friend Request succeed.");

	lpn->adv_duration = duration;

	if (IS_ENABLED(CONFIG_BT_MESH_LPN_ESTABLISHMENT)) {
		k_delayed_work_submit(&lpn->timer, FRIEND_REQ_WAIT);
		lpn_set_state(BT_MESH_LPN_REQ_WAIT);
	} else {
		k_delayed_work_submit(&lpn->timer,
				      duration + FRIEND_REQ_TIMEOUT);
		lpn_set_state(BT_MESH_LPN_WAIT_OFFER);
	}
}

static const struct bt_mesh_send_cb friend_req_sent_cb = {
	.start = friend_req_sent,
};

static int send_friend_req(struct bt_mesh_lpn *lpn)
{
	const struct bt_mesh_comp *comp = bt_mesh_comp_get();
	struct bt_mesh_msg_ctx ctx = {
		.net_idx  = bt_mesh.sub[0].net_idx,
		.app_idx  = BT_MESH_KEY_UNUSED,
		.addr     = BT_MESH_ADDR_FRIENDS,
		.send_ttl = 0,
	};
	struct bt_mesh_net_tx tx = {
		.sub = &bt_mesh.sub[0],
		.ctx = &ctx,
		.src = bt_mesh_primary_addr(),
		.xmit = POLL_XMIT,
	};
	struct bt_mesh_ctl_friend_req req = {
		.criteria    = LPN_CRITERIA,
		.recv_delay  = LPN_RECV_DELAY,
		.poll_to     = LPN_POLL_TO,
		.prev_addr   = lpn->old_friend,
		.num_elem    = comp->elem_count,
		.lpn_counter = sys_cpu_to_be16(lpn->counter),
	};

	BT_DBG("");

	return bt_mesh_ctl_send(&tx, TRANS_CTL_OP_FRIEND_REQ, &req,
				sizeof(req), NULL, &friend_req_sent_cb, NULL);
}

static void req_sent(u16_t duration, int err, void *user_data)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

#if defined(CONFIG_BT_MESH_DEBUG_LOW_POWER)
	BT_DBG("req 0x%02x duration %u err %d state %s",
	       lpn->sent_req, duration, err, state2str(lpn->state));
#endif

	if (err) {
		BT_ERR("Sending request failed (err %d)", err);
		lpn->sent_req = 0;
		group_zero(lpn->pending);
		return;
	}

	lpn->req_attempts++;
	lpn->adv_duration = duration;

	if (lpn->established || IS_ENABLED(CONFIG_BT_MESH_LPN_ESTABLISHMENT)) {
		lpn_set_state(BT_MESH_LPN_RECV_DELAY);
		/* We start scanning a bit early to elimitate risk of missing
		 * response data due to HCI and other latencies.
		 */
		k_delayed_work_submit(&lpn->timer,
				      LPN_RECV_DELAY - SCAN_LATENCY);
	} else {
		lpn_set_state(BT_MESH_LPN_WAIT_UPDATE);
		k_delayed_work_submit(&lpn->timer,
				      LPN_RECV_DELAY + duration +
				      lpn->recv_win);
	}
}

static const struct bt_mesh_send_cb req_sent_cb = {
	.start = req_sent,
};

static int send_friend_poll(void)
{
	struct bt_mesh_msg_ctx ctx = {
		.net_idx     = bt_mesh.sub[0].net_idx,
		.app_idx     = BT_MESH_KEY_UNUSED,
		.addr        = bt_mesh.lpn.frnd,
		.send_ttl    = 0,
	};
	struct bt_mesh_net_tx tx = {
		.sub = &bt_mesh.sub[0],
		.ctx = &ctx,
		.src = bt_mesh_primary_addr(),
		.xmit = POLL_XMIT,
		.friend_cred = true,
	};
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;
	u8_t fsn = lpn->fsn;
	int err;

	BT_DBG("%s: lpn->sent_req 0x%02x", __func__, lpn->sent_req);

	if (lpn->sent_req) {
		BT_DBG("%s %d", __func__, __LINE__);
		if (lpn->sent_req != TRANS_CTL_OP_FRIEND_POLL) {
			lpn->pending_poll = 1;
		}
		return 0;
	}

	BT_DBG("%s %d", __func__, __LINE__);

	err = bt_mesh_ctl_send(&tx, TRANS_CTL_OP_FRIEND_POLL, &fsn, 1,
			       NULL, &req_sent_cb, NULL);
	if (err == 0) {
		BT_DBG("%s %d", __func__, __LINE__);
		lpn->pending_poll = 0;
		lpn->sent_req = TRANS_CTL_OP_FRIEND_POLL;
	}

	BT_DBG("%s %d", __func__, __LINE__);
	return err;
}

void bt_mesh_lpn_disable(bool force)
{
	if (bt_mesh.lpn.state == BT_MESH_LPN_DISABLED) {
		return;
	}

	clear_friendship(force, true);
}

int bt_mesh_lpn_set(bool enable)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

	if (enable) {
		if (lpn->state != BT_MESH_LPN_DISABLED) {
			return 0;
		}
	} else {
		if (lpn->state == BT_MESH_LPN_DISABLED) {
			return 0;
		}
	}

	if (!bt_mesh_is_provisioned()) {
		if (enable) {
			lpn_set_state(BT_MESH_LPN_ENABLED);
		} else {
			lpn_set_state(BT_MESH_LPN_DISABLED);
		}

		return 0;
	}

	if (enable) {
		lpn_set_state(BT_MESH_LPN_ENABLED);

		if (IS_ENABLED(CONFIG_BT_MESH_LPN_ESTABLISHMENT)) {
			bt_mesh_scan_disable();
		}

		send_friend_req(lpn);
	} else {
		if (IS_ENABLED(CONFIG_BT_MESH_LPN_AUTO) &&
		    lpn->state == BT_MESH_LPN_TIMER) {
			k_delayed_work_cancel(&lpn->timer);
			lpn_set_state(BT_MESH_LPN_DISABLED);
		} else {
			bt_mesh_lpn_disable(false);
		}
	}

	return 0;
}

static void friend_response_received(struct bt_mesh_lpn *lpn)
{
	BT_DBG("%s: lpn->sent_req 0x%02x", __func__, lpn->sent_req);

	if (lpn->sent_req == TRANS_CTL_OP_FRIEND_POLL) {
		lpn->fsn++;
	}

	k_delayed_work_cancel(&lpn->timer);
	bt_mesh_scan_disable();
	lpn_set_state(BT_MESH_LPN_ESTABLISHED);
	lpn->req_attempts = 0;
	lpn->sent_req = 0;
}

void bt_mesh_lpn_msg_received(struct bt_mesh_net_rx *rx)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

	if (lpn->state == BT_MESH_LPN_TIMER) {
		BT_DBG("Restarting establishment timer");
		k_delayed_work_submit(&lpn->timer, LPN_AUTO_TIMEOUT);
		return;
	}

	if (lpn->sent_req != TRANS_CTL_OP_FRIEND_POLL) {
		BT_WARN("Unexpected message withouth a preceding Poll");
		return;
	}

	friend_response_received(lpn);

	BT_DBG("Requesting more messages from Friend");

	send_friend_poll();
}

int bt_mesh_lpn_friend_offer(struct bt_mesh_net_rx *rx,
			     struct net_buf_simple *buf)
{
	struct bt_mesh_ctl_friend_offer *msg = (void *)buf->data;
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;
	struct bt_mesh_subnet *sub = rx->sub;
	struct friend_cred *cred;
	u16_t frnd_counter;
	int err;

	if (buf->len < sizeof(*msg)) {
		BT_WARN("Too short Friend Offer");
		return -EINVAL;
	}

	if (lpn->state != BT_MESH_LPN_WAIT_OFFER) {
		BT_WARN("Ignoring unexpected Friend Offer");
		return 0;
	}

	if (!msg->recv_win) {
		BT_WARN("Prohibited ReceiveWindow value");
		return -EINVAL;
	}

	frnd_counter = sys_be16_to_cpu(msg->frnd_counter);

	BT_DBG("recv_win %u queue_size %u sub_list_size %u rssi %d counter %u",
	       msg->recv_win, msg->queue_size, msg->sub_list_size, msg->rssi,
	       frnd_counter);

	lpn->frnd = rx->ctx.addr;

	cred = friend_cred_create(sub, lpn->frnd, lpn->counter, frnd_counter);
	if (!cred) {
		BT_DBG("friend_cred_create failed.");
		lpn->frnd = BT_MESH_ADDR_UNASSIGNED;
		return -ENOMEM;
	}

	/* TODO: Add offer acceptance criteria check */

	k_delayed_work_cancel(&lpn->timer);

	lpn->recv_win = msg->recv_win;
	lpn->queue_size = msg->queue_size;

	BT_DBG("Will send_friend_poll");

	err = send_friend_poll();
	if (err) {
		BT_DBG("send_friend_poll failed.");
		friend_cred_clear(cred);
		lpn->frnd = BT_MESH_ADDR_UNASSIGNED;
		lpn->recv_win = 0;
		lpn->queue_size = 0;
		return err;
	}

	BT_DBG("send_friend_poll succeed.");

	lpn->counter++;

	return 0;
}

int bt_mesh_lpn_friend_clear_cfm(struct bt_mesh_net_rx *rx,
				 struct net_buf_simple *buf)
{
	struct bt_mesh_ctl_friend_clear_confirm *msg = (void *)buf->data;
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;
	u16_t addr, counter;

	if (buf->len < sizeof(*msg)) {
		BT_WARN("Too short Friend Clear Confirm");
		return -EINVAL;
	}

	if (lpn->state != BT_MESH_LPN_CLEAR) {
		BT_WARN("Ignoring unexpected Friend Clear Confirm");
		return 0;
	}

	addr = sys_be16_to_cpu(msg->lpn_addr);
	counter = sys_be16_to_cpu(msg->lpn_counter);

	BT_DBG("LPNAddress 0x%04x LPNCounter 0x%04x", addr, counter);

	if (addr != bt_mesh_primary_addr() || counter != lpn->counter) {
		BT_WARN("Invalid parameters in Friend Clear Confirm");
		return 0;
	}

	lpn->clear_success = 1;
	clear_friendship(false, lpn->disable);

	return 0;
}

static void lpn_group_add(u16_t group)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;
	u16_t *free_slot = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(lpn->groups); i++) {
		if (lpn->groups[i] == group) {
			atomic_clear_bit(lpn->to_remove, i);
			return;
		}

		if (!free_slot && lpn->groups[i] == BT_MESH_ADDR_UNASSIGNED) {
			free_slot = &lpn->groups[i];
		}
	}

	if (!free_slot) {
		BT_WARN("Friend Subscription List exceeded!");
		return;
	}

	*free_slot = group;
	lpn->groups_changed = 1;
}

static void lpn_group_del(u16_t group)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;
	int i;

	for (i = 0; i < ARRAY_SIZE(lpn->groups); i++) {
		if (lpn->groups[i] == group) {
			if (atomic_test_bit(lpn->added, i) ||
			    atomic_test_bit(lpn->pending, i)) {
				atomic_set_bit(lpn->to_remove, i);
				lpn->groups_changed = 1;
			} else {
				lpn->groups[i] = BT_MESH_ADDR_UNASSIGNED;
			}
		}
	}
}

static inline int group_popcount(atomic_t *target)
{
#if CONFIG_BT_MESH_LPN_GROUPS > 32
	int i, count = 0;

	for (i = 0; i < ARRAY_SIZE(bt_mesh.lpn.added); i++) {
		count += popcount(atomic_get(&target[i]));
	}
#else
	return popcount(atomic_get(target));
#endif
}

static bool sub_update(u8_t op)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;
	int added_count = group_popcount(lpn->added);
	struct bt_mesh_msg_ctx ctx = {
		.net_idx     = bt_mesh.sub[0].net_idx,
		.app_idx     = BT_MESH_KEY_UNUSED,
		.addr        = lpn->frnd,
		.send_ttl    = 0,
	};
	struct bt_mesh_net_tx tx = {
		.sub = &bt_mesh.sub[0],
		.ctx = &ctx,
		.src = bt_mesh_primary_addr(),
		.xmit = POLL_XMIT,
		.friend_cred = true,
	};
	struct bt_mesh_ctl_friend_sub req;
	size_t i, g;

	BT_DBG("op 0x%02x sent_req 0x%02x", op, lpn->sent_req);

	if (lpn->sent_req) {
		return false;
	}

	for (i = 0, g = 0; i < ARRAY_SIZE(lpn->groups); i++) {
		if (lpn->groups[i] == BT_MESH_ADDR_UNASSIGNED) {
			continue;
		}

		if (op == TRANS_CTL_OP_FRIEND_SUB_ADD) {
			if (atomic_test_bit(lpn->added, i)) {
				continue;
			}
		} else {
			if (!atomic_test_bit(lpn->to_remove, i)) {
				continue;
			}
		}

		if (added_count + g >= lpn->queue_size) {
			BT_WARN("Friend Queue Size exceeded");
			break;
		}

		req.addr_list[g++] = sys_cpu_to_be16(lpn->groups[i]);
		atomic_set_bit(lpn->pending, i);

		if (g == ARRAY_SIZE(req.addr_list)) {
			break;
		}
	}

	if (g == 0) {
		group_zero(lpn->pending);
		return false;
	}

	req.xact = lpn->xact_next++;

	if (bt_mesh_ctl_send(&tx, op, &req, 1 + g * 2, NULL,
			     &req_sent_cb, NULL) < 0) {
		group_zero(lpn->pending);
		return false;
	}

	lpn->xact_pending = req.xact;
	lpn->sent_req = op;
	return true;
}

static void update_timeout(struct bt_mesh_lpn *lpn)
{
	if (lpn->established) {
                int delay_timeout;

		BT_WARN("No response from Friend during ReceiveWindow");
		bt_mesh_scan_disable();
		lpn_set_state(BT_MESH_LPN_ESTABLISHED);
                delay_timeout = (lpn->req_attempts + 1) * POLL_RETRY_TIMEOUT;
                if (delay_timeout > POLL_RETRY_TIMEOUT_MAX) {
                    delay_timeout = POLL_RETRY_TIMEOUT_MAX;
                }
		k_delayed_work_submit(&lpn->timer, delay_timeout);
	} else {
		if (IS_ENABLED(CONFIG_BT_MESH_LPN_ESTABLISHMENT)) {
			bt_mesh_scan_disable();
		}

		if (lpn->req_attempts < REQ_RETRY_TIMES_MAX * 2) {
			BT_WARN("Retrying first Friend Poll");
			lpn->sent_req = 0;
			if (send_friend_poll() == 0) {
				return;
			}
		}

		BT_ERR("Timed out waiting for first Friend Update");
		clear_friendship(false, false);
	}
}

static void lpn_timeout(struct k_work *work)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

#if defined(CONFIG_BT_MESH_DEBUG_LOW_POWER)
	BT_DBG("state: %s", state2str(lpn->state));
#endif

	switch (lpn->state) {
	case BT_MESH_LPN_DISABLED:
		break;
	case BT_MESH_LPN_CLEAR:
		clear_friendship(false, bt_mesh.lpn.disable);
		break;
	case BT_MESH_LPN_TIMER:
		BT_DBG("Starting to look for Friend nodes");
		lpn_set_state(BT_MESH_LPN_ENABLED);
		if (IS_ENABLED(CONFIG_BT_MESH_LPN_ESTABLISHMENT)) {
			bt_mesh_scan_disable();
		}
		/* fall through */
	case BT_MESH_LPN_ENABLED:
		send_friend_req(lpn);
		break;
	case BT_MESH_LPN_REQ_WAIT:
		bt_mesh_scan_enable();
		k_delayed_work_submit(&lpn->timer,
				      lpn->adv_duration + FRIEND_REQ_SCAN);
		lpn_set_state(BT_MESH_LPN_WAIT_OFFER);
		break;
	case BT_MESH_LPN_WAIT_OFFER:
		BT_WARN("No acceptable Friend Offers received");
		if (IS_ENABLED(CONFIG_BT_MESH_LPN_ESTABLISHMENT)) {
			bt_mesh_scan_disable();
		}
		lpn->counter++;
		lpn_set_state(BT_MESH_LPN_ENABLED);
		k_delayed_work_submit(&lpn->timer, FRIEND_REQ_RETRY_TIMEOUT);
		break;
	case BT_MESH_LPN_ESTABLISHED:
		if (lpn->req_attempts < REQ_ATTEMPTS(lpn)) {
			u8_t req = lpn->sent_req;

			lpn->sent_req = 0;

			if (!req || req == TRANS_CTL_OP_FRIEND_POLL) {
				send_friend_poll();
			} else {
				sub_update(req);
			}

			break;
		}

		BT_ERR("No response from Friend after %u retries",
		       lpn->req_attempts);
		lpn->req_attempts = 0;
		clear_friendship(false, false);
		break;
	case BT_MESH_LPN_RECV_DELAY:
		k_delayed_work_submit(&lpn->timer,
				      lpn->adv_duration + SCAN_LATENCY +
				      lpn->recv_win);
		bt_mesh_scan_enable();
		lpn_set_state(BT_MESH_LPN_WAIT_UPDATE);
		break;
	case BT_MESH_LPN_WAIT_UPDATE:
		update_timeout(lpn);
		break;
	default:
		__ASSERT(0, "Unhandled LPN state");
		break;
	}
}

void bt_mesh_lpn_group_add(u16_t group)
{
	BT_DBG("group 0x%04x", group);

	lpn_group_add(group);

	if (!bt_mesh_lpn_established() || bt_mesh.lpn.sent_req) {
		return;
	}

	sub_update(TRANS_CTL_OP_FRIEND_SUB_ADD);
}

void bt_mesh_lpn_group_del(u16_t *groups, size_t group_count)
{
	int i;

	for (i = 0; i < group_count; i++) {
		if (groups[i] != BT_MESH_ADDR_UNASSIGNED) {
			BT_DBG("group 0x%04x", groups[i]);
			lpn_group_del(groups[i]);
		}
	}

	if (!bt_mesh_lpn_established() || bt_mesh.lpn.sent_req) {
		return;
	}

	sub_update(TRANS_CTL_OP_FRIEND_SUB_REM);
}

static s32_t poll_timeout(struct bt_mesh_lpn *lpn)
{
	/* If we're waiting for segment acks keep polling at high freq */
	if (bt_mesh_tx_in_progress()) {
		return min(POLL_TIMEOUT_MAX(lpn), K_SECONDS(1));
	}

	if (lpn->poll_timeout < POLL_TIMEOUT_MAX(lpn)) {
		lpn->poll_timeout *= 2;
		lpn->poll_timeout = min(lpn->poll_timeout,
					POLL_TIMEOUT_MAX(lpn));
	}

	BT_DBG("Poll Timeout is %ums", lpn->poll_timeout);

	return lpn->poll_timeout;
}

int bt_mesh_lpn_friend_sub_cfm(struct bt_mesh_net_rx *rx,
			       struct net_buf_simple *buf)
{
	struct bt_mesh_ctl_friend_sub_confirm *msg = (void *)buf->data;
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

	if (buf->len < sizeof(*msg)) {
		BT_WARN("Too short Friend Subscription Confirm");
		return -EINVAL;
	}

	BT_DBG("xact 0x%02x", msg->xact);

	if (!lpn->sent_req) {
		BT_WARN("No pending subscription list message");
		return 0;
	}

	if (msg->xact != lpn->xact_pending) {
		BT_WARN("Transaction mismatch (0x%02x != 0x%02x)",
			msg->xact, lpn->xact_pending);
		return 0;
	}

	if (lpn->sent_req == TRANS_CTL_OP_FRIEND_SUB_ADD) {
		group_set(lpn->added, lpn->pending);
		group_zero(lpn->pending);
	} else if (lpn->sent_req == TRANS_CTL_OP_FRIEND_SUB_REM) {
		int i;

		group_clear(lpn->added, lpn->pending);

		for (i = 0; i < ARRAY_SIZE(lpn->groups); i++) {
			if (atomic_test_and_clear_bit(lpn->pending, i) &&
			    atomic_test_and_clear_bit(lpn->to_remove, i)) {
				lpn->groups[i] = BT_MESH_ADDR_UNASSIGNED;
			}
		}
	} else {
		BT_WARN("Unexpected Friend Subscription Confirm");
		return 0;
	}

	friend_response_received(lpn);

	if (lpn->groups_changed) {
		sub_update(TRANS_CTL_OP_FRIEND_SUB_ADD);
		sub_update(TRANS_CTL_OP_FRIEND_SUB_REM);

		if (!lpn->sent_req) {
			lpn->groups_changed = 0;
		}
	}

	if (lpn->pending_poll) {
		send_friend_poll();
	}

	if (!lpn->sent_req) {
		k_delayed_work_submit(&lpn->timer, poll_timeout(lpn));
	}

	return 0;
}

int bt_mesh_lpn_friend_update(struct bt_mesh_net_rx *rx,
			      struct net_buf_simple *buf)
{
	struct bt_mesh_ctl_friend_update *msg = (void *)buf->data;
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;
	struct bt_mesh_subnet *sub = rx->sub;
	u32_t iv_index;

	if (buf->len < sizeof(*msg)) {
		BT_WARN("Too short Friend Update");
		return -EINVAL;
	}

	if (lpn->sent_req != TRANS_CTL_OP_FRIEND_POLL) {
		BT_WARN("Unexpected friend update");
		return 0;
	}

	if (sub->kr_phase == BT_MESH_KR_PHASE_2 && !rx->new_key) {
		BT_WARN("Ignoring Phase 2 KR Update secured using old key");
		return 0;
	}

	if (bt_mesh.ivu_initiator &&
	    bt_mesh.iv_update == BT_MESH_IV_UPDATE(msg->flags)) {
		bt_mesh_beacon_ivu_initiator(false);
	}

	if (!lpn->established) {
		/* This is normally checked on the transport layer, however
		 * in this state we're also still accepting master
		 * credentials so we need to ensure the right ones (Friend
		 * Credentials) were used for this message.
		 */
		if (!rx->friend_cred) {
			BT_WARN("Friend Update with wrong credentials");
			return -EINVAL;
		}

		lpn->established = 1;

		BT_INFO("Friendship established with 0x%04x", lpn->frnd);

		if (lpn_cb) {
			lpn_cb(lpn->frnd, true);
		}

		/* Set initial poll timeout */
		lpn->poll_timeout = min(POLL_TIMEOUT_MAX(lpn),
					POLL_TIMEOUT_INIT);
	}

	friend_response_received(lpn);

	iv_index = sys_be32_to_cpu(msg->iv_index);

	BT_DBG("flags 0x%02x iv_index 0x%08x md %u", msg->flags, iv_index,
	       msg->md);

	if (bt_mesh_kr_update(sub, BT_MESH_KEY_REFRESH(msg->flags),
			      rx->new_key)) {
		bt_mesh_net_beacon_update(sub);
	}

	bt_mesh_net_iv_update(iv_index, BT_MESH_IV_UPDATE(msg->flags));

	if (lpn->groups_changed) {
		sub_update(TRANS_CTL_OP_FRIEND_SUB_ADD);
		sub_update(TRANS_CTL_OP_FRIEND_SUB_REM);

		if (!lpn->sent_req) {
			lpn->groups_changed = 0;
		}
	}

	if (msg->md) {
		BT_DBG("Requesting for more messages");
		send_friend_poll();
	}

	if (!lpn->sent_req) {
		k_delayed_work_submit(&lpn->timer, poll_timeout(lpn));
	}

	return 0;
}

int bt_mesh_lpn_poll(void)
{
	if (!bt_mesh.lpn.established) {
		return -EAGAIN;
	}

	BT_DBG("Requesting more messages");

	return send_friend_poll();
}

void bt_mesh_lpn_set_cb(void (*cb)(u16_t friend_addr, bool established))
{
	lpn_cb = cb;
}

int bt_mesh_lpn_init(void)
{
	struct bt_mesh_lpn *lpn = &bt_mesh.lpn;

	BT_DBG("");

	k_delayed_work_init(&lpn->timer, lpn_timeout);

	if (lpn->state == BT_MESH_LPN_ENABLED) {
		if (IS_ENABLED(CONFIG_BT_MESH_LPN_ESTABLISHMENT)) {
			bt_mesh_scan_disable();
		} else {
			bt_mesh_scan_enable();
		}

		send_friend_req(lpn);
	} else {
		bt_mesh_scan_enable();

		if (IS_ENABLED(CONFIG_BT_MESH_LPN_AUTO)) {
			BT_DBG("Waiting %u ms for messages", LPN_AUTO_TIMEOUT);
			lpn_set_state(BT_MESH_LPN_TIMER);
			k_delayed_work_submit(&lpn->timer, LPN_AUTO_TIMEOUT);
		}
	}

	return 0;
}

#endif // CONFIG_BT_MESH_LOW_POWER
