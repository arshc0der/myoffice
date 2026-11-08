#pragma once
#include <QUndoStack>
#include <functional>
#include <memory>
#include <vector>

#include "myexcel/spreadsheet/sheetmodel.h"
#include "ui/workspace/workspace.h"

class QDialog;
class QLineEdit;
class QStackedWidget;
class QTabBar;
class QTableView;

// One MyExcel workbook: formula bar, name box, grid per sheet, sheet tabs.
class ExcelDocument : public DocumentView
{
    Q_OBJECT
public:
    struct State { bool canUndo = false, canRedo = false, hasSelection = false; CellStyle style; QString sheetName; };

    explicit ExcelDocument(QWidget *parent = nullptr);
    bool eventFilter(QObject *o, QEvent *e) override;
    ~ExcelDocument() override;

    QString displayName() const override;
    QString filePath() const override { return m_path; }
    bool isModified() const override { return !m_undo.isClean() || m_extraDirty; }
    bool save() override;
    bool saveAs() override;
    bool isViewMode() const override { return m_viewMode; }
    void setViewMode(bool on) override;
    void load(const QString &path, bool viewMode) override;
    void activated() override;
    bool exportTo(const QString &path);
    void exportDialog(const QString &suffix);

    State state() const;
    int zoom() const { return m_zoom; }
    void setZoom(int percent);
    bool gridlines() const { return m_gridlines; }
    void setGridlines(bool on);
    int sheetCount() const { return int(m_pages.size()); }

    void undo() { m_undo.undo(); }
    void redo() { m_undo.redo(); }
    void copy();
    void cut();
    void paste();
    void clearSelection();
    void styleSelection(const std::function<void(CellStyle &)> &fn, const QString &text);
    void fill(bool down);
    void autoSum();
    void sortSelection(bool ascending);
    void insertDelete(bool rows, bool insert);
    void addSheet();
    void deleteSheet();
    void renameSheet();
    void findReplace();
    void focusFormulaBar(const QString &prefill);

private:
    struct Page {
        QString name;
        std::unique_ptr<Sheet> sheet;
        SheetModel *model = nullptr;
        QTableView *view = nullptr;
    };
    Page *page() const;
    Page *addPage(const QString &name);
    QList<QRect> selectionRects() const; // x = column, y = row
    QRect clamp(QRect r) const;
    void onCurrentChanged();
    void commitFormula();
    void jumpTo(const QString &text);
    void styleViews();
    void applyViewMode();
    bool writeFile(const QString &path, QString *error);
    bool saveToPath(const QString &path);
    bool parseBytes(const QString &path, const QByteArray &data, QString *error);
    bool exportPdf(const QString &path, QString *error);
    void goToCell(int r, int c);

    QString m_path, m_untitled;
    bool m_viewMode = false, m_extraDirty = false, m_gridlines = true;
    int m_zoom = 100;
    QUndoStack m_undo;
    std::vector<std::unique_ptr<Page>> m_pages;
    QStackedWidget *m_stack;
    QTabBar *m_tabs;
    QLineEdit *m_nameBox, *m_formulaBar;
    QDialog *m_findDlg = nullptr;
};
