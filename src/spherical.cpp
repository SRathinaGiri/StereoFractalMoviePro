#include "spherical.h"

#include <QFile>
#include <QList>
#include <QSaveFile>
#include <cstring>

namespace Spherical {
namespace {

struct Box {
    QByteArray type;  // 4 bytes
    qsizetype start;  // offset of the box
    qsizetype header; // 8 or 16
    qsizetype end;    // offset after the box
};

quint32 be32(const QByteArray &b, qsizetype i) {
    const auto *p = reinterpret_cast<const uchar *>(b.constData()) + i;
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

quint64 be64(const QByteArray &b, qsizetype i) {
    return (quint64(be32(b, i)) << 32) | be32(b, i + 4);
}

void put32(QByteArray &o, quint32 v) {
    const char c[4] = {char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
    o.append(c, 4);
}

void put64(QByteArray &o, quint64 v) {
    put32(o, quint32(v >> 32));
    put32(o, quint32(v));
}

// The boxes of `b` from `from` to `to`.
bool boxes(const QByteArray &b, qsizetype from, qsizetype to, QList<Box> &out, QString *error) {
    qsizetype p = from;
    while (p + 8 <= to) {
        quint64 size = be32(b, p);
        QByteArray type = b.mid(p + 4, 4);
        qsizetype hl = 8;
        if (size == 0) {
            size = quint64(to - p);
        } else if (size == 1) {
            if (p + 16 > to) {
                if (error) *error = QStringLiteral("A box is cut off.");
                return false;
            }
            size = be64(b, p + 8);
            hl = 16;
        }
        if (size < quint64(hl) || quint64(p) + size > quint64(to)) {
            if (error) *error = QStringLiteral("Box '%1' does not fit in its file.").arg(QString::fromLatin1(type));
            return false;
        }
        out.append({type, p, hl, p + qsizetype(size)});
        p += qsizetype(size);
    }
    return true;
}

QByteArray makeBox(const char *type, const QByteArray &body) {
    QByteArray o;
    put32(o, quint32(body.size() + 8));
    o.append(type, 4);
    o.append(body);
    return o;
}

// A "full box": version and flags (0) before the body.
QByteArray fullBox(const char *type, const QByteArray &body) {
    return makeBox(type, QByteArray(4, '\0') + body);
}

QByteArray sphericalBoxes(Stereo stereo) {
    QByteArray st3d = fullBox("st3d", QByteArray(1, char(int(stereo))));
    QByteArray svhd = fullBox("svhd", QByteArray("StereoFractalMoviePro", 22)); // with its NUL
    QByteArray prhd = fullBox("prhd", QByteArray(12, '\0'));                    // yaw, pitch, roll 0
    QByteArray equi = fullBox("equi", QByteArray(16, '\0'));                    // no crop
    QByteArray proj = makeBox("proj", prhd + equi);
    return st3d + makeBox("sv3d", svhd + proj);
}

// The uuid of the Spherical Video V1 metadata box.
const QByteArray &v1Uuid() {
    static const QByteArray u = QByteArray::fromHex("ffcc8263f8554a938814587a02521fdd");
    return u;
}

// The Spherical Video V1 box (Google's spherical video RFC: XML in a uuid
// box in the video track), what Google's Spatial Media Metadata Injector
// writes (players such as Facebook's may only look for this one).
QByteArray sphericalV1Box(Stereo stereo) {
    QByteArray mode;
    if (stereo == Stereo::TopBottom) mode = "<GSpherical:StereoMode>top-bottom</GSpherical:StereoMode>";
    if (stereo == Stereo::LeftRight) mode = "<GSpherical:StereoMode>left-right</GSpherical:StereoMode>";
    QByteArray xml =
        "<?xml version=\"1.0\"?><rdf:SphericalVideo\n"
        "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n"
        "xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\">"
        "<GSpherical:Spherical>true</GSpherical:Spherical>"
        "<GSpherical:Stitched>true</GSpherical:Stitched>"
        "<GSpherical:StitchingSoftware>Spherical Metadata Tool</GSpherical:StitchingSoftware>"
        "<GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>" +
        mode + "</rdf:SphericalVideo>";
    return makeBox("uuid", v1Uuid() + xml);
}

bool isVideoEntry(const QByteArray &t) {
    static const char *const kinds[] = {"avc1", "avc3", "hvc1", "hev1", "mp4v", "vp09", "av01"};
    for (const char *k : kinds) {
        if (t == QByteArray(k, 4)) return true;
    }
    return false;
}

// The fields of a VisualSampleEntry before its boxes.
constexpr qsizetype kVisualFields = 78;

// The boxes of b[from, to) rewritten: the containers on the way to the
// video sample entries entered, the spherical boxes appended to those (old
// ones dropped), the chunk offsets moved by `delta`.
bool rewrite(const QByteArray &b, qsizetype from, qsizetype to, Stereo stereo, qint64 delta,
             int &found, QByteArray &out, QString *error) {
    QList<Box> list;
    if (!boxes(b, from, to, list, error)) return false;
    for (const Box &bx : list) {
        const qsizetype body = bx.start + bx.header;
        const QByteArray &t = bx.type;
        if (t == "trak") {
            // a video track also gets the V1 box (an old one dropped)
            const int before = found;
            QByteArray inner;
            if (!rewrite(b, body, bx.end, stereo, delta, found, inner, error)) return false;
            if (found > before) {
                QList<Box> kids;
                if (!boxes(inner, 0, inner.size(), kids, error)) return false;
                QByteArray t2;
                for (const Box &k : kids) {
                    const bool v1 = k.type == "uuid" && inner.mid(k.start + k.header, 16) == v1Uuid();
                    if (!v1) t2.append(inner.mid(k.start, k.end - k.start));
                }
                t2.append(sphericalV1Box(stereo));
                out.append(makeBox("trak", t2));
            } else {
                out.append(makeBox("trak", inner));
            }
        } else if (t == "moov" || t == "mdia" || t == "minf" || t == "stbl") {
            QByteArray inner;
            if (!rewrite(b, body, bx.end, stereo, delta, found, inner, error)) return false;
            out.append(makeBox(t.constData(), inner));
        } else if (t == "stsd") {
            if (bx.end - body < 8) {
                if (error) *error = QStringLiteral("Sample description too short.");
                return false;
            }
            QByteArray nb = b.mid(body, 8);
            QList<Box> entries;
            if (!boxes(b, body + 8, bx.end, entries, error)) return false;
            for (const Box &e : entries) {
                const qsizetype eb = e.start + e.header;
                if (isVideoEntry(e.type) && e.end - eb >= kVisualFields) {
                    ++found;
                    QByteArray entry = b.mid(eb, kVisualFields);
                    QList<Box> children;
                    if (!boxes(b, eb + kVisualFields, e.end, children, error)) return false;
                    for (const Box &c : children) {
                        if (c.type != "st3d" && c.type != "sv3d") {
                            entry.append(b.mid(c.start, c.end - c.start));
                        }
                    }
                    entry.append(sphericalBoxes(stereo));
                    nb.append(makeBox(e.type.constData(), entry));
                } else {
                    nb.append(b.mid(e.start, e.end - e.start));
                }
            }
            out.append(makeBox("stsd", nb));
        } else if ((t == "stco" || t == "co64") && delta != 0) {
            const bool wide = (t == "co64");
            if (bx.end - body < 8) {
                if (error) *error = QStringLiteral("Chunk offsets too short.");
                return false;
            }
            const quint32 n = be32(b, body + 4);
            const qsizetype w = wide ? 8 : 4;
            if (bx.end - body < 8 + qsizetype(n) * w) {
                if (error) *error = QStringLiteral("Chunk offsets cut off.");
                return false;
            }
            QByteArray nb = b.mid(body, 8);
            for (quint32 i = 0; i < n; ++i) {
                const qsizetype p = body + 8 + qsizetype(i) * w;
                if (wide) {
                    put64(nb, quint64(qint64(be64(b, p)) + delta));
                } else {
                    const qint64 v = qint64(be32(b, p)) + delta;
                    if (v < 0 || v > qint64(0xFFFFFFFFu)) {
                        if (error) *error = QStringLiteral("The file is too large for its 32-bit chunk offsets.");
                        return false;
                    }
                    put32(nb, quint32(v));
                }
            }
            out.append(makeBox(t.constData(), nb));
        } else {
            out.append(b.mid(bx.start, bx.end - bx.start));
        }
    }
    return true;
}

} // namespace

QByteArray inject(const QByteArray &mp4, Stereo stereo, QString *error) {
    QList<Box> top;
    if (!boxes(mp4, 0, mp4.size(), top, error)) return {};
    const Box *moov = nullptr;
    for (const Box &b : top) {
        if (b.type == "moov") moov = &b;
    }
    if (!moov) {
        if (error) *error = QStringLiteral("No movie header ('moov'): not an MP4 file.");
        return {};
    }
    bool mdatAfter = false;
    for (const Box &b : top) {
        if (b.type == "mdat" && b.start > moov->start) mdatAfter = true;
    }
    int found = 0;
    QByteArray first;
    if (!rewrite(mp4, moov->start, moov->end, stereo, 0, found, first, error)) return {};
    if (found == 0) {
        if (error) *error = QStringLiteral("No video track (H.264, H.265, MPEG-4, VP9 or AV1) found.");
        return {};
    }
    const qint64 delta = qint64(first.size()) - qint64(moov->end - moov->start);
    QByteArray newMoov = first;
    if (mdatAfter && delta != 0) {
        int f = 0;
        newMoov.clear();
        if (!rewrite(mp4, moov->start, moov->end, stereo, delta, f, newMoov, error)) return {};
    }
    QByteArray out;
    out.reserve(mp4.size() + newMoov.size() - (moov->end - moov->start));
    out.append(mp4.left(moov->start));
    out.append(newMoov);
    out.append(mp4.mid(moov->end));
    return out;
}

bool injectFile(const QString &path, Stereo stereo, QString *error) {
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot read %1").arg(path);
        return false;
    }
    const QByteArray data = in.readAll();
    in.close();
    const QByteArray out = inject(data, stereo, error);
    if (out.isEmpty()) return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(out) != out.size() || !f.commit()) {
        if (error) *error = QStringLiteral("Cannot write %1").arg(path);
        return false;
    }
    return true;
}

} // namespace Spherical
