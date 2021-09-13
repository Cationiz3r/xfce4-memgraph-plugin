/*  mem.c
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
#include "mem.h"
#include "settings.h"
#include "mode.h"
#include "properties.h"

#include <libxfce4ui/libxfce4ui.h>
#include <math.h>
#ifndef _
# include <libintl.h>
# define _(String) gettext (String)
#endif

static void       memgraph_construct   (XfcePanelPlugin    *plugin);
static MEMGraph  *create_gui           (XfcePanelPlugin    *plugin);
static void       create_bars          (MEMGraph           *base,
                                        GtkOrientation      orientation);
static guint      init_mem_data        (MemData           **data);
static void       shutdown             (XfcePanelPlugin    *plugin,
                                        MEMGraph           *base);
static void       delete_bars          (MEMGraph           *base);
static gboolean   size_cb              (XfcePanelPlugin    *plugin,
                                        guint               size,
                                        MEMGraph           *base);
static void       about_cb             (XfcePanelPlugin    *plugin,
                                        MEMGraph           *base);
static void       set_bars_size        (MEMGraph           *base);
static void       mode_cb              (XfcePanelPlugin    *plugin,
                                        XfcePanelPluginMode mode,
                                        MEMGraph           *base);
static void       update_tooltip       (MEMGraph           *base);
static gboolean   tooltip_cb           (GtkWidget          *widget,
                                        gint                x,
                                        gint                y,
                                        gboolean            keyboard,
                                        GtkTooltip         *tooltip,
                                        MEMGraph           *base);
static void       draw_area_cb         (GtkWidget          *w,
                                        cairo_t            *cr,
                                        gpointer            data);
static void       draw_bars_cb         (GtkWidget          *w,
                                        cairo_t            *cr,
                                        gpointer            data);
static gboolean   command_cb           (GtkWidget          *w,
                                        GdkEventButton     *event,
                                        MEMGraph           *base);

XFCE_PANEL_PLUGIN_REGISTER (memgraph_construct);

static void
memgraph_construct (XfcePanelPlugin *plugin)
{
    MEMGraph *base;

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    base = create_gui (plugin);
    read_settings (plugin, base);
    xfce_panel_plugin_menu_show_configure (plugin);

    xfce_panel_plugin_menu_show_about (plugin);

    g_signal_connect (plugin, "about", G_CALLBACK (about_cb), base);
    g_signal_connect (plugin, "free-data", G_CALLBACK (shutdown), base);
    g_signal_connect (plugin, "save", G_CALLBACK (write_settings), base);
    g_signal_connect (plugin, "configure-plugin", G_CALLBACK (create_options), base);
    g_signal_connect (plugin, "size-changed", G_CALLBACK (size_cb), base);
    g_signal_connect (plugin, "mode-changed", G_CALLBACK (mode_cb), base);
}

static MEMGraph *
create_gui (XfcePanelPlugin *plugin)
{
    GtkWidget *frame, *ebox;
    GtkOrientation orientation;
    MEMGraph *base = g_new0 (MEMGraph, 1);

    orientation = xfce_panel_plugin_get_orientation (plugin);
    if ((base->nr_cores = init_mem_data (&base->mem_data)) == 0)
        fprintf (stderr,"Cannot init mem data !\n");

    /* Read MEM data twice in order to initialize
     * mem_data[].previous_used and mem_data[].previous_total
     * with the current HWMs. HWM = High Water Mark. */
    read_mem_data (base->mem_data);
    read_mem_data (base->mem_data);

    base->topology = read_topology ();

    base->plugin = plugin;

    base->ebox = ebox = gtk_event_box_new ();
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
    gtk_event_box_set_above_child (GTK_EVENT_BOX (ebox), TRUE);
    gtk_container_add (GTK_CONTAINER (plugin), ebox);
    xfce_panel_plugin_add_action_widget (plugin, ebox);
    g_signal_connect (ebox, "button-press-event", G_CALLBACK (command_cb), base);

    base->box = gtk_box_new (orientation, 0);
    gtk_container_add (GTK_CONTAINER (ebox), base->box);
    gtk_widget_set_has_tooltip (base->box, TRUE);
    g_signal_connect (base->box, "query-tooltip", G_CALLBACK (tooltip_cb), base);

    base->frame_widget = frame = gtk_frame_new (NULL);
    gtk_box_pack_end (GTK_BOX (base->box), frame, TRUE, TRUE, 2);

    base->draw_area = gtk_drawing_area_new ();
    gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (base->draw_area));
    g_signal_connect_after (base->draw_area, "draw", G_CALLBACK (draw_area_cb), base);

    base->has_bars = FALSE;
    base->has_barcolor = FALSE;
    base->bars.orientation = orientation;
    base->highlight_smt = HIGHLIGHT_SMT_BY_DEFAULT;
    base->per_core_spacing = PER_CORE_SPACING_DEFAULT;

    mode_cb (plugin, (XfcePanelPluginMode) orientation, base);
    gtk_widget_show_all (ebox);

    base->tooltip_text = gtk_label_new (NULL);
    g_object_ref (base->tooltip_text);

    return base;
}

