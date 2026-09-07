#pragma once
#include <QLayout>
#include <QWidget>

int findChildWidgetIndex(QLayout* layout, QWidget* child); 
QWidget* findChildWidgetBy(QLayout* layout, std::function<bool(QWidget*)> predicate); 

