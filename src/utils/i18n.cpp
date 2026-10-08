#include "i18n.hpp"
#include <QTranslator>
#include <QApplication>
#include <QPointer>
#include <QList>
#include <QLabel>
#include <QDirIterator>
#include <QPushButton>
#include <qobject.h>

void setLanguage(QLocale locale) {
    static std::unique_ptr<QTranslator> translator = std::make_unique<QTranslator>();

    if (translator->load(locale, qApp->applicationName(), "_", ":/translations")) {
        qApp->installTranslator(translator.get());
    } else {
        qDebug() << "Failed to load translation for locale:" << locale;
    }
}

QList<QLocale> getAvailableLocales() {
  QList<QLocale> availableLocales;

  // Direct path to application directory + /translations
  QDir transDir(":/translations");
  QStringList qmFiles =
      transDir.entryList(QStringList() << "*.qm", QDir::Files);

  QRegularExpression regex(QStringLiteral("VirtualClub_([a-zA-Z_]+)\\.qm$"));

  for (const QString &fileName : qmFiles) {
    QRegularExpressionMatch match = regex.match(fileName);
    if (match.hasMatch()) {
      availableLocales.append(QLocale(match.captured(1)));
    }
  }

  return availableLocales;
}

// RetranslateHandler but run if any of the autotl-specific dynamic property on a widget changed
class IndividualRetranslateHandler : public QObject {
  QWidget* m_targetWidget;
public:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if (event->type() == QEvent::DynamicPropertyChange) {
      auto* propEvent = static_cast<QDynamicPropertyChangeEvent*>(event);
      if (propEvent->propertyName() == "mvcTLText" || propEvent->propertyName() == "mvcTLPlaceholderArgs" || propEvent->propertyName() == "mvcTLContext") {
        auto* widget = qobject_cast<QWidget*>(obj);
        if (widget == m_targetWidget) {
          retranslateWidget(widget);
        }
        return true;
      }
    }
    return QObject::eventFilter(obj, event);
  }
  IndividualRetranslateHandler(QWidget* targetWidget) : m_targetWidget(targetWidget) {
  }
};

QList<QPointer<QWidget>> g_retranslatingWidgets;
std::vector<std::function<void()>> g_retranslateCallbacks;
void addWidgetToRetranslateList_(QWidget* widget) {
  g_retranslatingWidgets.append(QPointer(widget));
  if (widget->property("mvcUsesDynamicString").toBool()) widget->installEventFilter(new IndividualRetranslateHandler(widget));
}
void addRetranslateCallback(std::function<void()> callback, bool immediate) {
  g_retranslateCallbacks.push_back(callback);
  if (immediate) callback();
};

void removeRetranslateCallback(std::function<void()> callback) {
  // why
  auto it = std::remove_if(g_retranslateCallbacks.begin(), g_retranslateCallbacks.end(),
                           [&callback](const std::function<void()>& storedCallback) {
                             // Compare the target of the std::function objects
                             return storedCallback.target_type() == callback.target_type() &&
                                    storedCallback.target<void()>() == callback.target<void()>();
                           });
  g_retranslateCallbacks.erase(it, g_retranslateCallbacks.end());
}

void retranslateWidget(QWidget* widgetPtr) {
  if (!widgetPtr) return;
  QString text = widgetPtr->property("mvcTLText").toString();
  QVariant placeholderArgsVar = widgetPtr->property("mvcTLPlaceholderArgs");
  QString context = widgetPtr->property("mvcTLContext").toString();
  if (!text.isEmpty()) {
    QString translatedText = qApp->translate(context.toStdString().c_str(), text.toUtf8().constData());
    if (placeholderArgsVar.isValid() && placeholderArgsVar.canConvert<QList<QVariant>>()) {
      QList<QVariant> placeholderArgs = placeholderArgsVar.value<QList<QVariant>>();
      for (const auto& arg : placeholderArgs) {
        translatedText = translatedText.arg(arg.toString());
      }
    }
    #define $translateTextLikeIfType(type) \
    if (auto widgetPtrCasted = qobject_cast<type*>(widgetPtr)) \
      widgetPtrCasted->setText(translatedText);
    
    $translateTextLikeIfType(QLabel);
    $translateTextLikeIfType(QPushButton);

    #undef $translateTextLikeIfType
  }
}

class RetranslateHandler : public QObject {
public:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if (obj == qApp && event->type() == QEvent::LanguageChange) {
      for (auto& callback : g_retranslateCallbacks) {
        callback();
      }
      for (auto widgetPtr : g_retranslatingWidgets) {
        retranslateWidget(widgetPtr.data()); 
      }
    }
    return QObject::eventFilter(obj, event);
  }
};

void setupReTLHandler() {
  static RetranslateHandler* handler = new RetranslateHandler();
  qApp->installEventFilter(handler);
}