static void
about_cb (XfcePanelPlugin *plugin, MEMGraph *base)
{
    GdkPixbuf *icon;
    const gchar *auth[] = {
        "Alexander Nordfelth <alex.nordfelth@telia.com>", "gatopeich <gatoguan-os@yahoo.com>",
        "lidiriel <lidiriel@coriolys.org>","Angelo Miguel Arrifano <miknix@gmail.com>",
        "Florian Rivoal <frivoal@gmail.com>","Peter Tribble <peter.tribble@gmail.com>", NULL};
    icon = xfce_panel_pixbuf_from_source ("xfce4-memgraph-plugin", NULL, 48);
    gtk_show_about_dialog (NULL,
        "logo", icon,
        "license", xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
        "version", PACKAGE_VERSION,
        "program-name", PACKAGE_NAME,
        "comments", _("Graphical representation of the MEM load"),
        "website", "https://docs.xfce.org/panel-plugins/xfce4-memgraph-plugin",
        "copyright", _("Copyright (c) 2003-2021\n"),
        "authors", auth, NULL);

    if (icon)
        g_object_unref (G_OBJECT (icon));
}

static void
ebox_revalidate (MEMGraph *base)
{
    gtk_event_box_set_above_child (GTK_EVENT_BOX (base->ebox), FALSE);
    gtk_event_box_set_above_child (GTK_EVENT_BOX (base->ebox), TRUE);
}

static guint
nb_bars (MEMGraph *base)
{
    return base->tracked_core == 0 ? base->nr_cores : 1;
}

static void
create_bars (MEMGraph *base, GtkOrientation orientation)
{
    base->bars.frame = gtk_frame_new (NULL);
    base->bars.draw_area = gtk_drawing_area_new ();
    base->bars.orientation = orientation;
    set_frame (base, base->has_frame);
    gtk_container_add (GTK_CONTAINER (base->bars.frame), base->bars.draw_area);
    gtk_box_pack_end (GTK_BOX (base->box), base->bars.frame, TRUE, TRUE, 0);
    g_signal_connect_after (base->bars.draw_area, "draw", G_CALLBACK (draw_bars_cb), base);
    gtk_widget_show_all (base->bars.frame);
    ebox_revalidate (base);
}

guint
init_mem_data (MemData **data)
{
    *data = (MemData *) g_malloc0 (sizeof (MemData));
    return 1;
}

static void
shutdown (XfcePanelPlugin *plugin, MEMGraph *base)
{
    g_free (base->mem_data);
    g_free (base->topology);
    delete_bars (base);
    gtk_widget_destroy (base->ebox);
    gtk_widget_destroy (base->tooltip_text);
    if (base->timeout_id)
        g_source_remove (base->timeout_id);
    if (base->history.data)
    {
        guint core;
        for (core = 0; core < base->nr_cores+1; core++)
            g_free (base->history.data[core]);
        g_free (base->history.data);
    }
    g_free (base->command);
    g_free (base);
}

static void
queue_draw (MEMGraph *base)
{
    if (base->mode != MODE_DISABLED)
        gtk_widget_queue_draw (base->draw_area);
    if (base->bars.draw_area)
        gtk_widget_queue_draw (base->bars.draw_area);
}

static void
delete_bars (MEMGraph *base)
{
    if (base->bars.frame)
    {
        gtk_widget_destroy (base->bars.frame);
        base->bars.frame = NULL;
        base->bars.draw_area = NULL;
    }
}

