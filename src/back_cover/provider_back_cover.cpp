/*
 * StateFS TOH back cover provider
 *
 * Copyright (C) 2014 Jolla Ltd.
 * Contact: Mohammed Hassan <mohammed.hassan@jolla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define DEV_DIR "/dev/input/"
#define TOH_NAME "toh-event"

#define LONG_BITS (sizeof(long) * 8)
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)

static inline int
bit_is_set(const unsigned long *array, int bit)
{
    return !!(array[bit / LONG_BITS] & (1LL << (bit % LONG_BITS)));
}

class Provider : public statefs::AProvider
{
public:
    Provider(struct statefs_server *server);
    ~Provider();

    void release();

    static Provider *instance(struct statefs_server *server);

private:
    static Provider *m_provider;
};

Provider *Provider::m_provider = 0;

class NsChild : public statefs::Namespace
{
public:
    NsChild();
    ~NsChild() {}

    void release() {}
};

class BackCoverMonitor {
public:
    BackCoverMonitor(statefs::AProperty *parent);

    int getattr() const;
    ssize_t size() const;
    bool connect(statefs_slot *slot);
    void disconnect();
    int read(std::string *h, char *dst, size_t len, off_t off);
    int write(std::string *h, char const *src, size_t len, off_t off);
    void release();

private:
    int findDevice();
    bool isBackCover(int fd);
    void readValue();

    static void run_thread(BackCoverMonitor& that);

    statefs::AProperty *m_parent;
    statefs_slot *m_slot;
    int m_fd;
    int m_val;
    std::thread m_thread;
    std::mutex m_mutex;
};

Provider::Provider(struct statefs_server *server)
    : AProvider("BackCover", server)
{
    insert(new NsChild);
}

Provider::~Provider()
{

}

void Provider::release()
{
    if (m_provider) {
        delete m_provider;
        m_provider = 0;
    }
}

Provider *Provider::instance(struct statefs_server *server)
{
    if (m_provider) {
        throw std::logic_error("provider ptr is already set");
    }

    m_provider = new Provider(server);
    return m_provider;
}

NsChild::NsChild()
    : Namespace("BackCover")
{
    insert(new statefs::BasicPropertyOwner<BackCoverMonitor, std::string>("attached"));
}

BackCoverMonitor::BackCoverMonitor(statefs::AProperty *parent)
    : m_parent(parent),
      m_slot(0),
      m_fd(-1),
      m_val(0)
{

}

bool BackCoverMonitor::isBackCover(int fd)
{
    char buff[10];
    memset(buff, 0, sizeof(buff));

    if (ioctl(fd, EVIOCGNAME(sizeof(buff)), buff) < 0) {
        return false;
    }

    if (strncmp(TOH_NAME, buff, strlen(TOH_NAME))) {
        return false;
    }

    unsigned long bits[NLONGS(SW_CNT)];
    memset(bits, 0, sizeof(bits));

    if (ioctl(fd, EVIOCGBIT(EV_SW, sizeof(bits)), bits) < 0) {
        return false;
    }

    if (!bit_is_set(bits, EV_SW)) {
        return false;
    }

    return fd;
}

int BackCoverMonitor::findDevice()
{
    DIR *d = opendir(DEV_DIR);
    if (!d) {
        std::cerr << "Failed to open dir " << DEV_DIR << std::endl;
        return -1;
    }

    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        std::stringstream s;

        try {
            s << DEV_DIR << entry->d_name;
        }
        catch (...) {
            closedir(d);
            throw;
        }

        int fd = open(s.str().c_str(), O_RDONLY);
        if (fd == -1) {
            std::cerr << "Failed to check " << entry->d_name << std::endl;
            continue;
        }

        if (isBackCover(fd)) {
            closedir(d);
            return fd;
        }

        close(fd);
    }

    closedir(d);

    std::cerr << "Could not find toh event device" << std::endl;

    return -1;
}

int BackCoverMonitor::getattr() const
{
    return STATEFS_ATTR_READ | STATEFS_ATTR_DISCRETE;
}

ssize_t BackCoverMonitor::size() const
{
    return 1;
}

bool BackCoverMonitor::connect(statefs_slot *slot)
{
    // Let's find our device:
    int fd = findDevice();
    if (fd == -1) {
        return false;
    }

    m_fd = fd;
    m_slot = slot;

    // Get initial value
    readValue();

    // Start monitor
    try {
        m_thread = std::thread(run_thread, std::ref(*this));
    }
    catch (...) {
        close(m_fd);
        throw;
    }

    return true;
}

void BackCoverMonitor::readValue()
{
    unsigned long bits[NLONGS(SW_CNT)];
    memset(bits, 0, sizeof(bits));

    if (ioctl(m_fd, EVIOCGSW(sizeof(bits)), bits) == -1) {
        std::cerr << "Failed to get initial dock value" << std::endl;
        m_val = 0;
        return;
    }

    m_val = bit_is_set(bits, SW_DOCK);
}

void BackCoverMonitor::disconnect()
{
    // Cancel thread.
    pthread_cancel(m_thread.native_handle());

    // Join
    try {
        m_thread.join();
    }
    catch (...) {
        close(m_fd);
        throw;
    }

    // Close fd
    close(m_fd);
    m_fd = -1;

    m_slot = 0;
}

int BackCoverMonitor::read(std::string *, char *dst, size_t len, off_t off)
{
    if (len < 1 || off != 0) {
        return -1;
    }

    std::lock_guard<std::mutex> locker(m_mutex);

    // Hex values for 1 and 0
    dst[0] = m_val ? 0x31 : 0x30;

    return 1;
}

int BackCoverMonitor::write(std::string *, char const *, size_t, off_t)
{
    return -1;
}

void BackCoverMonitor::release()
{
    // Nothing.
}

void BackCoverMonitor::run_thread(BackCoverMonitor& that)
{
    struct input_event buf;

    while (true) {
        if (that.m_fd == -1) {
            return;
        }

        int rc = ::read(that.m_fd, &buf, sizeof(buf));
        if (that.m_fd == -1) {
            return;
        }
        else if (rc == -1) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }

            return;
        }
        else if (rc == 0) {
            return;
        }
        else if (rc % sizeof(buf)) {
            std::cerr << "read returned " << rc << " bytes!" << std::endl;
            continue;
        }
        else {
            if (buf.type == EV_SW && buf.code == SW_DOCK) {
                that.m_mutex.lock();
                if (that.m_val == buf.value) {
                    that.m_mutex.unlock();
                    continue;
                }
                else {
                    that.m_val = buf.value;
                    that.m_mutex.unlock();
                    that.m_slot->on_changed(that.m_slot, that.m_parent);
                }
            }
        }
    }
}

EXTERN_C struct statefs_provider * statefs_provider_get(struct statefs_server *p)
{
    return Provider::instance(p);
}
