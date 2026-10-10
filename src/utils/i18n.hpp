#pragma once

#include <QLocale>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QLabel>

QList<QLocale> getAvailableLocales();

void setLanguage(QLocale locale);

void setupReTLHandler();

template <typename T>
concept HasSetTextWidget = std::derived_from<T, QWidget> && requires(T widget, const QString& text) {
    widget.setText(text);
};

void addWidgetToRetranslateList_(QWidget* widget);
template<HasSetTextWidget T>
void addWidgetToRetranslateList(T* widget) {addWidgetToRetranslateList_(widget);};
void addRetranslateCallback(std::function<void()> callback, bool immediate = true);
void removeRetranslateCallback(std::function<void()> callback);
void retranslateWidget(QWidget* widgetPtr);

/// Alternative syntax to QT_TR_NOOP due to the custom lupdate wrapper resolving the macro.
inline const char* trNoop(const char* text) {return text;}
/// Alternative syntax to QT_TRANSLATE_NOOP due to the custom lupdate wrapper resolving the macro.
inline const char* translateNoop(const char* context, const char* text) {return text;}

#define $setLabelTextForAutoTL(labelPtr, text, ...) \
  /*labelPtr->setText(tr(text)__VA_OPT__(.arg(__VA_ARGS__)));*/ \
  labelPtr->setProperty("mvcTLText", tr(text)); \
  labelPtr->setProperty("mvcTLContext", QString(metaObject()->className()));\
  $changeLabelPlaceholderArgs(labelPtr, __VA_ARGS__);

#define $changeLabelPlaceholderArgs(labelPtr, ...) \
  labelPtr->setProperty("mvcTLPlaceholderArgs", QList<QVariant>({__VA_ARGS__})); \
  retranslateWidget(labelPtr);

#define $setLabelTextAndRegisterAutoTL(labelPtr, text, ...) \
  $setLabelTextForAutoTL(labelPtr, text, __VA_ARGS__); \
  addWidgetToRetranslateList(labelPtr);
#define $setDynamicLabelTextAndRegisterAutoTL(labelPtr, text, ...) \
  labelPtr->setProperty("mvcUsesDynamicString", true);\
  $setLabelTextAndRegisterAutoTL(labelPtr, text, __VA_ARGS__);

#define $createAutoTLLabelInline(text, ...) ({\
  auto *label = new QLabel(); \
  $setLabelTextAndRegisterAutoTL(label, text, __VA_ARGS__); \
  label; \
})
