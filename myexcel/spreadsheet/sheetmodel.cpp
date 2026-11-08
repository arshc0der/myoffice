#include "sheetmodel.h"

#include <QBrush>
#include <QGuiApplication>

namespace {
class CellsCommand : public QUndoCommand
{
public:
    CellsCommand(SheetModel *m, QVector<SheetModel::Edit> e, const QString &text) : m_model(m), m_edits(std::move(e)) { setText(text); }
    void redo() override { m_model->applyEdits(m_edits, true); }
    void undo() override { m_model->applyEdits(m_edits, false); }
private:
    SheetModel *m_model;
    QVector<SheetModel::Edit> m_edits;
};

class SnapshotCommand : public QUndoCommand
{
public:
    SnapshotCommand(SheetModel *m, QHash<quint64, Cell> before, QHash<quint64, Cell> after, const QString &text)
        : m_model(m), m_before(std::move(before)), m_after(std::move(after)) { setText(text); }
    void redo() override { if (m_first) { m_first = false; return; } m_model->restore(m_after); } // already applied once
    void undo() override { m_model->restore(m_before); }
private:
    SheetModel *m_model;
    QHash<quint64, Cell> m_before, m_after;
    bool m_first = true;
};
}

SheetModel::SheetModel(Sheet *sheet, QUndoStack *undo, QObject *parent)
    : QAbstractTableModel(parent), m_sheet(sheet), m_undo(undo), m_font(QGuiApplication::font()) {}

QVariant SheetModel::data(const QModelIndex &idx, int role) const
{
    const int r = idx.row(), c = idx.column();
    const Cell *cell = m_sheet->find(r, c);
    switch (role) {
    case Qt::DisplayRole: return cell ? m_sheet->display(r, c) : QVariant();
    case Qt::EditRole: return cell ? cell->input : QString();
    case Qt::ToolTipRole: { if (!cell) return {}; const CellValue v = m_sheet->value(r, c); return v.isError() ? QVariant(v.text) : QVariant(); }
    case Qt::TextAlignmentRole: {
        int a = cell ? cell->style.align : 0;
        if (a == 1) return int(Qt::AlignLeft | Qt::AlignVCenter);
        if (a == 2) return int(Qt::AlignHCenter | Qt::AlignVCenter);
        if (a == 3) return int(Qt::AlignRight | Qt::AlignVCenter);
        if (!cell || cell->input.isEmpty()) return int(Qt::AlignLeft | Qt::AlignVCenter);
        const CellValue v = m_sheet->value(r, c);
        if (v.type == CellValue::Number) return int(Qt::AlignRight | Qt::AlignVCenter);
        if (v.type == CellValue::Bool || v.type == CellValue::Error) return int(Qt::AlignHCenter | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    case Qt::FontRole:
        if (cell && (cell->style.bold || cell->style.italic || cell->style.underline)) {
            QFont f = m_font;
            f.setBold(cell->style.bold); f.setItalic(cell->style.italic); f.setUnderline(cell->style.underline);
            return f;
        }
        return {};
    case Qt::ForegroundRole:
        if (cell && cell->style.fg.isValid()) return QBrush(cell->style.fg);
        if (cell && m_sheet->value(r, c).isError()) return QBrush(QColor("#C0392B"));
        return {};
    case Qt::BackgroundRole: return (cell && cell->style.bg.isValid()) ? QVariant(QBrush(cell->style.bg)) : QVariant();
    default: return {};
    }
}

QVariant SheetModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role == Qt::DisplayRole) return o == Qt::Horizontal ? Formula::colName(section) : QString::number(section + 1);
    if (role == Qt::TextAlignmentRole) return int(Qt::AlignCenter);
    return {};
}

Qt::ItemFlags SheetModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | (m_readOnly ? Qt::ItemFlags() : Qt::ItemIsEditable);
}

bool SheetModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (role != Qt::EditRole || m_readOnly) return false;
    const Cell before = m_sheet->cell(idx.row(), idx.column());
    Cell after = before;
    after.input = value.toString();
    if (after.input == before.input) return false;
    autoFormat(after);
    pushEdits({ { idx.row(), idx.column(), before, after } }, tr("Edit cell"));
    return true;
}

void SheetModel::pushEdits(const QVector<Edit> &edits, const QString &text)
{
    if (edits.isEmpty()) return;
    if (m_undo) m_undo->push(new CellsCommand(this, edits, text)); // push() runs redo()
    else applyEdits(edits, true);
}

void SheetModel::applyEdits(const QVector<Edit> &edits, bool useAfter)
{
    for (const Edit &e : edits) m_sheet->setCell(e.r, e.c, useAfter ? e.after : e.before);
    refreshAll(); // formulas anywhere may depend on the change; views only repaint what is visible
}

void SheetModel::changeSnapshot(const QString &text, const std::function<void(Sheet &)> &mutate)
{
    const QHash<quint64, Cell> before = m_sheet->cells();
    mutate(*m_sheet);
    const QHash<quint64, Cell> after = m_sheet->cells();
    refreshAll();
    if (m_undo) m_undo->push(new SnapshotCommand(this, before, after, text));
}

void SheetModel::restore(const QHash<quint64, Cell> &cells) { m_sheet->replaceAll(cells); refreshAll(); }

void SheetModel::refreshAll()
{
    emit dataChanged(index(0, 0), index(Sheet::kMaxRows - 1, Sheet::kMaxCols - 1));
}
