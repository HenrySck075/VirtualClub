#include "OnlineModsIndex.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFuture>
#include <QPromise>
#include <nlohmann/json.hpp>

QFuture<QByteArray> fetchUrlAsync(QNetworkAccessManager* manager, const QUrl& url)
{
    auto promise = std::make_shared<QPromise<QByteArray>>();
    QFuture<QByteArray> future = promise->future();

    QNetworkRequest request(url);
    QNetworkReply* reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [reply, promise]() {
        promise->start();
        if (reply->error() == QNetworkReply::NoError) {
            promise->addResult(reply->readAll());
        } else {
            promise->addResult(QByteArray()); 
        }
        promise->finish();
        
        reply->deleteLater();
    });

    return future;
}

QFuture<nlohmann::json> fetchJsonAsync(QNetworkAccessManager* manager, const QUrl& url)
{
    auto promise = std::make_shared<QPromise<nlohmann::json>>();
    QFuture<nlohmann::json> future = promise->future();

    QNetworkRequest request(url);
    QNetworkReply* reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [reply, promise]() {
        promise->start();
        if (reply->error() == QNetworkReply::NoError) {
            try {
                nlohmann::json jsonData = nlohmann::json::parse(reply->readAll().toStdString());
                promise->addResult(jsonData);
            } catch (const std::exception&) {
                promise->addResult(nlohmann::json()); 
            }
        } else {
            promise->addResult(nlohmann::json()); 
        }
        promise->finish();
        
        reply->deleteLater();
    });

    return future;
}

///////////////////////////////////

QFuture<std::vector<OnlineMod>> ModdingCentralIndex::getMods(OnlineIndex::QueryConfig config) {
  (void)(perPage); // page limiting is not implemented in ddmc.site
  auto* manager = new QNetworkAccessManager(this);
  auto resp = fetchJsonAsync()
}
