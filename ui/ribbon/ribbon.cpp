#include "ribbon.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "ui/widgets/fadestack.h"

// ---------------- RibbonGroup ----------------
RibbonGroup::RibbonGroup(const QString &title, QWidget *parent) : QWidget(parent), m_title(title)
{
    m_content = new QHBoxLayout(this);
    m_content->setContentsMargins(8, 4, 8, 20);
    m_content->setSpacing(2);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

RibbonButton *RibbonGroup::addLarge(QAction *action, QMenu *menu)
{
    auto *b = new RibbonButton(action, RibbonButton::Size::Large, this);
    if (menu) { b->setMenu(menu); b->setFixedSize(b->sizeHint()); }
    m_content->addWidget(b, 0, Qt::AlignTop);
    m_col = nullptr;
    return b;
}

void RibbonGroup::beginColumn()
{
    m_col = new QVBoxLayout;
    m_col->setContentsMargins(0, 0, 0, 0);
    m_col->setSpacing(2);
    m_col->addStretch(1);
    m_content->addLayout(m_col);
}

RibbonButton *RibbonGroup::addSmall(QAction *action, bool showText, QMenu *menu)
{
    if (!m_col) beginColumn();
    auto *b = new RibbonButton(action, showText ? RibbonButton::Size::Small : RibbonButton::Size::SmallIcon, this);
    if (menu) { b->setMenu(menu); b->setFixedSize(b->sizeHint()); }
    m_col->insertWidget(m_col->count() - 1, b, 0, Qt::AlignLeft);
    return b;
}

void RibbonGroup::addRow(const QList<QWidget *> &widgets)
{
    if (!m_col) beginColumn();
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);
    for (QWidget *w : widgets) row->addWidget(w);
    row->addStretch(1);
    m_col->insertLayout(m_col->count() - 1, row);
}

void RibbonGroup::addWidget(QWidget *w)
{
    if (!m_col) beginColumn();
    m_col->insertWidget(m_col->count() - 1, w, 0, Qt::AlignLeft);
}

void RibbonGroup::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setPen(pal.border);
    p.drawLine(width() - 1, 8, width() - 1, height() - 8);
    p.setPen(pal.textSecondary);
    p.setFont(theme.uiFont(Design::Type::caption - 0.5));
    p.drawText(QRectF(0, height() - 19, width(), 16), Qt::AlignHCenter | Qt::AlignVCenter, m_title);
}

// ---------------- RibbonPage ----------------
RibbonPage::RibbonPage(QWidget *parent) : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 2);
    m_layout->setSpacing(0);
    m_layout->addStretch(1);
}

RibbonGroup *RibbonPage::addGroup(const QString &title)
{
    auto *g = new RibbonGroup(title, this);
    m_layout->insertWidget(m_layout->count() - 1, g);
    return g;
}

// ---------------- RibbonTabStrip ----------------
RibbonTabStrip::RibbonTabStrip(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(34);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Ribbon tabs"));
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void RibbonTabStrip::addTab(const QString &title, bool primary) { m_tabs.append({ title, primary }); update(); }

QRectF RibbonTabStrip::tabRect(int i) const
{
    const QFontMetrics fm(ThemeManager::instance().uiFont(Design::Type::body, QFont::DemiBold));
    qreal x = 10;
    for (int k = 0; k < i; ++k) x += fm.horizontalAdvance(m_tabs[k].title) + 34;
    return QRectF(x, 3, fm.horizontalAdvance(m_tabs[i].title) + 30, height() - 3);
}

int RibbonTabStrip::indexAt(const QPointF &p) const
{
    for (int i = 0; i < m_tabs.size(); ++i) if (tabRect(i).contains(p)) return i;
    return -1;
}

void RibbonTabStrip::setCurrentIndex(int i)
{
    if (i < 0 || i >= m_tabs.size() || i == m_current) return;
    m_current = i;
    update();
    emit currentChanged(i);
}

void RibbonTabStrip::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < m_tabs.size(); ++i) {
        const QRectF r = tabRect(i);
        const bool cur = i == m_current;
        if (m_tabs[i].primary) {
            p.setPen(Qt::NoPen);
            p.setBrush(i == m_hover ? pal.accentDeep : pal.accent);
            p.drawRoundedRect(r.adjusted(2, 2, -2, -4), 7, 7);
            p.setPen(Qt::white);
        } else {
            if (i == m_hover && !cur) { p.setPen(Qt::NoPen); p.setBrush(pal.surfaceHover); p.drawRoundedRect(r.adjusted(0, 2, 0, -4), 7, 7); }
            p.setPen(cur ? pal.accentDeep : pal.text);
        }
        p.setFont(theme.uiFont(Design::Type::body, cur ? QFont::Bold : QFont::DemiBold));
        p.drawText(r, Qt::AlignCenter, m_tabs[i].title);
        if (cur && !m_tabs[i].primary) { p.setPen(Qt::NoPen); p.setBrush(pal.accent); p.drawRoundedRect(QRectF(r.left() + 10, height() - 3, r.width() - 20, 3), 1.5, 1.5); }
    }
    if (hasFocus()) { p.setBrush(Qt::NoBrush); p.setPen(QPen(pal.accent, 1, Qt::DotLine)); p.drawRect(rect().adjusted(1, 1, -1, -1)); }
}

void RibbonTabStrip::mousePressEvent(QMouseEvent *e) { const int i = indexAt(e->position()); if (i >= 0) setCurrentIndex(i); }
void RibbonTabStrip::mouseMoveEvent(QMouseEvent *e) { m_hover = indexAt(e->position()); update(); }
void RibbonTabStrip::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Right) setCurrentIndex(qMin(m_current + 1, int(m_tabs.size()) - 1));
    else if (e->key() == Qt::Key_Left) setCurrentIndex(qMax(m_current - 1, 0));
    else QWidget::keyPressEvent(e);
}

// ---------------- Ribbon ----------------
Ribbon::Ribbon(QWidget *parent) : QWidget(parent)
{
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    m_topRow = new QHBoxLayout;
    m_topRow->setContentsMargins(0, 0, 8, 0);
    m_strip = new RibbonTabStrip(this);
    m_topRow->addWidget(m_strip, 1);
    col->addLayout(m_topRow);
    m_pages = new FadeStack(this);
    m_pages->setFixedHeight(96);
    col->addWidget(m_pages);
    connect(m_strip, &RibbonTabStrip::currentChanged, this, [this](int i) { m_pages->fadeTo(m_pages->widget(i)); });
}

RibbonPage *Ribbon::addPage(const QString &title, bool primary)
{
    auto *page = new RibbonPage(m_pages);
    m_pages->addWidget(page);
    m_strip->addTab(title, primary);
    return page;
}

void Ribbon::setCurrentIndex(int i) { m_strip->setCurrentIndex(i); }
void Ribbon::setCornerWidget(QWidget *w) { m_topRow->addWidget(w); }

void Ribbon::paintEvent(QPaintEvent *)
{
    const Palette &pal = ThemeManager::instance().palette();
    QPainter p(this);
    p.fillRect(rect(), pal.surface);
    p.setPen(pal.border);
    p.drawLine(0, height() - 1, width(), height() - 1);
}
