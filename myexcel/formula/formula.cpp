#include "formula.h"

#include <QDate>
#include <QDateTime>
#include <cmath>
#include <memory>

// ------------------------------------------------------------------ CellValue
bool CellValue::toNumber(double *out) const
{
    switch (type) {
    case Empty: *out = 0; return true;
    case Number: case Bool: *out = num; return true;
    case Text: { bool ok = false; const double d = text.trimmed().toDouble(&ok); if (ok) *out = d; return ok; }
    default: return false;
    }
}
QString CellValue::toText() const
{
    switch (type) {
    case Number: return Formula::formatGeneral(num);
    case Bool: return num != 0 ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    case Text: case Error: return text;
    default: return QString();
    }
}
bool CellValue::toBool(bool *ok) const
{
    *ok = true;
    if (type == Bool || type == Number) return num != 0;
    if (type == Empty) return false;
    if (type == Text) { if (text.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0) return true; if (text.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0) return false; }
    *ok = false;
    return false;
}

// ------------------------------------------------------------------ helpers
namespace Formula {
QString colName(int col)
{
    QString s;
    for (int c = col + 1; c > 0; c = (c - 1) / 26) s.prepend(QChar('A' + (c - 1) % 26));
    return s;
}
QString refName(int row, int col) { return colName(col) + QString::number(row + 1); }
QString formatGeneral(double n)
{
    if (std::isnan(n) || std::isinf(n)) return QStringLiteral("#NUM!");
    return QString::number(n, 'g', 12);
}
bool parseRef(const QString &in, int *row, int *col)
{
    QString s = in;
    s.remove(QLatin1Char('$'));
    int i = 0, c = 0;
    while (i < s.size() && s[i].isLetter() && s[i].unicode() < 128) { c = c * 26 + (s[i].toUpper().unicode() - 'A' + 1); ++i; }
    if (i == 0 || i > 3 || i >= s.size()) return false;
    bool ok = false;
    const int r = s.mid(i).toInt(&ok);
    if (!ok || r < 1 || r > 1048576 || c < 1 || c > 16384) return false;
    for (int k = i; k < s.size(); ++k) if (!s[k].isDigit()) return false;
    *row = r - 1;
    *col = c - 1;
    return true;
}

namespace {
// Rewrites each cell reference outside string literals with `fn`.
template <typename Fn>
QString rewriteRefs(const QString &f, Fn fn)
{
    QString out;
    bool inQuote = false;
    const int n = f.size();
    int i = 0;
    while (i < n) {
        const QChar ch = f[i];
        if (ch == QLatin1Char('"')) { inQuote = !inQuote; out += ch; ++i; continue; }
        const bool boundary = i == 0 || !(f[i - 1].isLetterOrNumber() || f[i - 1] == QLatin1Char('_') || f[i - 1] == QLatin1Char('.'));
        if (!inQuote && boundary && (ch.isLetter() || ch == QLatin1Char('$')) && ch.unicode() < 128) {
            int j = i;
            const bool absCol = f[j] == QLatin1Char('$');
            if (absCol) ++j;
            const int ls = j;
            while (j < n && f[j].isLetter() && f[j].unicode() < 128 && j - ls < 3) ++j;
            const int le = j;
            bool absRow = false;
            if (le > ls && j < n && f[j] == QLatin1Char('$')) { absRow = true; ++j; }
            const int ds = j;
            while (j < n && f[j].isDigit()) ++j;
            const bool valid = le > ls && j > ds && !(j < n && (f[j].isLetterOrNumber() || f[j] == QLatin1Char('_') || f[j] == QLatin1Char('('))) ;
            if (valid) {
                int row, col;
                if (parseRef(f.mid(i, j - i), &row, &col)) { out += fn(row, col, absRow, absCol); i = j; continue; }
            }
        }
        out += ch;
        ++i;
    }
    return out;
}
}

QString shiftReferences(const QString &formula, bool isRow, int at, int delta)
{
    return rewriteRefs(formula, [&](int row, int col, bool absRow, bool absCol) -> QString {
        int &idx = isRow ? row : col;
        if (delta > 0) { if (idx >= at) idx += delta; }
        else if (idx >= at && idx < at - delta) return QStringLiteral("#REF!");
        else if (idx >= at - delta) idx += delta;
        if (row < 0 || row >= 1048576 || col < 0 || col >= 16384) return QStringLiteral("#REF!");
        return QString(absCol ? "$" : "") + colName(col) + (absRow ? "$" : "") + QString::number(row + 1);
    });
}

QString translateReferences(const QString &formula, int dRow, int dCol)
{
    return rewriteRefs(formula, [&](int row, int col, bool absRow, bool absCol) -> QString {
        if (!absRow) row += dRow;
        if (!absCol) col += dCol;
        if (row < 0 || row >= 1048576 || col < 0 || col >= 16384) return QStringLiteral("#REF!");
        return QString(absCol ? "$" : "") + colName(col) + (absRow ? "$" : "") + QString::number(row + 1);
    });
}

// ------------------------------------------------------------------ tokenizer
namespace {
struct Tok {
    enum T { Num, Str, Ident, Op, LP, RP, Comma, Colon, End } t = End;
    QString s;
    double n = 0;
};

bool tokenize(const QString &e, std::vector<Tok> &out)
{
    int i = 0;
    const int n = e.size();
    while (i < n) {
        const QChar c = e[i];
        if (c.isSpace()) { ++i; continue; }
        Tok t;
        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && e[i + 1].isDigit())) {
            int j = i;
            while (j < n && (e[j].isDigit() || e[j] == QLatin1Char('.'))) ++j;
            if (j < n && (e[j] == QLatin1Char('e') || e[j] == QLatin1Char('E')) && j + 1 < n && (e[j + 1].isDigit() || e[j + 1] == QLatin1Char('-') || e[j + 1] == QLatin1Char('+'))) { j += 2; while (j < n && e[j].isDigit()) ++j; }
            bool ok = false;
            t.t = Tok::Num; t.n = e.mid(i, j - i).toDouble(&ok);
            if (!ok) return false;
            i = j;
        } else if (c == QLatin1Char('"')) {
            int j = i + 1;
            QString s;
            bool closed = false;
            while (j < n) {
                if (e[j] == QLatin1Char('"')) { if (j + 1 < n && e[j + 1] == QLatin1Char('"')) { s += QLatin1Char('"'); j += 2; continue; } closed = true; ++j; break; }
                s += e[j++];
            }
            if (!closed) return false;
            t.t = Tok::Str; t.s = s; i = j;
        } else if (c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$')) {
            int j = i;
            while (j < n && (e[j].isLetterOrNumber() || e[j] == QLatin1Char('_') || e[j] == QLatin1Char('$') || e[j] == QLatin1Char('.'))) ++j;
            t.t = Tok::Ident; t.s = e.mid(i, j - i); i = j;
        } else if (c == QLatin1Char('(')) { t.t = Tok::LP; ++i; }
        else if (c == QLatin1Char(')')) { t.t = Tok::RP; ++i; }
        else if (c == QLatin1Char(',') || c == QLatin1Char(';')) { t.t = Tok::Comma; ++i; }
        else if (c == QLatin1Char(':')) { t.t = Tok::Colon; ++i; }
        else if (QString("+-*/^&=%<>").contains(c)) {
            t.t = Tok::Op; t.s = QString(c); ++i;
            if ((c == QLatin1Char('<') || c == QLatin1Char('>')) && i < n && (e[i] == QLatin1Char('=') || (c == QLatin1Char('<') && e[i] == QLatin1Char('>')))) { t.s += e[i]; ++i; }
        } else return false;
        out.push_back(t);
    }
    out.push_back(Tok());
    return true;
}

