/*  os.c
 *  Part of xfce4-memgraph-plugin
 *
 *  Copyright (c) Alexander Nordfelth <alex.nordfelth@telia.com>
 *  Copyright (c) gatopeich <gatoguan-os@yahoo.com>
 *  Copyright (c) 2007-2008 Angelo Arrifano <miknix@gmail.com>
 *  Copyright (c) 2007-2008 Lidiriel <lidiriel@coriolys.org>
 *  Copyright (c) 2010 Florian Rivoal <frivoal@gmail.com>
 *  Copyright (c) 2010 Peter Tribble <peter.tribble@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "os.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_STAT "/proc/stat"
#define PROCMAXLNLEN 256 /* should make it */

// START MEMREAD LIBS
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MEMINFOBUFSIZE (2 * 1024)
static char MemInfoBuf[MEMINFOBUFSIZE];

static unsigned long MTotal = 0;
static unsigned long MFree = 0;
static unsigned long MBuffers = 0;
static unsigned long MCached = 0;
static unsigned long MAvail = 0;
static unsigned long MUsed = 0;
// END MEMREAD LIBS

gint
read_mem_file(guint64 *MT, guint64 *MU)
{
    int fd;
    size_t n;
    char *b_MTotal, *b_MFree, *b_MBuffers, *b_MCached, *b_MAvail, *b_STotal, *b_SFree;

    if ((fd = open("/proc/meminfo", O_RDONLY)) < 0)
    {
        g_warning("Cannot open \'/proc/meminfo\'");
        return -1;
    }
    if ((n = read(fd, MemInfoBuf, MEMINFOBUFSIZE - 1)) == MEMINFOBUFSIZE - 1)
    {
        g_warning("Internal buffer too small to read \'/proc/mem\'");
        close(fd);
        return -1;
    }
    close(fd);

    MemInfoBuf[n] = '\0';

    b_MTotal = strstr(MemInfoBuf, "MemTotal");
    if (!b_MTotal || !sscanf(b_MTotal + strlen("MemTotal"), ": %lu", &MTotal))
        return -1;

    b_MFree = strstr(MemInfoBuf, "MemFree");
    if (!b_MFree || !sscanf(b_MFree + strlen("MemFree"), ": %lu", &MFree))
        return -1;

    b_MBuffers = strstr(MemInfoBuf, "Buffers");
    if (!b_MBuffers || !sscanf(b_MBuffers + strlen("Buffers"), ": %lu", &MBuffers))
        return -1;

    b_MCached = strstr(MemInfoBuf, "Cached");
    if (!b_MCached || !sscanf(b_MCached + strlen("Cached"), ": %lu", &MCached))
        return -1;

    /* In Linux 3.14+, use MemAvailable instead */
    b_MAvail = strstr(MemInfoBuf, "MemAvailable");
    if (b_MAvail && sscanf(b_MAvail + strlen("MemAvailable"), ": %lu", &MAvail))
    {
        MFree = MAvail;
        MBuffers = 0;
        MCached = 0;
    }

    MFree += MCached + MBuffers;
    MUsed = MTotal - MFree;

    *MT = MTotal;
    *MU = MUsed;

    return 0;
}

gboolean
read_mem_data (MemData *data)
{
    guint64 MT, MU;
    read_mem_file(&MT, &MU);

	data[0].load = (gfloat) (MU) / (gfloat) (MT);
    data[0].previous_used  = MU;
    data[0].previous_total = MT;

    return TRUE;
}


Topology*
read_topology (void)
{
    return NULL;
}
