/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/mount.h>

#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <logwrap/logwrap.h>
#include "VoldUtil.h"

#define LOG_TAG "Vold"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include "Ntfs.h"

static char MKNTFS_PATH[] = "/system/bin/mkntfs";

int Ntfs::doMount(const char *fsPath, const char *mountPoint, bool ro, bool remount, bool executable, int ownerUid, int ownerGid, int permMask, bool createLost) {
    int rc;
    unsigned long flags;
    char mountData[255];
    bool isexecutable = executable;

    /*
     * Note: This is a temporary hack. If the sampling profiler is enabled,
     * we make the SD card world-writable so any process can write snapshots.
     *
     * TODO: Remove this code once we have a drop box in system_server.
     */
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.vold.permissivefs", value, "");
    if (value[0] == '1') {
        SLOGW("The SD card is world-writable because the"
            " 'persist.sampling_profiler' system property is set to '1'.");
        permMask = 0;
	isexecutable = true;
    }

    flags = MS_NODEV | MS_NOSUID | MS_DIRSYNC;

    flags |= (isexecutable ? 0 : MS_NOEXEC);
    flags |= (ro ? MS_RDONLY : 0);
    flags |= (remount ? MS_REMOUNT : 0);

    sprintf(mountData, "utf8,uid=%d,gid=%d,fmask=%o,dmask=%o", ownerUid, ownerGid, permMask, permMask);

    rc = mount(fsPath, mountPoint, "tntfs", flags, mountData);

    if (!rc)
    {
	rc = mount(fsPath, mountPoint, "ntfs", flags, mountData);
    }

    return rc;
}

int Ntfs::format(const char *fsPath, bool wipe) {

    const char *args[4];
    int rc = -1;
    int status;

    if (access(MKNTFS_PATH, X_OK)) {
        SLOGE("Unable to format, mkntfs not found.");
        return -1;
    }

    args[0] = MKNTFS_PATH;
    if (wipe) {
        args[1] = fsPath;
        args[2] = NULL;
    } else {
        args[1] = "-f";
        args[2] = fsPath;
        args[3] = NULL;
    }

    rc = android_fork_execvp(ARRAY_SIZE(args), (char **)args, &status, false,
            true);

    if (rc == 0) {
        SLOGI("Filesystem (NTFS) formatted OK");
        return 0;
    } else {
        SLOGE("Format (NTFS) failed (unknown exit code %d)", rc);
        errno = EIO;
        return -1;
    }
    return 0;
}
