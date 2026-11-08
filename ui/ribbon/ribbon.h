#pragma once
#include <QList>
#include <QStringList>
#include <QWidget>

#include "ribbonbutton.h"

class QHBoxLayout;
class QVBoxLayout;
class FadeStack;

// A titled cluster of commands. Large buttons sit side by side; small buttons/widgets stack in columns.
class RibbonGroup : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonGroup(const QString &title, QWidget *parent = nullptr);
    RibbonButton *addLarge(QAction *action, QMenu *menu = nullptr);
    void beginColumn();
    RibbonButton *addSmall(QAction *action, bool showText = true, QMenu *menu = nullptr);
    void addRow(const QList<QWidget *> &widgets);
    void addWidget(QWidget *w);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_title;
    QHBoxLayout *m_content;
    QVBoxLayout *m_col = nullptr;
};

class RibbonPage : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonPage(QWidget *parent = nullptr);
    RibbonGroup *addGroup(const QString &title);

private:
    QHBoxLayout *m_layout;
};

class RibbonTabStrip : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonTabStrip(QWidget *parent = nullptr);
    void addTab(const QString &title, bool primary);
    void setCurrentIndex(int i);
    int currentIndex() const { return m_current; }
    QSize sizeHint() const override { return { 400, 34 }; }

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { m_hover = -1; update(); }
    void keyPressEvent(QKeyEvent *e) override;

private:
    struct Tab { QString title; bool primary; };
    QRectF tabRect(int i) const;
    int indexAt(const QPointF &p) const;
    QList<Tab> m_tabs;
    int m_current = 0, m_hover = -1;
};

class Ribbon : public QWidget
{
    Q_OBJECT
public:
    explicit Ribbon(QWidget *parent = nullptr);
    RibbonPage *addPage(const QString &title, bool primary = false);
    void setCurrentIndex(int i);
    void setCornerWidget(QWidget *w);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    RibbonTabStrip *m_strip;
    FadeStack *m_pages;
    QHBoxLayout *m_topRow;
};
