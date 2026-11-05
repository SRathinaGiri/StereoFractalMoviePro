#ifndef SPHERICAL_H
#define SPHERICAL_H

#include <QByteArray>
#include <QString>

// 360-degree video metadata for VR headsets and 360 players.
//
// An equirectangular video is shown as a sphere only when its MP4 says so:
// the Spherical Video V2 boxes (Google's specification, read by YouTube,
// Meta Quest's players, VLC and others) inside the video track's sample
// entry: 'st3d' (the stereo layout) and 'sv3d' (with an equirectangular
// 'proj'), and the older Spherical Video V1 XML in a 'uuid' box in the
// video track (what Google's Spatial Media Metadata Injector writes, so
// what Facebook's 360 detection has been made to work with). FFmpeg
// writes neither, so they are added after encoding.
namespace Spherical {

enum class Stereo { Mono = 0, TopBottom = 1, LeftRight = 2 };

// The MP4 file `mp4` with the boxes added to every video track (metadata
// there already is replaced). The media data is unchanged; where the movie
// header comes before it, the chunk offsets are moved by its growth.
// Returns an empty array and sets `error` on failure.
QByteArray inject(const QByteArray &mp4, Stereo stereo, QString *error = nullptr);

// inject() on a file, in place.
bool injectFile(const QString &path, Stereo stereo, QString *error = nullptr);

} // namespace Spherical

#endif // SPHERICAL_H
