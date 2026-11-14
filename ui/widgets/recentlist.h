#pragma once
#include <QWidget>

// Launcher list of recently opened files.
class RecentList : public QWidget
{
    Q_OBJECT
public:
    explicit RecentList(QWidget *parent = nullptr);
    void refresh();
    QSize sizeHint() const override;

signals:
    void openRequested(const QString &path);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { m_hover = -1; update(); }

private:
    int rowAt(const QPoint &p) const;
    QStringList m_files;
    int m_hover = -1;
    static constexpr int kRowHeight = 52;
    static constexpr int kHeaderHeight = 30;
    static constexpr int kMaxRows = 10;
};