static void
resize_history (MEMGraph *base, gssize history_size)
{
    const guint fastest = get_update_interval_ms (RATE_FASTEST);
    const guint slowest = get_update_interval_ms (RATE_SLOWEST);
    gssize cap_pow2, old_cap_pow2, old_mask, old_offset;
    MemLoad **old_data;

    old_cap_pow2 = base->history.cap_pow2;
    old_data = base->history.data;
    old_mask = base->history.mask;
    old_offset = base->history.offset;

    cap_pow2 = 1;
    while (cap_pow2 < MAX_SIZE * slowest / fastest)
        cap_pow2 <<= 1;
    while (cap_pow2 < history_size * slowest / fastest)
        cap_pow2 <<= 1;

    if (cap_pow2 != old_cap_pow2)
    {
        guint core;
        gssize i;
        base->history.cap_pow2 = cap_pow2;
        base->history.data = (MemLoad**) g_malloc0 ((base->nr_cores + 1) * sizeof (MemLoad*));
        for (core = 0; core < base->nr_cores + 1; core++)
            base->history.data[core] = (MemLoad*) g_malloc0 (cap_pow2 * sizeof (MemLoad));
        base->history.mask = cap_pow2 - 1;
        base->history.offset = 0;
        if (old_data != NULL)
        {
            for (core = 0; core < base->nr_cores+1; core++)
            {
                for (i = 0; i < old_cap_pow2 && i < cap_pow2; i++)
                    base->history.data[core][i] = old_data[core][(old_offset + i) & old_mask];
                g_free (old_data[core]);
            }
            g_free (old_data);
        }
    }

    base->history.size = history_size;
}

static gboolean
size_cb (XfcePanelPlugin *plugin, guint plugin_size, MEMGraph *base)
{
    gint frame_h, frame_v, size;
    gssize history;
    GtkOrientation orientation;
    guint border_width;
    const gint shadow_width = base->has_frame ? 2*1 : 0;

    size = base->size;
    if (base->per_core && base->nr_cores >= 2)
    {
        size *= base->nr_cores;
        size += (base->nr_cores - 1) * base->per_core_spacing;
    }

    orientation = xfce_panel_plugin_get_orientation (plugin);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        frame_h = size + shadow_width;
        frame_v = plugin_size;
        history = base->size;
    }
    else
    {
        frame_h = plugin_size;
        frame_v = size + shadow_width;
        history = plugin_size;
    }

    /* Expand history size for the non-linear time-scale mode.
     *   128 * pow(1.04, 128) = 19385.5175366781
     *   163 * pow(1.04, 163) = 97414.11965601446
     */
    history = ceil (history * pow(NONLINEAR_MODE_BASE, history));
    if (G_UNLIKELY (history < 0 || history > MAX_HISTORY_SIZE))
        history = MAX_HISTORY_SIZE;

    if (history > base->history.cap_pow2)
        resize_history (base, history);
    else
        base->history.size = history;

    gtk_widget_set_size_request (GTK_WIDGET (base->frame_widget), frame_h, frame_v);
    if (base->has_bars) {
        base->bars.orientation = orientation;
        set_bars_size (base);
    }

    if (base->has_border)
        border_width = (xfce_panel_plugin_get_size (base->plugin) > 26 ? 2 : 1);
    else
        border_width = 0;
    gtk_container_set_border_width (GTK_CONTAINER (base->box), border_width);

    set_border (base, base->has_border);

    return TRUE;
}

static void
set_bars_size (MEMGraph *base)
{
    gint h, v;
    gint shadow_width;

    shadow_width = base->has_frame ? 2*1 : 0;

    if (base->bars.orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        h = 6 * nb_bars (base) - 2 + shadow_width;
        v = -1;
    }
    else
    {
        h = -1;
        v = 6 * nb_bars (base) - 2 + shadow_width;
    }

    gtk_widget_set_size_request (base->bars.frame, h, v);
}

static void
mode_cb (XfcePanelPlugin *plugin, XfcePanelPluginMode mode, MEMGraph *base)
{
    gtk_orientable_set_orientation (GTK_ORIENTABLE (base->box),
                                    xfce_panel_plugin_get_orientation (plugin));

    size_cb (plugin, xfce_panel_plugin_get_size (base->plugin), base);
}

static void
detect_smt_issues (MEMGraph *base)
{
	return;
}