// ------------------------------------------------------------------ parser
struct Node;
using NP = std::unique_ptr<Node>;
struct Node {
    enum K { Num, Str, Bool, Ref, Range, Neg, Pct, Bin, Func, Err } k = Err;
    double n = 0;
    QString s;
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    std::vector<NP> a;
};

class Parser
{
public:
    explicit Parser(std::vector<Tok> toks) : t(std::move(toks)) {}
    NP parse()
    {
        NP n = cmp();
        if (cur().t != Tok::End) return err(QStringLiteral("#NAME?"));
        return n;
    }

private:
    std::vector<Tok> t;
    size_t i = 0;
    int depth = 0;
    const Tok &cur() const { return t[i]; }
    static NP mk(Node::K k) { auto n = std::make_unique<Node>(); n->k = k; return n; }
    static NP err(const QString &e) { auto n = mk(Node::Err); n->s = e; return n; }
    bool isOp(const char *o) const { return cur().t == Tok::Op && cur().s == QLatin1String(o); }
    static NP bin(const QString &op, NP l, NP r) { auto n = mk(Node::Bin); n->s = op; n->a.push_back(std::move(l)); n->a.push_back(std::move(r)); return n; }

    NP cmp()
    {
        if (++depth > 200) return err(QStringLiteral("#NAME?"));
        NP l = cat();
        while (cur().t == Tok::Op && (cur().s == "=" || cur().s == "<>" || cur().s == "<" || cur().s == ">" || cur().s == "<=" || cur().s == ">=")) {
            const QString op = cur().s; ++i; l = bin(op, std::move(l), cat());
        }
        --depth;
        return l;
    }
    NP cat() { NP l = add(); while (isOp("&")) { ++i; l = bin(QStringLiteral("&"), std::move(l), add()); } return l; }
    NP add() { NP l = term(); while (isOp("+") || isOp("-")) { const QString op = cur().s; ++i; l = bin(op, std::move(l), term()); } return l; }
    NP term() { NP l = power(); while (isOp("*") || isOp("/")) { const QString op = cur().s; ++i; l = bin(op, std::move(l), power()); } return l; }
    NP power() { NP l = unary(); while (isOp("^")) { ++i; l = bin(QStringLiteral("^"), std::move(l), unary()); } return l; }
    NP unary()
    {
        if (isOp("-")) { ++i; auto n = mk(Node::Neg); n->a.push_back(unary()); return n; }
        if (isOp("+")) { ++i; return unary(); }
        NP n = primary();
        while (isOp("%")) { ++i; auto p = mk(Node::Pct); p->a.push_back(std::move(n)); n = std::move(p); }
        return n;
    }
    NP primary()
    {
        const Tok tk = cur();
        switch (tk.t) {
        case Tok::Num: { ++i; auto n = mk(Node::Num); n->n = tk.n; return n; }
        case Tok::Str: { ++i; auto n = mk(Node::Str); n->s = tk.s; return n; }
        case Tok::LP: { ++i; NP e = cmp(); if (cur().t != Tok::RP) return err(QStringLiteral("#NAME?")); ++i; return e; }
        case Tok::Ident: {
            ++i;
            if (cur().t == Tok::LP) {
                ++i;
                auto f = mk(Node::Func);
                f->s = tk.s.toUpper();
                if (cur().t != Tok::RP) { while (true) { f->a.push_back(cmp()); if (cur().t == Tok::Comma) { ++i; continue; } break; } }
                if (cur().t != Tok::RP) return err(QStringLiteral("#NAME?"));
                ++i;
                return f;
            }
            const QString u = tk.s.toUpper();
            if (u == QLatin1String("TRUE") || u == QLatin1String("FALSE")) { auto n = mk(Node::Bool); n->n = u == QLatin1String("TRUE"); return n; }
            int r, c;
            if (!parseRef(tk.s, &r, &c)) return err(QStringLiteral("#NAME?"));
            if (cur().t == Tok::Colon) {
                ++i;
                int r2, c2;
                if (cur().t != Tok::Ident || !parseRef(cur().s, &r2, &c2)) return err(QStringLiteral("#REF!"));
                ++i;
                auto n = mk(Node::Range);
                n->r1 = qMin(r, r2); n->r2 = qMax(r, r2); n->c1 = qMin(c, c2); n->c2 = qMax(c, c2);
                return n;
            }
            auto n = mk(Node::Ref); n->r1 = r; n->c1 = c; return n;
        }
        default: return err(QStringLiteral("#NAME?"));
        }
    }
};

