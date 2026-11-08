#pragma once
#include <QMarginsF>
#include <QSizeF>

// Page geometry shared by the editor, DOCX import/export and PDF export (96 dpi pixel units on screen).
struct PageSetup {
    double widthMm = 210, heightMm = 297; // portrait dimensions (A4)
    bool landscape = false;
    double marginLeftMm = 25.4, marginTopMm = 25.4, marginRightMm = 25.4, marginBottomMm = 25.4;
    bool pageNumbers = false;             // drawn in the footer of exported PDFs

    static double mmToPx(double mm) { return mm * 96.0 / 25.4; }
    QSizeF pageSizePx() const { return landscape ? QSizeF(mmToPx(heightMm), mmToPx(widthMm)) : QSizeF(mmToPx(widthMm), mmToPx(heightMm)); }
    QMarginsF marginsPx() const { return QMarginsF(mmToPx(marginLeftMm), mmToPx(marginTopMm), mmToPx(marginRightMm), mmToPx(marginBottomMm)); }
    QSizeF contentSizePx() const
    {
        const QSizeF s = pageSizePx();
        const QMarginsF m = marginsPx();
        return QSizeF(s.width() - m.left() - m.right(), s.height() - m.top() - m.bottom());
    }
};
