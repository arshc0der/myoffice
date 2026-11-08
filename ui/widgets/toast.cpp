#include "toast.h"

#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>

#include "core/designtokens.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"

Toast::Toast(QWidget *parent, const QString &text, int kind) : QWidget(parent), m_text(text), m_kind(kind)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAccessibleName(text);
    const QFontMetrics fm(ThemeManager::instance().uiFont(Design::Type::body));
    setFixedSize(qMin(560, fm.horizontalAdvance(text) + 76), 44);
}

void Toast::show(QWidget *host, const QString &text, int kind, int ms)
{
    if (!host) return;
    QWidget *top = host->window();
    auto *t = new Toast(top, text, kind);
    t->move((top->width() - t->width()) / 2, top->height() - t->height() - 56);
    t->QWidget::show();
    t->raise();

    auto *fx = new QGraphicsOpacityEffect(t);
    t->setGraphicsEffect(fx);
    const int fade = ThemeManager::instance().duration(Design::Duration::normal);
    auto *in = new QPropertyAnimation(fx, "opacity", t);
    in->setDuration(fade);
    in->setStartValue(0.0);
    in->setEndValue(1.0);
    in->start(QAbstractAnimation::DeleteWhenStopped);
    QTimer::singleShot(ms, t, [t, fx, fade] {
        auto *out = new QPropertyAnimation(fx, "opacity", t);
        out->setDuration(fade);
        out->setStartValue(1.0);
        out->setEndValue(0.0);
        connect(out, &QPropertyAnimation::finished, t, &QObject::deleteLater);
        out->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void Toast::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    p.setBrush(pal.surface);
    p.setPen(QPen(pal.border, 1));
    p.drawRoundedRect(r, 10, 10);
    const QColor tint = m_kind == Error ? pal.danger : (m_kind == Success ? QColor("#1E9E62") : pal.accent);
    const QString icon = m_kind == Error ? QStringLiteral("alert") : (m_kind == Success ? QStringLiteral("check") : QStringLiteral("info"));
    p.drawPixmap(QPointF(16, (height() - 18) / 2.0), Icons::pixmap(icon, tint, 18, devicePixelRatioF()));
    p.setPen(pal.text);
    p.setFont(theme.uiFont(Design::Type::body));
    p.drawText(QRectF(44, 0, width() - 56, height()), Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(m_text, Qt::ElideRight, width() - 56));
}
