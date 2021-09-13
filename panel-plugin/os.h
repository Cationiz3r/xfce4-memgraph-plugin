/*  os.h
 *  Part of xfce4-memgraph-plugin
 *
 *  Copyright (c) Alexander Nordfelth <alex.nordfelth@telia.com>
 *  Copyright (c) gatopeich <gatoguan-os@yahoo.com>
 *  Copyright (c) 2007-2008 Angelo Arrifano <miknix@gmail.com>
 *  Copyright (c) 2007-2008 Lidiriel <lidiriel@coriolys.org>
 *  Copyright (c) 2010 Florian Rivoal <frivoal@gmail.com>
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
#ifndef _XFCE_OS_H_
#define _XFCE_OS_H_

#include <glib.h>

typedef struct
{
    gfloat load; /* Range: from 0.0 to 1.0 */
    guint64 previous_used;
    guint64 previous_total;
    gboolean smt_highlight;
} MemData;


/* All pointers in this data structure are internal to a single memory allocation
 * and thus the whole data structure can be deallocated using a single call to g_free().
 * Consequenly, pointers exported/read from this data structure are invalid after the deallocation. */
typedef struct
{
    guint num_all_logical_mems;
    guint num_online_logical_mems;
    guint num_all_cores;      /* Range: <1, num_all_logical_mems> */
    guint num_online_cores;   /* Range: <1, num_online_logical_mems> */
    gint *logical_mem_2_core; /* Maps a logical MEM to its core, or to -1 if offline */
    struct MemCore {
        guint num_logical_mems; /* Number of logical MEMs in this core. Might be zero. */
        guint *logical_mems;    /* logical_mems[i] range: <0, num_all_logical_mems> */
    } *cores;
    gboolean smt;       /* Simultaneous multi-threading (hyper-threading) */
    gdouble smt_ratio;  /* Equals to (num_online_logical_mems / num_online_cores), >= 1.0 */
} Topology;

gboolean read_mem_data (MemData *data);
Topology* read_topology (void);

#endif /* !_XFCE_OS_H */
