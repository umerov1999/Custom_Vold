/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) 2013 The CyanogenMod Project
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
#include <sys/wait.h>

#include <linux/kdev_t.h>
#include <logwrap/logwrap.h>
#include "VoldUtil.h"

#define LOG_TAG "Vold"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include "Exfat.h"

static char EXFAT_MKFS[] = "/system/bin/mkexfat";

int Exfat::doMount(const char *fsPath, const char *mountPoint, bool ro, bool remount, bool executable, int ownerUid, int ownerGid, int permMask, bool ISVfat) {
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
    if(ISVfat == true)
    {
	rc = mount(fsPath, mountPoint, "vfat", flags, mountData);
    }
    else
    {
    	rc = mount(fsPath, mountPoint, "texfat", flags, mountData);
    }

    if (rc !=0 && ISVfat == false) {
        rc = mount(fsPath, mountPoint, "exfat", flags, mountData);
    }

    return rc;
}

int Exfat::format(const char *fsPath) {

    int fd;
    const char *args[3];
    int rc = -1;
    int status;

    if (access(EXFAT_MKFS, X_OK)) {
        SLOGE("Unable to format, mkexfatfs not found.");
        return -1;
    }

    args[0] = EXFAT_MKFS;
    args[1] = fsPath;
    args[2] = NULL;

    rc = android_fork_execvp(ARRAY_SIZE(args), (char **)args, &status, false,
            true);

    if (rc == 0) {
        SLOGI("Filesystem (exFAT) formatted OK");
        return 0;
    } else {
        SLOGE("Format (exFAT) failed (unknown exit code %d)", rc);
        errno = EIO;
        return -1;
    }
    return 0;
}