static gboolean
update_cb (gpointer user_data)
{
    MEMGraph *base = user_data;

    if (!read_mem_data (base->mem_data))
        return TRUE;

    detect_smt_issues (base);

    if (base->history.data != NULL)
    {
        const gint64 timestamp = g_get_real_time ();
        guint core;

        /* Prepend the current MEM load to the history */
        base->history.offset = (base->history.offset - 1) & base->history.mask;
        for (core = 0; core < base->nr_cores+1; core++)
        {
            MemLoad load;
            load.timestamp = timestamp;
            load.value = base->mem_data[core].load;
            base->history.data[core][base->history.offset] = load;
        }
    }

    queue_draw (base);
    update_tooltip (base);

    return TRUE;
}

static void
update_tooltip (MEMGraph *base)
{
    gchar tooltip[32];
    g_snprintf (tooltip, 32, _("%u%% ( %uMb / %uMb )"), (guint) roundf (base->mem_data[0].load * 100),
		(guint) roundf (base->mem_data[0].previous_used / 1024),
		(guint) roundf (base->mem_data[0].previous_total / 1024));
    if (strcmp (gtk_label_get_text (GTK_LABEL (base->tooltip_text)), tooltip))
        gtk_label_set_text (GTK_LABEL (base->tooltip_text), tooltip);
}

static gboolean
tooltip_cb (GtkWidget *widget, gint x, gint y, gboolean keyboard, GtkTooltip *tooltip, MEMGraph *base)
{
    gtk_tooltip_set_custom (tooltip, base->tooltip_text);
    return TRUE;
}

static void
draw_area_cb (GtkWidget *widget, cairo_t *cr, gpointer data)
{
    MEMGraph *base = (MEMGraph *) data;
    GtkAllocation alloc;
    gint w, h;
    void (*draw) (MEMGraph *base, cairo_t *cr, gint w, gint h, guint core) = NULL;

    gtk_widget_get_allocation (base->draw_area, &alloc);
    w = alloc.width;
    h = alloc.height;

    switch (base->mode)
    {
        case MODE_DISABLED:
            break;
        case MODE_NORMAL:
            draw = draw_graph_normal;
            break;
        case MODE_LED:
            draw = draw_graph_LED;
            break;
        case MODE_NO_HISTORY:
            draw = draw_graph_no_history;
            break;
        case MODE_GRID:
            draw = draw_graph_grid;
            break;
    }

    if (draw)
    {
        if (!base->per_core || base->nr_cores == 1)
        {
            guint core;

            if (base->colors[BG_COLOR].alpha != 0)
            {
                gdk_cairo_set_source_rgba (cr, &base->colors[BG_COLOR]);
                cairo_rectangle (cr, 0, 0, w, h);
                cairo_fill (cr);
            }

            core = base->tracked_core;
            if (G_UNLIKELY (core > base->nr_cores+1))
                core = 0;
            draw (base, cr, w, h, core);
        }
        else
        {
            guint core;
            gboolean horizontal;
            gint w1, h1;

            horizontal = (xfce_panel_plugin_get_orientation (base->plugin) == GTK_ORIENTATION_HORIZONTAL);
            if (horizontal)
            {
                w1 = base->size;
                h1 = h;
            }
            else
            {
                w1 = w;
                h1 = base->size;
            }

            for (core = 0; core < base->nr_cores; core++)
            {
                cairo_save (cr);
                {
                    cairo_rectangle_t translation = {};
                    *(horizontal ? &translation.x : &translation.y) = core * (base->size + base->per_core_spacing);
                    cairo_translate (cr, translation.x, translation.y);

                    if (base->colors[BG_COLOR].alpha != 0)
                    {
                        gdk_cairo_set_source_rgba (cr, &base->colors[BG_COLOR]);
                        cairo_rectangle (cr, 0, 0, w1, h1);
                        cairo_fill (cr);
                    }

                    cairo_rectangle (cr, 0, 0, w1, h1);
                    cairo_clip (cr);
                    draw (base, cr, w1, h1, core+1);
                }
                cairo_restore (cr);
            }
        }
    }
}

