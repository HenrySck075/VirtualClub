#pragma once

#include <QLocale>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QLabel>

QList<QLocale> getAvailableLocales();

void setLanguage(QLocale locale);

void setupReTLHandler();

void addRetranslateCallback(std::function<void()> callback, bool immediate = true);
void removeRetranslateCallback(std::function<void()> callback);

/// Alternative syntax to QT_TR_NOOP due to the custom lupdate wrapper resolving the macro.
inline const char* trNoop(const char* text) {return text;}
/// Alternative syntax to QT_TRANSLATE_NOOP due to the custom lupdate wrapper resolving the macro.
inline const char* translateNoop(const char* context, const char* text) {return text;}
