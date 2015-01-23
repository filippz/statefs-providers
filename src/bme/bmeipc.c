/*
 * Battery management entity IPC client API
 *
 * Copyright (C) 2012 Jolla Ltd.
 * Contact: Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "bmeipc.h"

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define BME_API (11)

#define BME_SOCK_PATH "/tmp/.bmesrv"
#define BME_COOKIE "BMentity"
#define BME_SYNC 0x434e5953

#define BME_XCHG_FNAME "/tmp/.bmeevt"

#define LOG_ERR(msg, args...)                   \
    {                                           \
        perror("msg");                          \
        fprintf(stderr, msg, ##args);           \
    } while(0)

#define LOG_WARN(msg, args...) LOG_ERR(msg, ##args)

#define LOG_RC(rc, msg, args...)                            \
    ({                                                      \
        perror(msg);                                        \
        fprintf(stderr, "rc=%d, errno=%d: ", rc, errno);    \
        fprintf(stderr, msg, ##args);                       \
        rc;                                                 \
    })

typedef enum {
    bme_event_charge,
    bme_event_charger,
    bme_event_bat,
    bme_event_sys,

    bme_event_ids_end
}  bme_event_id;


#pragma pack(push)
#pragma pack(4)

typedef enum {
    bme_msg_id_stat = 0x8003,
} bme_msg_id;

struct bme_msg_hdr {
    uint32_t sync;
    int32_t size;
};

struct bme_msg
{
    uint16_t id;
    uint16_t option;
};

struct bme_xchg
{
    int32_t events[bme_event_ids_end];
    int32_t watch;
};

#pragma pack(pop)

struct bme_xchg_desc
{
    int h;
    struct bme_xchg stored;
};

static int bme_send(int fd, void const *msg, size_t msg_size)
{
    int rc;
    struct bme_msg_hdr hdr = {
        .sync = BME_SYNC,
        .size = msg_size
    };
    struct iovec iov[] = {
        { .iov_base = &hdr, .iov_len = sizeof(hdr) },
        { .iov_base = (void*)msg, .iov_len = msg_size }
    };
    size_t full_len = msg_size + sizeof(hdr);

    rc = writev(fd, iov, ARRAY_SIZE(iov));
    if (rc < 0)
        return LOG_RC(rc, "sending data\n");

    if (rc < full_len) {
        LOG_ERR("wrote %d instead of %zd\n", rc, full_len);
        return -ENOMEM;
    }
    return rc;
}

static int bme_recv(int fd, void *res, size_t res_size)
{
    struct bme_msg_hdr expected_hdr = {
        .sync = BME_SYNC,
        .size = res_size
    };
    struct bme_msg_hdr hdr;
    struct iovec iov[] = {
        { .iov_base = &hdr, .iov_len = sizeof(hdr) },
        { .iov_base = res, .iov_len = res_size }
    };
    int rc;
    size_t full_len;

    rc = readv(fd, iov, ARRAY_SIZE(iov));
    if (rc < 0)
        return LOG_RC(rc, "reading data\n");

    if (rc < sizeof(hdr)) {
        LOG_ERR("Got a mess, size (%d) < hdr size (%zd)\n",
                rc, sizeof(hdr));
        return -EPROTO;
    }
    full_len = res_size + sizeof(hdr);
    if (rc < full_len) {
        LOG_ERR("Expected %zu bytes but got %d\n", full_len, rc);
        return -EPROTO;
    }
    if (hdr.sync != expected_hdr.sync) {
        LOG_ERR("Sync %x is wrong, need %x\n",
                hdr.sync, expected_hdr.sync);
        return -EPROTO;
    }
    if (hdr.size != expected_hdr.size) {
        LOG_ERR("Size field %x is wrong, need %x\n",
                hdr.size, expected_hdr.size);
        return -EPROTO;
    }
    return rc;
}

static int bme_send_recv
(int fd, void const *msg, size_t msg_size, void *res, size_t res_size)
{
    int rc;
    rc = bme_send(fd, msg, msg_size);
    if (rc < 0)
        return rc;

    rc = bme_recv(fd, res, res_size);
    return rc;
}

#define BME_MSG_SIZE_MAX (0x80)

static void bme_clean_reply(int fd, size_t size)
{
    int rc;
    char buf[BME_MSG_SIZE_MAX];

    if (size > sizeof(buf)) {
        LOG_ERR("Too long ipc reply %zu\n", size);
        return;
    }
    rc = bme_recv(fd, &buf, size);
    if (rc != size) {
        LOG_ERR("Issue %d cleaning reply\n", rc);
    }
}

/* bme ipc api was changed to supply size of data sent back in the
 * status field. Earlier it returned only 0 on success or negative
 * value on failure. Current nemo mobile uses old bme binary. After
 * update to a new one BME_API_12 should be set
 */
