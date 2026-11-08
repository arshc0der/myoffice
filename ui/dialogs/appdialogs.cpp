#include "appdialogs.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/filemanager.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"
#include "ui/widgets/pillbutton.h"
#include "ui/widgets/themeutil.h"
#include "ui/widgets/toast.h"
#include "ui/widgets/toggleswitch.h"

namespace AppDialogs {

static QPixmap logoPixmap(int size, qreal dpr)
{
    const Palette &pal = ThemeManager::instance().palette();
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(pal.accent);
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.26, size * 0.26);
    p.setPen(Qt::white);
    p.setFont(ThemeManager::instance().uiFont(size * 0.42, QFont::Bold));
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("M"));
    return pm;
}

void showAbout(QWidget *parent)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("About MyOffice"));
    dlg.setMinimumWidth(560);
    auto *col = new QVBoxLayout(&dlg);
    col->setContentsMargins(28, 24, 28, 22);
    col->setSpacing(14);

    auto *head = new QHBoxLayout;
    head->setSpacing(16);
    auto *logo = new QLabel(&dlg);
    logo->setPixmap(logoPixmap(64, dlg.devicePixelRatioF()));
    auto *titles = new QVBoxLayout;
    auto *name = new QLabel(QStringLiteral("MyOffice"), &dlg);
    name->setFont(theme.uiFont(Design::Type::display, QFont::DemiBold));
    auto *version = new QLabel(QObject::tr("Version %1  ·  Qt %2").arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion())), &dlg);
    setTextColor(version, pal.textSecondary);
    titles->addWidget(name);
    titles->addWidget(version);
    head->addWidget(logo);
    head->addLayout(titles, 1);
    col->addLayout(head);

    auto *desc = new QLabel(QObject::tr("A modern desktop productivity suite — MyWord for documents, MyExcel for spreadsheets and "
                                        "MyPowerPoint for presentations — in one fast native application built with C++ and Qt."), &dlg);
    desc->setWordWrap(true);
    col->addWidget(desc);

    auto *features = new QLabel(QObject::tr(
        "<ul style='margin-left:-18px'>"
        "<li><b>MyWord</b> — rich text, tables, pictures, find &amp; replace, DOCX / HTML / Markdown / PDF</li>"
        "<li><b>MyExcel</b> — formulas, sorting, formatting, multiple sheets, XLSX / CSV / PDF</li>"
        "<li><b>MyPowerPoint</b> — shapes, pictures, animations, transitions, slide show, PPTX / PDF / PNG</li>"
        "<li>Open any file in <b>view mode</b>, then enable editing when you are ready</li>"
        "<li>Light and dark themes, a different colour for each app, document tabs</li></ul>"), &dlg);
    features->setWordWrap(true);
    col->addWidget(features);

    auto *repoRow = new QHBoxLayout;
    auto *repoIcon = new QLabel(&dlg);
    repoIcon->setPixmap(Icons::pixmap(QStringLiteral("code"), pal.accent, 18, dlg.devicePixelRatioF()));
    auto *link = new QLabel(QStringLiteral("<a href=\"%1\">%1</a>").arg(kRepoUrl), &dlg);
    link->setOpenExternalLinks(true);
    link->setTextInteractionFlags(Qt::TextBrowserInteraction);
    link->setAccessibleName(QObject::tr("Project repository"));
    repoRow->addWidget(repoIcon);
    repoRow->addWidget(link, 1);
    col->addLayout(repoRow);

    auto *by = new QLabel(QObject::tr("Created by Arsh (arshc0der). Contributions and bug reports are welcome on GitHub."), &dlg);
    by->setWordWrap(true);
    setTextColor(by, pal.textSecondary);
    col->addWidget(by);

    auto *buttons = new QHBoxLayout;
    auto *gh = new PillButton(QObject::tr("View on GitHub"), QStringLiteral("code"), true, &dlg);
    auto *issue = new PillButton(QObject::tr("Report an issue"), QStringLiteral("alert"), false, &dlg);
    auto *close = new PillButton(QObject::tr("Close"), QString(), false, &dlg);
    buttons->addWidget(gh);
    buttons->addWidget(issue);
    buttons->addStretch(1);
    buttons->addWidget(close);
    col->addLayout(buttons);
    QObject::connect(gh, &PillButton::clicked, &dlg, [] { QDesktopServices::openUrl(QUrl(kRepoUrl)); });
    QObject::connect(issue, &PillButton::clicked, &dlg, [] { QDesktopServices::openUrl(QUrl(kRepoUrl + QStringLiteral("/issues"))); });
    QObject::connect(close, &PillButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void showShortcuts(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Keyboard shortcuts"));
    auto *col = new QVBoxLayout(&dlg);
    col->setContentsMargins(24, 20, 24, 20);
    struct Row { const char *keys, *what; };
    const struct { const char *title; QList<Row> rows; } groups[] = {
        { "Everywhere", { { "Ctrl+N / O / S", "New / Open / Save" }, { "Ctrl+Shift+S", "Save As" }, { "Ctrl+W", "Close tab" }, { "Ctrl+Shift+D", "Toggle light / dark theme" }, { "F1", "About MyOffice" } } },
        { "MyWord", { { "Ctrl+B / I / U", "Bold / italic / underline" }, { "Ctrl+L / E / R / J", "Align left / center / right / justify" }, { "Ctrl+F / H", "Find / replace" }, { "Ctrl+K", "Insert link" }, { "Ctrl+Enter", "Page break" } } },
        { "MyExcel", { { "Ctrl+C / X / V", "Copy / cut / paste cells" }, { "Ctrl+D / R", "Fill down / right" }, { "Delete", "Clear cells" }, { "Ctrl+F", "Find and replace" } } },
        { "MyPowerPoint", { { "Ctrl+M", "New slide" }, { "Ctrl+D", "Duplicate selected object" }, { "F5 / Shift+F5", "Slide show from beginning / current slide" }, { "Space, →, click", "Next step / slide" }, { "←, Backspace", "Previous slide" }, { "Esc", "End slide show" } } },
    };
    QString html = QStringLiteral("<table cellpadding='4'>");
    for (const auto &g : groups) {
        html += QStringLiteral("<tr><td colspan='2'><b>%1</b></td></tr>").arg(QObject::tr(g.title));
        for (const Row &r : g.rows) html += QStringLiteral("<tr><td><code>%1</code>&nbsp;&nbsp;</td><td>%2</td></tr>").arg(QString::fromUtf8(r.keys).toHtmlEscaped(), QObject::tr(r.what).toHtmlEscaped());
    }
    html += QStringLiteral("</table>");
    auto *label = new QLabel(html, &dlg);
    label->setTextFormat(Qt::RichText);
    col->addWidget(label);
    auto *close = new PillButton(QObject::tr("Close"), QString(), true, &dlg);
    col->addWidget(close, 0, Qt::AlignRight);
    QObject::connect(close, &PillButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void showSettings(QWidget *parent)
{
    auto &theme = ThemeManager::instance();
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Settings"));
    dlg.setMinimumWidth(440);
    auto *col = new QVBoxLayout(&dlg);
    col->setContentsMargins(24, 20, 24, 20);
    col->setSpacing(14);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(14);

    auto *mode = new QComboBox(&dlg);
    mode->addItems({ QObject::tr("Follow system"), QObject::tr("Light"), QObject::tr("Dark") });
    mode->setCurrentIndex(int(theme.mode()));
    QObject::connect(mode, &QComboBox::activated, &dlg, [](int i) { ThemeManager::instance().setMode(static_cast<ThemeManager::Mode>(i)); });
    form->addRow(QObject::tr("Theme"), mode);

    auto *motion = new ToggleSwitch(QObject::tr("Reduce motion"), &dlg);
    motion->setChecked(theme.reducedMotion(), false);
    QObject::connect(motion, &ToggleSwitch::toggled, &dlg, [](bool on) { ThemeManager::instance().setReducedMotion(on); });
    form->addRow(QObject::tr("Reduce animations"), motion);

    auto *viewFirst = new ToggleSwitch(QObject::tr("Open files in view mode first"), &dlg);
    viewFirst->setChecked(QSettings().value(QStringLiteral("files/openInViewMode"), true).toBool(), false);
    QObject::connect(viewFirst, &ToggleSwitch::toggled, &dlg, [](bool on) { QSettings().setValue(QStringLiteral("files/openInViewMode"), on); });
    form->addRow(QObject::tr("Open files in view mode"), viewFirst);
    col->addLayout(form);

    auto *row = new QHBoxLayout;
    auto *clear = new PillButton(QObject::tr("Clear recent files"), QStringLiteral("trash"), false, &dlg);
    auto *keys = new PillButton(QObject::tr("Keyboard shortcuts"), QStringLiteral("keyboard"), false, &dlg);
    row->addWidget(clear);
    row->addWidget(keys);
    row->addStretch(1);
    col->addLayout(row);
    QObject::connect(clear, &PillButton::clicked, &dlg, [parent] { FileManager::instance().clearRecent(); Toast::show(parent, QObject::tr("Recent files cleared."), Toast::Success); });
    QObject::connect(keys, &PillButton::clicked, &dlg, [&dlg] { showShortcuts(&dlg); });

    auto *close = new PillButton(QObject::tr("Done"), QString(), true, &dlg);
    col->addWidget(close, 0, Qt::AlignRight);
    QObject::connect(close, &PillButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

} // namespace AppDialogs
