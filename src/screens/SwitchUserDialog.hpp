#pragma once
#include <QPushButton>
#include <QLabel>
#include <QPropertyAnimation>
#include "ui/PixmapWidget.hpp"
#include "ui/MESWidgets.hpp"

namespace detail {
class UserListItem : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(int hoverAlpha READ hoverAlpha WRITE setHoverAlpha)

signals:
    void selectedChanged();
    void deleteProfileRequest();

private:    
    int m_hoverAnimationValue = 0; // Ranges from 0 to 255
    QPropertyAnimation *m_fadeAnimation = nullptr;

    PixmapWidget *m_iconWidget = nullptr;
    QLabel *m_labelWidget = nullptr;
    QPushButton *m_deleteButton = nullptr;

    bool m_selected = false;
    bool m_hovered = false;
    bool m_switchable = true;

    bool m_showDeleteButton = true;

    int hoverAlpha() const;
    void setHoverAlpha(int alpha);

    void updateStyleSheet();

  protected:
    void enterEvent(QEnterEvent *event) override;

    void leaveEvent(QEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

  public:
    explicit UserListItem(QIcon icon, std::string label, bool switchable,
                          QWidget *parent = nullptr);

    void showDeleteButton(bool show);

    bool switchable() const;
    bool selected() const;

    void setSelected(bool selected);

    std::string label() const;
};

class SwitchUserDialogContent : public DialogContent {
  Q_OBJECT
public:
  void startNewWithProfile(const QString &profileId);

  SwitchUserDialogContent();

  void onNewProfileButton();
};
}

void showSwitchUserDialog();