static inline int bme_rc_recv(int fd, size_t res_size)
{
    int rc;
    int32_t ipc_rc;
    rc = bme_recv(fd, &ipc_rc, sizeof(ipc_rc));
    if (rc < 0) {
        LOG_ERR("getting ipc rc\n");
        return rc;
    }

    if (ipc_rc < 0) {
        LOG_ERR("ipc status %d\n", ipc_rc);
        errno = ipc_rc;
        return -1;
    }

    if (ipc_rc == 0) {
        ipc_rc = (int32_t)res_size;
    } else if (ipc_rc != res_size) {
        LOG_ERR("Provided reply len %d != expected %zu\n",
                ipc_rc, res_size);
        bme_clean_reply(fd, ipc_rc);
        errno = -EPROTO;
        return -1;
    }
    return ipc_rc;
}

static int bme_query
(int fd, void const *msg, size_t msg_size, void *res, size_t res_size)
{
    int rc;

    rc = bme_send(fd, msg, msg_size);
    if (rc < 0)
        return rc;

    rc = bme_rc_recv(fd, res ? res_size : 0);
    if (rc <= 0)
        return rc;

    rc = bme_recv(fd, res, res_size);
    return rc;
}

static int bme_cookie_set(int fd)
{
    char ack;
    return bme_send_recv(fd, BME_COOKIE, sizeof(BME_COOKIE) - 1,
                            &ack, sizeof(ack));
}

int bme_open()
{
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    int fd, rc;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return LOG_RC(fd, "opening socket");

    COMPILE_TIME_ASSERT(sizeof(addr.sun_path) >= sizeof(BME_SOCK_PATH),
                        too_long_sock_path);
    memcpy(addr.sun_path, BME_SOCK_PATH, sizeof(BME_SOCK_PATH));

    rc = connect(fd, (struct sockaddr*)&addr,
                 sizeof(addr.sun_family) + sizeof(BME_SOCK_PATH) - 1);
    if (rc < 0) {
        LOG_ERR("error connecting\n");
        goto err;
    }

    rc = bme_cookie_set(fd);
    if (rc < 0) {
        LOG_ERR("Setting cookie\n");
        goto err;
    }

    return fd;
err:
    bme_close(fd);
    return rc;
}

int bme_close(int fd)
{
    if (fd < 0)
        return fd;

    close(fd);
    return 0;
}

int bme_stat_get(int fd, bme_stat_t *stat)
{
    static struct bme_msg msg = {
        .id = bme_msg_id_stat,
        .option = 0
    };
    return bme_query(fd, &msg, sizeof(msg), stat, sizeof(stat[0]));
}

int bme_inotify_watch_add(bme_xchg_t h)
{
    int fd, rc;
    fd = open(BME_XCHG_FNAME, O_RDONLY | O_CREAT, 0666);
    if (fd < 0)
        return -1;
    rc = inotify_add_watch(h->h, BME_XCHG_FNAME,
                           IN_CLOSE_WRITE | IN_DELETE_SELF);
    if (rc < 0)
        goto out;
    h->stored.watch = rc;
out:
    close(fd);
    return rc;
}

int bme_inotify_watch_rm(bme_xchg_t h)
{
    int rc;
    if (h->stored.watch < 0) {
        errno = EINVAL;
        LOG_ERR("Unpaired unwatch");
        return -1;
    }
    rc = inotify_rm_watch(h->h, h->stored.watch);
    h->stored.watch = -1;
    return rc;
}

