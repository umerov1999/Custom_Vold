/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) 2014 The CyanogenMod Project
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

#include "F2FS.h"

#define MKF2FS_MKFS    "/system/bin/mkfs.f2fs"
#define RESIZE2FS_PATH "/system/bin/resize2fs"

int F2FS::doMount(const char *fsPath, const char *mountPoint, bool ro, bool
        remount, bool executable, bool sdcard, const char *mountOpts) {
    int rc;
    unsigned long flags;

    bool isexecutable = executable;

    /*
     * Note: This is a temporary hack. If the sampling profiler is enabled,
     * we make the SD card world-writable so any process can write snapshots.
     *
     * TODO: Remove this code once we have a drop box in system_server.
     */
    char value[PROPERTY_VALUE_MAX];
    property_get("persist.sampling_profiler", value, "");
    if (value[0] == '1') {
        SLOGW("The SD card is world-writable because the"
            " 'persist.sampling_profiler' system property is set to '1'.");
	isexecutable = true;
    }

    flags = MS_NOATIME | MS_NODEV | MS_NOSUID | MS_DIRSYNC;

    flags |= (isexecutable ? 0 : MS_NOEXEC);
    flags |= (ro ? MS_RDONLY : 0);
    flags |= (remount ? MS_REMOUNT : 0);

    rc = mount(fsPath, mountPoint, "f2fs", flags, mountOpts);

    if (sdcard && rc == 0) {
        // Write access workaround
        chown(mountPoint, AID_MEDIA_RW, AID_MEDIA_RW);
        chmod(mountPoint, 0777);
    }

    if (rc && errno == EROFS) {
        SLOGE("%s appears to be a read only filesystem - retrying mount RO", fsPath);
        flags |= MS_RDONLY;
        rc = mount(fsPath, mountPoint, "f2fs", flags, mountOpts);
    }

    return rc;
}

int F2FS::resize(const char *fspath, unsigned int numSectors) {
    const char *args[4];
    char* size_str;
    int rc;
    int status;

    args[0] = RESIZE2FS_PATH;
    args[1] = "-f";
    args[2] = fspath;
    if (asprintf(&size_str, "%ds", numSectors) < 0)
    {
      SLOGE("Filesystem (f2fs) resize failed to set size");
      return -1;
    }
    args[3] = size_str;
    rc = android_fork_execvp(ARRAY_SIZE(args), (char **)args, &status, false,
            true);
    free(size_str);
    if (rc != 0) {
        SLOGE("Filesystem (f2fs) resize failed due to logwrap error");
        errno = EIO;
        return -1;
    }

    if (!WIFEXITED(status)) {
        SLOGE("Filesystem (f2fs) resize did not exit properly");
        errno = EIO;
        return -1;
    }

    status = WEXITSTATUS(status);

    if (status == 0) {
        SLOGI("Filesystem (f2fs) resized OK");
        return 0;
    } else {
        SLOGE("Resize (f2fs) failed (unknown exit code %d)", status);
        errno = EIO;
        return -1;
    }
    return 0;
}

int F2FS::format(const char *fsPath) {

    const char *args[3];
    int rc = -1;
    int status;

    if (access(MKF2FS_MKFS, X_OK)) {
        SLOGE("Unable to format, mkfs.f2fs not found.");
        return -1;
    }

    args[0] = MKF2FS_MKFS;
    args[1] = fsPath;
    args[2] = NULL;

    rc = android_fork_execvp(ARRAY_SIZE(args), (char **)args, &status, false,
            true);

    if (rc == 0) {
        SLOGI("Filesystem (F2FS) formatted OK");
        return 0;
    } else {
        SLOGE("Format (F2FS) failed (unknown exit code %d)", rc);
        errno = EIO;
        return -1;
    }
    return 0;
}
