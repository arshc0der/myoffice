#pragma once
#include <QByteArray>
#include <QString>

#include "myppt/document/slidemodel.h"

// PPTX reader/writer on Qt's zip support.
// Reads: slide size, slide order, background colour, text boxes / autoshapes (rect, rounded rect, ellipse, line),
//        pictures, text (font size, bold/italic/underline, colour, alignment), placeholders (default geometry),
//        slide transitions (mapped to the nearest MyPowerPoint transition).
// Writes: a standards-conformant minimal package (master, layout, theme, slides, media) that PowerPoint opens.
// Object animations and speaker notes are stored in the package part ppt/myoffice/extras.xml. PowerPoint ignores
// that part (it does not play those animations); MyPowerPoint restores them.
// Skipped on read: tables, charts, SmartArt, video/audio, PowerPoint's own animation timeline, notes pages.
namespace PptxIO {
QByteArray write(const Deck &deck);
// `skipped` (optional) receives the number of unsupported elements that were dropped.
bool read(const QByteArray &zipData, Deck &out, QString *error, int *skipped = nullptr);
}
