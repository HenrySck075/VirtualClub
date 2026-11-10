#include "FlowLayout.hpp"
#include <QWidget>
//! [1]
FlowLayout::FlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing)
    : m_hSpace(hSpacing), m_vSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}
//! [1]

//! [2]
FlowLayout::~FlowLayout()
{
    QLayoutItem *item;
    while ((item = takeAt(0)))
        delete item;
}
//! [2]

//! [3]
void FlowLayout::addItem(QLayoutItem *item)
{
    itemList.append(item);
}
//! [3]

//! [4]
int FlowLayout::horizontalSpacing() const
{
    if (m_hSpace >= 0) {
        return m_hSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }
}

int FlowLayout::verticalSpacing() const
{
    if (m_vSpace >= 0) {
        return m_vSpace;
    } else {
        return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    }
}
//! [4]

//! [5]
int FlowLayout::count() const
{
    return itemList.size();
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return itemList.value(index);
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index >= 0 && index < itemList.size())
        return itemList.takeAt(index);
    return nullptr;
}
//! [5]

//! [6]
Qt::Orientations FlowLayout::expandingDirections() const
{
    return { };
}
//! [6]

//! [7]
bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    int height = doLayout(QRect(0, 0, width, 0), true);
    return height;
}
//! [7]

//! [8]
void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem *item : std::as_const(itemList)) {
        const QWidget *wid = item->widget();
        if (wid && wid->isHidden())
            continue;

        size = size.expandedTo(item->minimumSize());
    }

    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}
//! [8]

//! [9]
int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    struct LayoutItem {
        QLayoutItem *item;
        QSize size;
        int spacing;
    };

    struct LayoutLine {
        QList<LayoutItem> items;
        int spacingBefore = 0;
        int width = 0;
        int height = 0;
    };

    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    QList<LayoutLine> lines;

    for (QLayoutItem *item : std::as_const(itemList)) {
        const QWidget *wid = item->widget();
        if (wid && wid->isHidden())
            continue;

        int spaceX = horizontalSpacing();
        if (spaceX == -1)
            spaceX = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);
        int spaceY = verticalSpacing();
        if (spaceY == -1)
            spaceY = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);

        const QSize size = item->sizeHint();
        if (lines.isEmpty())
            lines.append(LayoutLine{});

        LayoutLine *line = &lines.last();
        const int gap = line->items.isEmpty() ? 0 : line->items.last().spacing;
        if (!line->items.isEmpty() && line->width + gap + size.width() > effectiveRect.width()) {
            lines.append(LayoutLine{});
            lines.last().spacingBefore = spaceY;
            line = &lines.last();
        }

        if (!line->items.isEmpty())
            line->width += line->items.last().spacing;
        line->items.append({item, size, spaceX});
        line->width += size.width();
        line->height = qMax(line->height, size.height());
    }

    int totalHeight = 0;
    for (const LayoutLine &line : std::as_const(lines))
        totalHeight += line.spacingBefore + line.height;

    int y = effectiveRect.y();
    if (!testOnly) {
        const int extraHeight = effectiveRect.height() - totalHeight;
        const Qt::Alignment verticalAlignment = alignment() & Qt::AlignVertical_Mask;
        if (verticalAlignment.testFlag(Qt::AlignVCenter))
            y += qMax(0, extraHeight / 2);
        else if (verticalAlignment.testFlag(Qt::AlignBottom))
            y += qMax(0, extraHeight);
    }

    for (const LayoutLine &line : std::as_const(lines)) {
        y += line.spacingBefore;
        int x = effectiveRect.x();
        if (!testOnly) {
            const int extraWidth = effectiveRect.width() - line.width;
            const Qt::Alignment horizontalAlignment = alignment() & Qt::AlignHorizontal_Mask;
            if (horizontalAlignment.testFlag(Qt::AlignHCenter))
                x += qMax(0, extraWidth / 2);
            else if (horizontalAlignment.testFlag(Qt::AlignRight))
                x += qMax(0, extraWidth);
        }

        for (const LayoutItem &layoutItem : line.items) {
            if (!testOnly)
                layoutItem.item->setGeometry(QRect(QPoint(x, y), layoutItem.size));
            x += layoutItem.size.width() + layoutItem.spacing;
        }
        y += line.height;
    }

    return totalHeight + top + bottom;
}
//! [11]
//! [12]
int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject *parent = this->parent();
    if (!parent) {
        return -1;
    } else if (parent->isWidgetType()) {
        QWidget *pw = static_cast<QWidget *>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    } else {
        return static_cast<QLayout *>(parent)->spacing();
    }
}
//! [12]
