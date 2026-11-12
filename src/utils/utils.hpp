#pragma once
#include <QLayout>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QFuture>
#include <QPromise>
#include <nlohmann/json.hpp>

int findChildWidgetIndex(QLayout* layout, QWidget* child); 
QWidget* findChildWidgetBy(QLayout* layout, std::function<bool(QWidget*)> predicate); 

inline void copyIcon(const QIcon* source, QIcon* dest) {
  QIcon temp(*source);
  dest->swap(temp);
}

bool askForBasePathChange();
bool askForProfilePictureChange();
std::string generate_uuid_v4();


bool createDesktopShortcut(const QString &shortcutName,
                           const QString &launchArgument,
                           const QString &iconPath);
QNetworkAccessManager* createNetworkAccessManager(QObject *context);
#include <chrono>
template <typename Rep, typename Period>
std::string format_duration(std::chrono::duration<Rep, Period> d) {
    using namespace std::chrono;

    auto months_count  = floor<months>(d);  d -= months_count;
    auto weeks_count   = floor<weeks>(d);   d -= weeks_count;
    auto days_count    = floor<days>(d);    d -= days_count;
    auto hours_count   = floor<hours>(d);   d -= hours_count;
    auto minutes_count = floor<minutes>(d); d -= minutes_count;
    auto seconds_count = floor<seconds>(d);

    std::string result;

    if (months_count.count() > 0 || !result.empty()) {
        result += std::to_string(months_count.count()) + "mo ";
    }
    if (weeks_count.count() > 0 || !result.empty()) {
        result += std::to_string(weeks_count.count()) + "w ";
    }
    if (days_count.count() > 0 || !result.empty()) {
        result += std::to_string(days_count.count()) + "d ";
    }
    if (hours_count.count() > 0 || !result.empty()) {
        result += std::to_string(hours_count.count()) + "h ";
    }
    if (minutes_count.count() > 0 || !result.empty()) {
        result += std::to_string(minutes_count.count()) + "m ";
    }
    result += std::to_string(seconds_count.count()) + "s";

    return result;
}

class network_error : public QException {
public:
  network_error(QNetworkReply::NetworkError error, QString url);

  const char* what() const noexcept override;

private:
  QNetworkReply::NetworkError error_;
  QString url_;

  std::string msg;
};


QFuture<QByteArray> fetchUrlAsync(QNetworkAccessManager* manager, const QUrl& url);
QFuture<nlohmann::json> fetchJsonAsync(QNetworkAccessManager* manager, const QUrl& url);
