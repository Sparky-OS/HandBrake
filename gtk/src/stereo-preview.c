/* SPDX-License-Identifier: GPL-2.0-or-later */
/* stereo-preview.c
 *
 * The preview of a 3D source shown in 3D on Stereo KWin (Sparky Stereo OS, ORDER 742).
 * The preview hands the two views of the source as one declared full side-by-side area inside the
 * preview window (the window itself is never declared), through the stereo layer for GTK 4. Every view
 * keeps its own samples; Stereo KWin scales once, at the output. The packing comes from the file's
 * entry in the shared per-file store, then from the scan, then from the frame's shape. What is encoded
 * never changes here, and the source file is never written.
 */

#include "stereo-preview.h"

#include "application.h"
#include "callbacks.h"
#include "handbrake/handbrake.h"
#include "hb-backend.h"

#include <stereo-gtk.h>

/* AV_STEREO3D_FLAG_INVERT of libavutil/stereo3d.h, carried as is in hb_stereo_3d_t.flags */
#define GHB_STEREO_INVERT 1

typedef struct
{
    GtkWidget *picture, *area, *menu, *message;
    char *path;
    int scan_layout, scan_order;
    GdkTexture *still, *still_next;
    /* the still's preview number, and whether its neighbour was asked for (it is read only when needed) */
    signal_user_data_t *ud;
    int still_index;
    gboolean still_next_read;
    GtkMediaStream *video;
    /* a packing whose views come in turn: the first view of the pair being played, its frame number */
    GdkTexture *live_first;
    gint64 live_first_index;
    int rate_num, rate_den;
    gulong video_handler;
    gboolean live;
} stereo_preview_t;

static stereo_preview_t *sp;

static void show_message (const char *text)
{
    if (text && *text)
    {
        gtk_label_set_text(GTK_LABEL(sp->message), text);
        ghb_log("Preview: %s", text);
    }
    gtk_widget_set_visible(sp->message, text && *text);
}

static int layout_from_scan (int type)
{
    switch (type)
    {
        case HB_STEREO3D_SIDEBYSIDE:          return STEREO_POLICY_LAYOUT_SIDE_BY_SIDE;
        case HB_STEREO3D_TOPBOTTOM:           return STEREO_POLICY_LAYOUT_TOP_AND_BOTTOM;
        case HB_STEREO3D_FRAMESEQUENCE:       return STEREO_POLICY_LAYOUT_FRAME_SEQUENTIAL;
        case HB_STEREO3D_CHECKERBOARD:        return STEREO_POLICY_LAYOUT_CHECKERBOARD;
        case HB_STEREO3D_SIDEBYSIDE_QUINCUNX: return STEREO_POLICY_LAYOUT_SIDE_BY_SIDE_QUINCUNX;
        case HB_STEREO3D_LINES:               return STEREO_POLICY_LAYOUT_ROWS;
        case HB_STEREO3D_COLUMNS:             return STEREO_POLICY_LAYOUT_COLUMNS;
        default:                              return STEREO_POLICY_LAYOUT_NONE;
    }
}

/* The file's entry first (a declaration can be wrong), then the scan, then the shape; the swap last. */
static gboolean decide (int width, int height, double aspect, int *layout, int *order, int *anaglyph)
{
    struct stereo_policy_file_choice choice = {0};
    stereo_gtk_file_menu_get_choice(sp->menu, &choice);
    *layout = STEREO_POLICY_LAYOUT_NONE;
    *order = STEREO_POLICY_LEFT_FIRST;
    *anaglyph = STEREO_POLICY_ANAGLYPH_NONE;
    if (choice.view == STEREO_POLICY_VIEW_MONO)
        return FALSE;
    if (choice.layout != STEREO_POLICY_LAYOUT_NONE || choice.anaglyph != STEREO_POLICY_ANAGLYPH_NONE)
    {
        *layout = choice.layout;
        *order = choice.order;
        *anaglyph = choice.anaglyph;
    }
    else if (sp->scan_layout != STEREO_POLICY_LAYOUT_NONE)
    {
        *layout = sp->scan_layout;
        *order = sp->scan_order;
    }
    else if (height > 0 && stereo_policy_video_side_by_side_by_shape((int)(height * aspect + 0.5), height))
    {
        *layout = STEREO_POLICY_LAYOUT_SIDE_BY_SIDE;
    }
    if (choice.swap_eyes)
        *order = !*order;
    (void)width;
    return *layout != STEREO_POLICY_LAYOUT_NONE || *anaglyph != STEREO_POLICY_ANAGLYPH_NONE;
}

