/* SPDX-License-Identifier: GPL-2.0-or-later */
/* stereo-preview.h
 * The preview of a 3D source shown in 3D on Stereo KWin (Sparky Stereo OS). */

#pragma once

#include "settings.h"

/* The still picture HandBrake made from the job settings. */
void ghb_stereo_preview_still(signal_user_data_t *ud, GdkPixbuf *pix);
/* The played clip starts (playing is TRUE) or stops. */
void ghb_stereo_preview_live(signal_user_data_t *ud, GtkMediaStream *video, gboolean playing);
