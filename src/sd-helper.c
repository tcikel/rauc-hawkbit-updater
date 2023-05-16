/**
 * SPDX-License-Identifier: LGPL-2.1-only
 * SPDX-FileCopyrightText: 2018-2020 Lasse K. Mikkelsen <lkmi@prevas.dk>, Prevas A/S (www.prevas.com)
 *
 * @file
 * @brief Systemd helper
 */

#include "sd-helper.h"

/**
 * @brief Callback function: prepare GSource
 *
 * @param[in] source sd_event_source that should be prepared.
 * @param[in] timeout not used
 * @return gboolean, TRUE on success, FALSE otherwise
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GSource
 */
static gboolean sd_source_prepare(GSource *source, gint *timeout)
{
        g_return_val_if_fail(source, FALSE);

        return sd_event_prepare(((struct SDSource *) source)->event) > 0 ? TRUE : FALSE;
}

/**
 * @brief Callback function: check GSource
 *
 * @param[in] source sd_event_source that should be checked
 * @return gboolean, TRUE on success, FALSE otherwise
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GSource
 */
static gboolean sd_source_check(GSource *source)
{
        g_return_val_if_fail(source, FALSE);

        return sd_event_wait(((struct SDSource *) source)->event, 0) > 0 ? TRUE : FALSE;
}

/**
 * @brief Callback function: dispatch
 *
 * @param[in] source sd_event_source that should be dispatched
 * @param[in] callback not used
 * @param[in] userdata not used
 * @return gboolean, TRUE on success, FALSE otherwise
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GSource
 */
static gboolean sd_source_dispatch(GSource *source, GSourceFunc callback, gpointer userdata)
{
        g_return_val_if_fail(source, FALSE);

        return sd_event_dispatch(((struct SDSource *) source)->event) >= 0
               ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

/**
 * @brief Callback function: finalize GSource
 *
 * @param[in] source sd_event_source that should be finalized
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GSource
 */
static void sd_source_finalize(GSource *source)
{
        g_return_if_fail(source);

        sd_event_unref(((struct SDSource *) source)->event);
}

/**
 * @brief Callback function: when source exits
 *
 * @param[in] source sd_event_source that exits
 * @param[in] userdata the GMainLoop the source is attached to.
 * @return always return 0
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GMainLoop
 */
static int sd_source_on_exit(sd_event_source *source, void *userdata)
{
        g_return_val_if_fail(source, -1);
        g_return_val_if_fail(userdata, -1);

        g_main_loop_quit(userdata);

        sd_event_source_set_enabled(source, FALSE);
        sd_event_source_unref(source);

        return 0;
}

int sd_source_attach(GSource *source, GMainLoop *loop)
{
        g_return_val_if_fail(source, -1);
        g_return_val_if_fail(loop, -1);

        g_source_set_name(source, "sd-event");
        g_source_add_poll(source, &((struct SDSource *) source)->pollfd);
        g_source_attach(source, g_main_loop_get_context(loop));

        return sd_event_add_exit(((struct SDSource *) source)->event,
                                 NULL,
                                 sd_source_on_exit,
                                 loop);
}

GSource * sd_source_new(sd_event *event)
{
        static GSourceFuncs funcs = {
                sd_source_prepare,
                sd_source_check,
                sd_source_dispatch,
                sd_source_finalize,
        };
        GSource *s = NULL;

        g_return_val_if_fail(event, NULL);

        s = g_source_new(&funcs, sizeof(struct SDSource));
        if (s) {
                ((struct SDSource *) s)->event = sd_event_ref(event);
                ((struct SDSource *) s)->pollfd.fd = sd_event_get_fd(event);
                ((struct SDSource *) s)->pollfd.events = G_IO_IN | G_IO_HUP;
        }

        return s;
}