// ------------------------------------------------------------------ evaluator
using V = CellValue;

int compareValues(const V &a, const V &b)
{
    const bool an = a.type != V::Text, bn = b.type != V::Text;
    if (an && bn) { double x = 0, y = 0; a.toNumber(&x); b.toNumber(&y); return x < y ? -1 : (x > y ? 1 : 0); }
    if (!an && !bn) return QString::compare(a.text, b.text, Qt::CaseInsensitive);
    return an ? -1 : 1; // numbers sort before text
}

V eval(const Node &n, CellProvider &cp);

bool gather(const Node &f, CellProvider &cp, std::vector<V> &out, V *err)
{
    for (const NP &arg : f.a) {
        if (arg->k == Node::Range) cp.rangeValues(arg->r1, arg->c1, arg->r2, arg->c2, out);
        else { V v = eval(*arg, cp); if (v.isError()) { *err = v; return false; } out.push_back(v); }
    }
    for (const V &v : out) if (v.isError()) { *err = v; return false; }
    return true;
}

V callFunction(const Node &f, CellProvider &cp)
{
    const QString &name = f.s;
    const size_t argc = f.a.size();
    auto bad = [] { return V::error(QStringLiteral("#VALUE!")); };
    auto numArg = [&](size_t i, double *out, V *err) -> bool {
        V v = eval(*f.a[i], cp);
        if (v.isError()) { *err = v; return false; }
        if (!v.toNumber(out)) { *err = V::error(QStringLiteral("#VALUE!")); return false; }
        return true;
    };
    auto textArg = [&](size_t i, QString *out, V *err) -> bool {
        V v = eval(*f.a[i], cp);
        if (v.isError()) { *err = v; return false; }
        *out = v.toText();
        return true;
    };

    if (name == QLatin1String("IF")) {
        if (argc < 2 || argc > 3) return bad();
        V c = eval(*f.a[0], cp);
        if (c.isError()) return c;
        bool ok;
        const bool b = c.toBool(&ok);
        if (!ok) return bad();
        if (b) return eval(*f.a[1], cp);
        return argc == 3 ? eval(*f.a[2], cp) : V::boolean(false);
    }
    if (name == QLatin1String("TODAY")) return V::number(QDate(1899, 12, 30).daysTo(QDate::currentDate()), true);
    if (name == QLatin1String("NOW")) {
        const QDateTime now = QDateTime::currentDateTime();
        return V::number(QDate(1899, 12, 30).daysTo(now.date()) + now.time().msecsSinceStartOfDay() / 86400000.0, true);
    }

    V err;
    if (name == QLatin1String("SUM") || name == QLatin1String("AVERAGE") || name == QLatin1String("MIN") || name == QLatin1String("MAX")
        || name == QLatin1String("COUNT") || name == QLatin1String("COUNTA")) {
        std::vector<V> vals;
        if (!gather(f, cp, vals, &err)) return err;
        if (name == QLatin1String("COUNTA")) { int c = 0; for (const V &v : vals) if (v.type != V::Empty) ++c; return V::number(c); }
        double sum = 0, mn = 0, mx = 0;
        int cnt = 0;
        for (const V &v : vals) {
            if (v.type != V::Number) continue;
            if (cnt == 0) mn = mx = v.num; else { mn = qMin(mn, v.num); mx = qMax(mx, v.num); }
            sum += v.num; ++cnt;
        }
        if (name == QLatin1String("COUNT")) return V::number(cnt);
        if (name == QLatin1String("SUM")) return V::number(sum);
        if (name == QLatin1String("AVERAGE")) return cnt ? V::number(sum / cnt) : V::error(QStringLiteral("#DIV/0!"));
        return V::number(name == QLatin1String("MIN") ? mn : mx);
    }
    if (name == QLatin1String("AND") || name == QLatin1String("OR")) {
        std::vector<V> vals;
        if (!gather(f, cp, vals, &err)) return err;
        if (vals.empty()) return bad();
        bool all = true, any = false;
        for (const V &v : vals) { bool ok; const bool b = v.toBool(&ok); if (!ok) continue; all = all && b; any = any || b; }
        return V::boolean(name == QLatin1String("AND") ? all : any);
    }
    if (name == QLatin1String("NOT")) {
        if (argc != 1) return bad();
        V v = eval(*f.a[0], cp);
        if (v.isError()) return v;
        bool ok; const bool b = v.toBool(&ok);
        return ok ? V::boolean(!b) : bad();
    }
    if (name == QLatin1String("CONCAT") || name == QLatin1String("CONCATENATE")) {
        std::vector<V> vals;
        if (!gather(f, cp, vals, &err)) return err;
        QString s;
        for (const V &v : vals) s += v.toText();
        return V::str(s);
    }
    double x, y;
    QString s;
    if (name == QLatin1String("ROUND")) {
        if (argc != 2) return bad();
        if (!numArg(0, &x, &err) || !numArg(1, &y, &err)) return err;
        const double p = std::pow(10.0, std::trunc(y));
        return V::number(std::round(x * p) / p);
    }
    if (name == QLatin1String("ABS") || name == QLatin1String("SQRT") || name == QLatin1String("INT")) {
        if (argc != 1) return bad();
        if (!numArg(0, &x, &err)) return err;
        if (name == QLatin1String("ABS")) return V::number(std::fabs(x));
        if (name == QLatin1String("INT")) return V::number(std::floor(x));
        return x < 0 ? V::error(QStringLiteral("#NUM!")) : V::number(std::sqrt(x));
    }
    if (name == QLatin1String("POWER") || name == QLatin1String("MOD")) {
        if (argc != 2) return bad();
        if (!numArg(0, &x, &err) || !numArg(1, &y, &err)) return err;
        if (name == QLatin1String("MOD")) return y == 0 ? V::error(QStringLiteral("#DIV/0!")) : V::number(x - y * std::floor(x / y));
        const double r = std::pow(x, y);
        return std::isnan(r) || std::isinf(r) ? V::error(QStringLiteral("#NUM!")) : V::number(r);
    }
    if (name == QLatin1String("LEFT") || name == QLatin1String("RIGHT")) {
        if (argc < 1 || argc > 2) return bad();
        if (!textArg(0, &s, &err)) return err;
        double cnt = 1;
        if (argc == 2 && !numArg(1, &cnt, &err)) return err;
        if (cnt < 0) return bad();
        const int k = int(cnt);
        return V::str(name == QLatin1String("LEFT") ? s.left(k) : s.right(k));
    }
    if (name == QLatin1String("LEN") || name == QLatin1String("UPPER") || name == QLatin1String("LOWER") || name == QLatin1String("TRIM")) {
        if (argc != 1) return bad();
        if (!textArg(0, &s, &err)) return err;
        if (name == QLatin1String("LEN")) return V::number(s.size());
        if (name == QLatin1String("UPPER")) return V::str(s.toUpper());
        if (name == QLatin1String("LOWER")) return V::str(s.toLower());
        return V::str(s.simplified());
    }
    return V::error(QStringLiteral("#NAME?"));
}

