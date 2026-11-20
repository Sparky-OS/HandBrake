/* SPDX-License-Identifier: GPL-2.0-or-later */
/* stereo-preview.h
 * The preview of a 3D source shown in 3D on Stereo KWin (Sparky Stereo OS). */

#pragma once

#include "settings.h"

/* The still picture HandBrake made from the job settings at preview number index; the other picture of its pair
 * of views in turn (kept by the scan, counted from the source's first frame) is read when the packing needs it. */
void ghb_stereo_preview_still(signal_user_data_t *ud, GdkPixbuf *pix, int index);
/* The range of the clip encoded for preview number preview: for a source shown with the 3D menu, a time range that
 * starts on the frame boundary nearest before the preview's place, so the played pictures' frame numbers in the
 * source are known; FALSE leaves HandBrake's own preview range. */
gboolean ghb_stereo_preview_live_range(signal_user_data_t *ud, int preview, GhbValue *range);
/* FALSE when the clip of preview number preview was encoded without a known start in the source while the source is
 * shown with the 3D menu: it is encoded again. */
gboolean ghb_stereo_preview_clip_known(int preview);
/* The played clip of preview number preview starts (playing is TRUE) or stops. */
void ghb_stereo_preview_live(signal_user_data_t *ud, GtkMediaStream *video, gboolean playing, int preview);
