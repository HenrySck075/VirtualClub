#pragma once

#include <QLocale>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QLabel>

QList<QLocale> getAvailableLocales();

void setLanguage(QLocale locale);

void setupReTLHandler();

// Add a callback when the application language changed.
//
// This is mainly used to update text UI widgets, but it should only do so for widgets that, 
// in practice, exists when a language change in SettingsScreen was made.
void addRetranslateCallback(std::function<void()> callback, bool immediate = true);
void addRetranslateCallback(QObject* context, std::function<void()> callback, bool immediate = true);
void removeRetranslateCallback(std::function<void()> callback);

