/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     rekols <rekols@foxmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kugouapi.h"
#include <QEventLoop>
#include <QTime>
#include <QDebug>

KugouAPI::KugouAPI(QObject *parent)
    : QObject(parent),
      m_manager(new QNetworkAccessManager(this))
{

}

KugouAPI::~KugouAPI()
{
}

void KugouAPI::search(const QString &keyword)
{
    QUrl url("http://mobilecdn.kugou.com/api/v3/search/song");
    QUrlQuery query;
    query.addQueryItem("format", "json");
    query.addQueryItem("keyword", keyword);
    query.addQueryItem("page", "1");
    query.addQueryItem("pagesize", "500");
    query.addQueryItem("showtype", "1");
    url.setQuery(query.toString(QUrl::FullyEncoded));

    QNetworkRequest request(url);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, &KugouAPI::handleSearchFinished);
}

void KugouAPI::handleSearchFinished()
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());

    if (reply->error()) {
        return;
    }

    QByteArray array = reply->readAll();
    QJsonDocument document = QJsonDocument::fromJson(array);
    QJsonObject object = document.object().value("data").toObject();
    QJsonArray info = object["info"].toArray();

    for (int i = 0; i < info.size(); ++i) {
        QJsonObject currentObject = info.at(i).toObject();
        MusicData *data = new MusicData;
        data->songName = currentObject.value("songname").toString();
        data->singerName = currentObject.value("singername").toString();
        data->songHash = currentObject.value("hash").toString();
        data->song320Hash = currentObject.value("320hash").toString();

        QUrl url(QString("http://m.kugou.com/app/i/getSongInfo.php?cmd=playInfo&hash=%1").arg(data->songHash));
        QNetworkRequest request(url);
        QNetworkReply *reply = m_manager->get(request);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray(reply->readAll()));
            QJsonObject object = doc.object();
            data->url = object.value("url").toString();

            data->imgUrl = object.value("album_img").toString();
            data->imgUrl = data->imgUrl.replace("{size}", "100");

            qint64 length = object.value("timeLength").toInt();
            QTime time(0, 0, 0);
            time = time.addSecs(length);
            data->timeLength = time.toString("mm:ss");

            emit searchFinished(data);
        }

        if (!data->song320Hash.isEmpty()) {
            url.setUrl(QString("http://m.kugou.com/app/i/getSongInfo.php?cmd=playInfo&hash=%1").arg(data->song320Hash));
            request.setUrl(url);
            QNetworkReply *reply = m_manager->get(request);
            QEventLoop loop;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(QByteArray(reply->readAll()));
                QJsonObject object = doc.object();
                data->url_320 = object.value("url").toString();
            }
        }
        reply->deleteLater();
    }
}