static void
draw_bars_cb (GtkWidget *widget, cairo_t *cr, gpointer data)
{
    MEMGraph *const base = (MEMGraph *) data;
    GtkAllocation alloc;
    gfloat size;
    const gboolean horizontal = (base->bars.orientation == GTK_ORIENTATION_HORIZONTAL);

    gtk_widget_get_allocation (base->bars.draw_area, &alloc);

    if (base->colors[BG_COLOR].alpha != 0)
    {
        gdk_cairo_set_source_rgba (cr, &base->colors[BG_COLOR]);
        cairo_rectangle (cr, 0, 0, alloc.width, alloc.height);
        cairo_fill (cr);
    }

    size = (horizontal ? alloc.height : alloc.width);
    if (base->tracked_core != 0 || base->nr_cores == 1)
    {
        gfloat usage = base->mem_data[0].load;
        if (usage < base->load_threshold)
            usage = 0;
        usage *= size;

        gdk_cairo_set_source_rgba (cr, &base->colors[BARS_COLOR]);
        if (horizontal)
            cairo_rectangle (cr, 0, size-usage, 4, usage);
        else
            cairo_rectangle (cr, 0, 0, usage, 4);
        cairo_fill (cr);
    }
    else
    {
        const GdkRGBA *active_color = NULL;
        gboolean fill = FALSE;
        guint i;
        for (i = 0; i < base->nr_cores; i++)
        {
            const GdkRGBA *color;
            const gboolean highlight = base->highlight_smt && base->mem_data[i+1].smt_highlight;
            gfloat usage;

            usage = base->mem_data[i+1].load;
            if (usage < base->load_threshold)
                usage = 0;
            usage *= size;

            /* Suboptimally placed threads on SMT MEMs are optionally painted using a different color. */
            color = &base->colors[highlight ? SMT_ISSUES_COLOR : BARS_COLOR];
            if (active_color != color)
            {
                if (fill)
                {
                    cairo_fill (cr);
                    fill = FALSE;
                }
                gdk_cairo_set_source_rgba (cr, color);
                active_color = color;
            }

            if (horizontal)
                cairo_rectangle (cr, 6*i, size-usage, 4, usage);
            else
                cairo_rectangle (cr, 0, 6*i, usage, 4);
            fill = TRUE;
        }
        if (fill)
            cairo_fill (cr);
    }
}

static const gchar*
default_command (gboolean *in_terminal, gboolean *startup_notification)
{
    gchar *s = g_find_program_in_path ("xfce4-taskmanager");
    if (s)
    {
        g_free (s);
        *in_terminal = FALSE;
        *startup_notification = TRUE;
        return "xfce4-taskmanager";
    }
    else
    {
        *in_terminal = TRUE;
        *startup_notification = FALSE;

        s = g_find_program_in_path ("htop");
        if (s)
        {
            g_free (s);
            return "htop";
        }
        else
        {
            return "top";
        }
    }
}

static gboolean
command_cb (GtkWidget *w, GdkEventButton *event, MEMGraph *base)
{
    if (event->button == 1)
    {
        const gchar *command;
        gboolean in_terminal, startup_notification;

        if (base->command)
        {
            command = base->command;
            in_terminal = base->command_in_terminal;
            startup_notification = base->command_startup_notification;
        }
        else
        {
            command = default_command (&in_terminal, &startup_notification);
        }

        xfce_spawn_command_line_on_screen (gdk_screen_get_default (),
                                           command, in_terminal,
                                           startup_notification, NULL);
    }
    return FALSE;
}

/**
 * get_update_interval_ms:
 *
 * Returns: update interval in milliseconds.
 */
guint
get_update_interval_ms (MEMGraphUpdateRate rate)
{
    switch (rate)
    {
        case RATE_FASTEST:
            return 250;
        case RATE_FAST:
            return 500;
        case RATE_NORMAL:
            return 750;
        case RATE_SLOW:
            return 1000;
        case RATE_SLOWEST:
            return 3000;
        default:
            return 750;
    }
}

void
set_startup_notification (MEMGraph *base, gboolean startup_notification)
{
    base->command_startup_notification = startup_notification;
}

void
set_in_terminal (MEMGraph *base, gboolean in_terminal)
{
    base->command_in_terminal = in_terminal;
}

void
set_command (MEMGraph *base, const gchar *command)
{
    g_free (base->command);
    base->command = g_strdup (command);
    g_strstrip (base->command);
    if (strlen (base->command) == 0)
    {
        g_free (base->command);
        base->command = NULL;
    }
}

void
set_bars (MEMGraph *base, gboolean bars)
{
    if (base->has_bars != bars)
    {
        base->has_bars = bars;
        if (bars)
        {
            create_bars (base, xfce_panel_plugin_get_orientation (base->plugin));
            set_bars_size (base);
        }
        else
            delete_bars (base);
    }
}

