#include "i18n.hpp"
#include <QTranslator>
#include <QApplication>
#include <QList>
#include <QDirIterator>
#include <qobject.h>
#include <QDebug>

void setLanguage(QLocale locale) {
    qDebug() << "i18n: setting language" << locale.name();
    static std::unique_ptr<QTranslator> translator = std::make_unique<QTranslator>();

    if (translator->load(locale, qApp->applicationName(), "_", ":/translations")) {
        qDebug() << "i18n: installed translation" << locale.name();
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

std::vector<std::function<void()>> g_retranslateCallbacks;
void addRetranslateCallback(std::function<void()> callback, bool immediate) {
  g_retranslateCallbacks.push_back(callback);
  if (immediate) callback();
};


void addRetranslateCallback(QObject* context, std::function<void()> callback, bool immediate) {
  addRetranslateCallback(std::move(callback), immediate);
  QObject::connect(context, &QObject::destroyed, [callback]() {
    removeRetranslateCallback(callback);
  });
}

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

class RetranslateHandler : public QObject {
public:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if (obj == qApp && event->type() == QEvent::LanguageChange) {
      for (auto& callback : g_retranslateCallbacks) {
        callback();
      }
    }
    return QObject::eventFilter(obj, event);
  }
};

void setupReTLHandler() {
  static RetranslateHandler* handler = new RetranslateHandler();
  qApp->installEventFilter(handler);
}