static inline int bme_inotify_read(bme_xchg_t h)
{
    int rc;
    struct inotify_event ev;

    memset(&ev, 0, sizeof(ev));

    rc = read(h->h, &ev, sizeof(ev));
    if (rc < 0) {
        LOG_ERR("Reading event\n");
        return rc;
    }

    if (rc != sizeof(ev)) {
        LOG_WARN("bad size %d vs %zu\n", rc, sizeof(ev));
        errno = EPROTO;
        return -1;
    }

    if (ev.mask & IN_CLOSE_WRITE) {
        LOG_ERR("Event source is closed\n");
        goto err_again;
    }

    if (ev.mask & IN_DELETE_SELF) {
        LOG_WARN("inotify: delete self, recreating\n");
        rc = bme_inotify_watch_add(h);
        if (rc < 0)
            return rc;

        goto err_again;
    }

    return 0;
err_again:
    errno = EAGAIN;
    return -1;
}

static void bme_flags_update(int32_t *stored, int32_t const *actual,
                                unsigned *flags, unsigned mask)
{
    if (*stored != *actual) {
        *flags &= ~BME_EV_TIMEOUT;
        *flags |= mask;
        *stored = *actual;
    }
}

static inline void bme_state_update
(int32_t *stored, int32_t const *actual, unsigned *flags)
{
    int i;
    static const unsigned masks[] = {
        BME_EV_CHARGE, /* bme_event_charge, */
        BME_EV_CHARGER, /* bme_event_charger, */
        BME_EV_BAT, /* bme_event_bat, */
        BME_EV_SYS /* bme_event_sys, */
    };
    COMPILE_TIME_ASSERT(ARRAY_SIZE(masks) == bme_event_ids_end,
                        wrong_array_size);
    for (i = 0; i < ARRAY_SIZE(masks); ++i) {
        bme_flags_update(&stored[i], &actual[i], flags, masks[i]);
    }
}

static inline int bme_state_get
(unsigned *state_mask, struct bme_xchg *xchg)
{
    int fd, rc;
    int32_t events[bme_event_ids_end];

    *state_mask = 0;

    fd = open(BME_XCHG_FNAME, O_RDONLY | O_CREAT, 0666);
    if (fd < 0)
        return LOG_RC(fd, "Opening xchg file\n");

    rc = read(fd, &events, sizeof(events));
    if (rc < 0) {
        LOG_ERR("Reading events\n");
        goto out;
    }
    if (rc != sizeof(events)) {
        LOG_ERR("Expected events size is %zu, got %d\n", sizeof(events), rc);
        rc = -1;
        goto out;
    }

    bme_state_update(xchg->events, events, state_mask);
out:
    close(fd);
    return rc;
}


bme_xchg_t bme_xchg_open()
{
    int h, rc;
    struct bme_xchg *xchg = NULL;
    bme_xchg_t desc;

    h = inotify_init();
    if (h < 0)
        return BME_XCHG_INVAL;

    desc = malloc(sizeof(desc[0]));
    if (!desc)
        goto err;

    memset(desc, 0, sizeof(desc[0]));
    desc->h = h;

    rc = bme_inotify_watch_add(desc);
    if (rc < 0) {
        LOG_ERR("Can't watch\n");
        goto err;
    }

    return desc;
err:
    bme_xchg_close(desc);
    return BME_XCHG_INVAL;
}

void bme_xchg_close(bme_xchg_t h)
{
    struct bme_xchg_desc *desc = (struct bme_xchg_desc *)h;
    if (!desc)
        return;

    if (desc->h >= 0) {
        close(desc->h);
        desc->h = 0;
    }
    free(desc);
}

int bme_xchg_inotify_desc(bme_xchg_t h)
{
    return h->h;
}

int bme_xchg_inotify_read(bme_xchg_t h, struct inotify_event *ev)
{
    return read(h->h, ev, sizeof(ev[0]));
}

int bme_xchg_read(bme_xchg_t h)
{
    int rc = -1;
    struct bme_xchg *xchg = &h->stored;
    unsigned state_mask = 0;

    rc = bme_inotify_read(h);
    if (rc < 0)
        LOG_WARN("inotify issues, reading state anyway\n");

    rc = bme_state_get(&state_mask, xchg);
    if (rc < 0) {
        if (errno == EAGAIN)
            state_mask = BME_EV_TIMEOUT;
        else
            state_mask = BME_EV_ERR;
    }
out:
    return state_mask;
}
