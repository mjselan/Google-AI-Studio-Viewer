#pragma once

#include <QString>
#include <QByteArray>

struct FileAttachment
{
    QString id;
    QString name;
    QString mimeType;
    QByteArray content;
};
