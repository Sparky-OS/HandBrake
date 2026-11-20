/* SPDX-License-Identifier: GPL-2.0-or-later */
/* stereo-preview.h
 * The preview of a 3D source shown in 3D on Stereo KWin (Sparky Stereo OS). */

#pragma once

#include "settings.h"

/* The still picture HandBrake made from the job settings at preview number index; the picture the scan decoded
 * after it (the other view of a packing whose views come in turn) is read when the packing needs it. */
void ghb_stereo_preview_still(signal_user_data_t *ud, GdkPixbuf *pix, int index);
/* The played clip starts (playing is TRUE) or stops. */
void ghb_stereo_preview_live(signal_user_data_t *ud, GtkMediaStream *video, gboolean playing);