/* The box the picture is shown in takes the shape of one view while the area shows the pair. */
static void set_box (gboolean pair, double aspect)
{
    GtkWidget *frame = ghb_builder_widget("preview_image_frame");
    if (aspect > 0)
        gtk_aspect_frame_set_ratio(GTK_ASPECT_FRAME(frame), aspect);
    gtk_widget_set_visible(sp->area, pair);
}

/* frame is the picture shown; second, when there is one, the picture decoded after it (the other view of a
 * packing whose views come in turn) */
static void present (GdkTexture *frame, GdkTexture *second, double aspect)
{
    int layout, order, anaglyph;
    GdkTexture *left = NULL, *right = NULL;
    g_autoptr(GError) error = NULL;

    if (frame == NULL)
        return;
    int width = gdk_texture_get_width(frame), height = gdk_texture_get_height(frame);
    if (aspect <= 0)
        aspect = (double)width / height;
    if (!decide(width, height, aspect, &layout, &order, &anaglyph))
    {
        set_box(FALSE, aspect);
        show_message(NULL);
        return;
    }
    gboolean unpacked;
    if (stereo_gtk_layout_needs_two_pictures(layout) && frame == sp->still && second == NULL && !sp->still_next_read)
    {
        sp->still_next_read = TRUE;
        GdkPixbuf *next = ghb_get_preview_neighbour(sp->still_index, sp->ud);
        if (next)
        {
            sp->still_next = second = gdk_texture_new_for_pixbuf(next);
            g_object_unref(next);
        }
    }
    if (stereo_gtk_layout_needs_two_pictures(layout) && second != NULL)
        unpacked = stereo_gtk_unpack_pair(frame, second, layout, order, &left, &right, &error);
    else
        unpacked = stereo_gtk_unpack(frame, layout, order, anaglyph, &left, &right, &error);
    if (!unpacked)
    {
        set_box(FALSE, aspect);
        show_message(error->message);
        return;
    }
    show_message(NULL);
    double view = stereo_gtk_view_aspect(width, height, aspect, layout);
    stereo_gtk_area_set_view_aspect(sp->area, view);
    stereo_gtk_area_set_pair(sp->area, left, right);
    stereo_gtk_area_set_enabled(sp->area, stereo_gtk_file_menu_get_enabled(sp->menu));
    set_box(TRUE, view);
    g_object_unref(left);
    g_object_unref(right);
}

/* The clip's frame number, from the stream's time and the source's frame rate. */
static gint64 live_index (void)
{
    if (sp->rate_num <= 0 || sp->rate_den <= 0)
        return -1;
    gint64 us = gtk_media_stream_get_timestamp(sp->video);
    return (us * sp->rate_num + sp->rate_den * G_GINT64_CONSTANT(500000)) / (sp->rate_den * G_GINT64_CONSTANT(1000000));
}

static void live_frame (GdkPaintable *paintable, gpointer data)
{
    (void)data;
    g_autoptr(GdkPaintable) image = gdk_paintable_get_current_image(paintable);
    if (!GDK_IS_TEXTURE(image))
        return;
    double aspect = gdk_paintable_get_intrinsic_aspect_ratio(paintable);
    int layout, order, anaglyph;
    int width = gdk_texture_get_width(GDK_TEXTURE(image)), height = gdk_texture_get_height(GDK_TEXTURE(image));
    if (decide(width, height, aspect > 0 ? aspect : (double)width / height, &layout, &order, &anaglyph) &&
        stereo_gtk_layout_needs_two_pictures(layout))
    {
        /* views in turn: an even frame opens a pair, the odd frame after it closes it */
        gint64 index = live_index();
        if (index >= 0 && index % 2 == 0)
        {
            g_set_object(&sp->live_first, GDK_TEXTURE(image));
            sp->live_first_index = index;
            return;
        }
        if (index < 0 || sp->live_first == NULL || sp->live_first_index != index - 1)
            return;
        present(sp->live_first, GDK_TEXTURE(image), aspect);
        g_clear_object(&sp->live_first);
        return;
    }
    present(GDK_TEXTURE(image), NULL, aspect);
}

static void refresh (void)
{
    if (sp->live && sp->video)
        live_frame(GDK_PAINTABLE(sp->video), NULL);
    else
        present(sp->still, sp->still_next, 0);
}

static void file_changed (GtkWidget *button, int error_number, gpointer data)
{
    (void)button; (void)data;
    if (error_number)
    {
        show_message(g_strerror(error_number));
        return;
    }
    refresh();
}

