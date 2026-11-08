#pragma once
#include <QWidget>

// Transient notification anchored to the bottom of the top-level window.
class Toast : public QWidget
{
    Q_OBJECT
public:
    enum Kind { Info = 0, Success = 1, Error = 2 };
    static void show(QWidget *host, const QString &text, int kind = Info, int ms = 3200);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Toast(QWidget *parent, const QString &text, int kind);
    QString m_text;
    int m_kind;
};