V eval(const Node &n, CellProvider &cp)
{
    switch (n.k) {
    case Node::Num: return V::number(n.n);
    case Node::Str: return V::str(n.s);
    case Node::Bool: return V::boolean(n.n != 0);
    case Node::Err: return V::error(n.s);
    case Node::Ref: return cp.cellValue(n.r1, n.c1);
    case Node::Range: return V::error(QStringLiteral("#VALUE!"));
    case Node::Neg: { V v = eval(*n.a[0], cp); if (v.isError()) return v; double x; return v.toNumber(&x) ? V::number(-x) : V::error(QStringLiteral("#VALUE!")); }
    case Node::Pct: { V v = eval(*n.a[0], cp); if (v.isError()) return v; double x; return v.toNumber(&x) ? V::number(x / 100.0) : V::error(QStringLiteral("#VALUE!")); }
    case Node::Func: return callFunction(n, cp);
    case Node::Bin: {
        V a = eval(*n.a[0], cp);
        if (a.isError()) return a;
        V b = eval(*n.a[1], cp);
        if (b.isError()) return b;
        const QString &op = n.s;
        if (op == QLatin1String("&")) return V::str(a.toText() + b.toText());
        if (op == QLatin1String("=") || op == QLatin1String("<>") || op == QLatin1String("<") || op == QLatin1String(">") || op == QLatin1String("<=") || op == QLatin1String(">=")) {
            const int c = compareValues(a, b);
            return V::boolean(op == QLatin1String("=") ? c == 0 : op == QLatin1String("<>") ? c != 0 : op == QLatin1String("<") ? c < 0
                              : op == QLatin1String(">") ? c > 0 : op == QLatin1String("<=") ? c <= 0 : c >= 0);
        }
        double x, y;
        if (!a.toNumber(&x) || !b.toNumber(&y)) return V::error(QStringLiteral("#VALUE!"));
        if (op == QLatin1String("+")) return V::number(x + y, a.dateHint || b.dateHint);
        if (op == QLatin1String("-")) return V::number(x - y);
        if (op == QLatin1String("*")) return V::number(x * y);
        if (op == QLatin1String("/")) return y == 0 ? V::error(QStringLiteral("#DIV/0!")) : V::number(x / y);
        const double r = std::pow(x, y);
        return std::isnan(r) || std::isinf(r) ? V::error(QStringLiteral("#NUM!")) : V::number(r);
    }
    }
    return V::error(QStringLiteral("#VALUE!"));
}
} // namespace

CellValue evaluate(const QString &expr, CellProvider &provider)
{
    std::vector<Tok> toks;
    if (expr.trimmed().isEmpty() || !tokenize(expr, toks)) return CellValue::error(QStringLiteral("#NAME?"));
    Parser parser(std::move(toks));
    NP root = parser.parse();
    if (root->k == Node::Range) return CellValue::error(QStringLiteral("#VALUE!"));
    return eval(*root, provider);
}

} // namespace Formula