static void enable_changed (GtkWidget *button, gboolean enabled, gpointer data)
{
    (void)button; (void)data;
    stereo_gtk_area_set_enabled(sp->area, enabled);
}

static void area_error (GtkWidget *area, const char *text, gpointer data)
{
    (void)area; (void)data;
    show_message(text);
}

static void init (void)
{
    if (sp != NULL)
        return;
    sp = g_new0(stereo_preview_t, 1);
    sp->picture = ghb_builder_widget("preview_image");
    GtkWidget *frame = ghb_builder_widget("preview_image_frame");

    /* The picture stays under the area: its click and motion controllers keep working, the area
     * takes no events, and the window cuts the area's hole only while both views are presented. */
    GtkWidget *overlay = gtk_overlay_new();
    g_object_ref(sp->picture);
    gtk_aspect_frame_set_child(GTK_ASPECT_FRAME(frame), NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), sp->picture);
    g_object_unref(sp->picture);
    sp->area = stereo_gtk_area_new();
    gtk_widget_set_can_target(sp->area, FALSE);
    gtk_widget_set_visible(sp->area, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), sp->area);
    sp->message = gtk_label_new(NULL);
    gtk_label_set_wrap(GTK_LABEL(sp->message), TRUE);
    gtk_widget_set_halign(sp->message, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(sp->message, GTK_ALIGN_START);
    gtk_widget_add_css_class(sp->message, "osd");
    gtk_widget_set_visible(sp->message, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), sp->message);
    gtk_aspect_frame_set_child(GTK_ASPECT_FRAME(frame), overlay);
    g_signal_connect(sp->area, "stereo-error", G_CALLBACK(area_error), NULL);

    stereo_gtk_area_keep_above(sp->area, ghb_builder_widget("preview_hud"));
    stereo_gtk_area_keep_above(sp->area, sp->message);

    sp->menu = stereo_gtk_file_menu_new(file_changed, enable_changed, NULL);
    GtkWidget *play = ghb_builder_widget("live_preview_play");
    gtk_box_append(GTK_BOX(gtk_widget_get_parent(play)), sp->menu);
}

static void set_source (signal_user_data_t *ud)
{
    int title_id = ghb_dict_get_int(ud->settings, "title"), index;
    const hb_title_t *title = ghb_lookup_title(title_id, &index);
    const char *path = title ? title->path : NULL;

    sp->scan_layout = title ? layout_from_scan(title->stereo_3d.type) : STEREO_POLICY_LAYOUT_NONE;
    sp->rate_num = title ? title->vrate.num : 0;
    sp->rate_den = title ? title->vrate.den : 0;
    sp->scan_order = title && (title->stereo_3d.flags & GHB_STEREO_INVERT) ? STEREO_POLICY_RIGHT_FIRST
                                                                         : STEREO_POLICY_LEFT_FIRST;
    if (g_strcmp0(path, sp->path) == 0)
        return;
    g_free(sp->path);
    sp->path = g_strdup(path);
    struct stereo_policy_file_choice choice;
    gboolean entry = path && stereo_policy_file_choice(path, &choice) == 1;
    gboolean shape = title && stereo_policy_video_side_by_side_by_shape(
        title->geometry.width * MAX(title->geometry.par.num, 1) / MAX(title->geometry.par.den, 1),
        title->geometry.height);
    stereo_gtk_file_menu_set_file(sp->menu, path,
                                  sp->scan_layout != STEREO_POLICY_LAYOUT_NONE || shape || entry);
}

void ghb_stereo_preview_still (signal_user_data_t *ud, GdkPixbuf *pix, int index)
{
    init();
    set_source(ud);
    g_clear_object(&sp->still);
    g_clear_object(&sp->still_next);
    sp->ud = ud;
    sp->still_index = index;
    sp->still_next_read = FALSE;
    if (pix)
        sp->still = gdk_texture_new_for_pixbuf(pix);
    if (!sp->live)
        present(sp->still, sp->still_next, 0);
}

void ghb_stereo_preview_live (signal_user_data_t *ud, GtkMediaStream *video, gboolean playing)
{
    init();
    set_source(ud);
    if (sp->video && sp->video_handler)
        g_signal_handler_disconnect(sp->video, sp->video_handler);
    sp->video_handler = 0;
    sp->video = video;
    sp->live = playing;
    g_clear_object(&sp->live_first);
    if (playing && video)
        sp->video_handler = g_signal_connect(video, "invalidate-contents", G_CALLBACK(live_frame), NULL);
    refresh();
}
