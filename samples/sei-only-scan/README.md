# Test files: 3D sources declared only by the frame packing SEI

Five files for HandBrake's scan, two seconds each, generated with ffmpeg's own test sources (no outside media).
The four 3D files carry the frame packing SEI in the video stream and nothing in the container (no st3d box).

| File | Video | Layout in the SEI |
| --- | --- | --- |
| sbs-h264-sei-only.mp4 | H.264, 1280x360 | side by side |
| tab-h264-sei-only.mp4 | H.264, 640x720 | top and bottom |
| sbs-hevc-sei-only.mp4 | HEVC, 1280x360 | side by side |
| tab-hevc-sei-only.mp4 | HEVC, 640x720 | top and bottom |
| control-2d.mp4 | H.264, 640x360 | none (2D control) |

The H.264 files come from x264 with `frame-packing=3` and `frame-packing=4`.
The HEVC files were encoded with x265 and the SEI was inserted before the first picture.