void
set_border (MEMGraph *base, gboolean border)
{
    if (base->has_border != border)
    {
        base->has_border = border;
        size_cb (base->plugin, xfce_panel_plugin_get_size (base->plugin), base);
    }
}

void
set_frame (MEMGraph *base, gboolean frame)
{
    base->has_frame = frame;
    gtk_frame_set_shadow_type (GTK_FRAME (base->frame_widget), base->has_frame ? GTK_SHADOW_IN : GTK_SHADOW_NONE);
    if (base->bars.frame)
        gtk_frame_set_shadow_type (GTK_FRAME (base->bars.frame), base->has_frame ? GTK_SHADOW_IN : GTK_SHADOW_NONE);
    size_cb (base->plugin, xfce_panel_plugin_get_size (base->plugin), base);
}

void
set_nonlinear_time (MEMGraph *base, gboolean nonlinear)
{
    if (base->non_linear != nonlinear)
    {
        base->non_linear = nonlinear;
        queue_draw (base);
    }
}

void
set_per_core (MEMGraph *base, gboolean per_core)
{
    if (base->per_core != per_core)
    {
        base->per_core = per_core;
        size_cb (base->plugin, xfce_panel_plugin_get_size (base->plugin), base);
    }
}

void
set_per_core_spacing (MEMGraph *base, guint spacing)
{
    if (G_UNLIKELY (spacing < PER_CORE_SPACING_MIN))
        spacing = PER_CORE_SPACING_MIN;
    if (G_UNLIKELY (spacing > PER_CORE_SPACING_MAX))
        spacing = PER_CORE_SPACING_MAX;

    if (base->per_core_spacing != spacing)
    {
        base->per_core_spacing = spacing;
        size_cb (base->plugin, xfce_panel_plugin_get_size (base->plugin), base);
    }
}

void
set_smt (MEMGraph *base, gboolean highlight_smt)
{
    base->highlight_smt = highlight_smt;
}

void
set_update_rate (MEMGraph *base, MEMGraphUpdateRate rate)
{
    gboolean change = (base->update_interval != rate);
    gboolean init = (base->timeout_id == 0);

    if (change || init)
    {
        guint interval = get_update_interval_ms (rate);

        base->update_interval = rate;
        if (base->timeout_id)
            g_source_remove (base->timeout_id);
        base->timeout_id = g_timeout_add (interval, update_cb, base);

        if (change && !init)
            queue_draw (base);
    }
}

void
set_size (MEMGraph *base, guint size)
{
    if (G_UNLIKELY (size < MIN_SIZE))
        size = MIN_SIZE;
    if (G_UNLIKELY (size > MAX_SIZE))
        size = MAX_SIZE;

    base->size = size;
    size_cb (base->plugin, xfce_panel_plugin_get_size (base->plugin), base);
}

void
set_color_mode (MEMGraph *base, guint color_mode)
{
    if (base->color_mode != color_mode)
    {
        base->color_mode = color_mode;
        queue_draw (base);
    }
}

void
set_mode (MEMGraph *base, MEMGraphMode mode)
{
    base->mode = mode;

    if (mode == MODE_DISABLED)
    {
        gtk_widget_hide (base->frame_widget);
    }
    else
    {
        gtk_widget_show (base->frame_widget);
        ebox_revalidate (base);
    }
}

void
set_color (MEMGraph *base, guint number, GdkRGBA color)
{
    if (!gdk_rgba_equal (&base->colors[number], &color))
    {
        base->colors[number] = color;
        queue_draw (base);
    }
}

void
set_tracked_core (MEMGraph *base, guint core)
{
    if (G_UNLIKELY (core > base->nr_cores+1))
        core = 0;

    if (base->tracked_core != core)
    {
        gboolean has_bars = base->has_bars;
        if (has_bars)
            set_bars (base, FALSE);
        base->tracked_core = core;
        if (has_bars)
            set_bars (base, TRUE);
    }
}

void
set_load_threshold (MEMGraph *base, gfloat threshold)
{
    if (threshold < 0)
        threshold = 0;
    if (threshold > MAX_LOAD_THRESHOLD)
        threshold = MAX_LOAD_THRESHOLD;
    base->load_threshold = threshold;
}
