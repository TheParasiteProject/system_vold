/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include <linux/netlink.h>

#include <android-base/logging.h>

#include "NetlinkHandler.h"
#include "NetlinkManager.h"

NetlinkManager* NetlinkManager::sInstance = NULL;

NetlinkManager* NetlinkManager::Instance() {
    if (!sInstance) sInstance = new NetlinkManager();
    return sInstance;
}

NetlinkManager::NetlinkManager() {
    mBroadcaster = NULL;
}

NetlinkManager::~NetlinkManager() {}

int NetlinkManager::start() {
    struct sockaddr_nl nladdr;
    int sz = 256 * 1024;
    int on = 1;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = getpid();
    nladdr.nl_groups = 0xffffffff;

    if ((mSock = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT)) < 0) {
        PLOG(ERROR) << "Unable to create uevent socket";
        return -1;
    }

    // When running in a net/user namespace, SO_RCVBUFFORCE will fail because
    // it will check for the CAP_NET_ADMIN capability in the root namespace.
    // Try using SO_RCVBUF if that fails.
    if ((setsockopt(mSock, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz)) < 0) &&
        (setsockopt(mSock, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz)) < 0)) {
        PLOG(ERROR) << "Unable to set uevent socket SO_RCVBUF/SO_RCVBUFFORCE option";
        goto out;
    }

    if (setsockopt(mSock, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0) {
        PLOG(ERROR) << "Unable to set uevent socket SO_PASSCRED option";
        goto out;
    }

    if (bind(mSock, (struct sockaddr*)&nladdr, sizeof(nladdr)) < 0) {
        PLOG(ERROR) << "Unable to bind uevent socket";
        goto out;
    }

    mHandler = new NetlinkHandler(mSock);
    if (mHandler->start()) {
        PLOG(ERROR) << "Unable to start NetlinkHandler";
        goto out;
    }

    return 0;

out:
    close(mSock);
    return -1;
}
