#pragma once
#include <QAbstractTableModel>
#include <QFont>
#include <QUndoStack>
#include <QVector>
#include <functional>

#include "sheet.h"

// Table model over a sparse Sheet. All edits go through the undo stack.
class SheetModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    struct Edit { int r, c; Cell before, after; };

    SheetModel(Sheet *sheet, QUndoStack *undo, QObject *parent = nullptr);
    Sheet *sheet() const { return m_sheet; }
    void setReadOnly(bool ro) { m_readOnly = ro; }
    bool isReadOnly() const { return m_readOnly; }
    void setBaseFont(const QFont &f) { m_font = f; refreshAll(); }

    int rowCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : Sheet::kMaxRows; }
    int columnCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : Sheet::kMaxCols; }
    QVariant data(const QModelIndex &idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &idx) const override;
    bool setData(const QModelIndex &idx, const QVariant &value, int role) override;

    void pushEdits(const QVector<Edit> &edits, const QString &text);
    void applyEdits(const QVector<Edit> &edits, bool useAfter);
    void changeSnapshot(const QString &text, const std::function<void(Sheet &)> &mutate);
    void restore(const QHash<quint64, Cell> &cells);
    void refreshAll();

private:
    Sheet *m_sheet;
    QUndoStack *m_undo;
    QFont m_font;
    bool m_readOnly = false;
};
