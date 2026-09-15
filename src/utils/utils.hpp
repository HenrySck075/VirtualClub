#pragma once
#include <QLayout>
#include <QWidget>

int findChildWidgetIndex(QLayout* layout, QWidget* child); 
QWidget* findChildWidgetBy(QLayout* layout, std::function<bool(QWidget*)> predicate); 

inline void copyIcon(const QIcon* source, QIcon* dest) {
  QIcon temp(*source);
  dest->swap(temp);
}

bool askForBasePathChange();
