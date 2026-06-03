#include "Conversation.h"

int Conversation::totalTokenCount() const
{
    int total = 0;
    for (const auto &m : messages)
        total += m.tokenCount;
    return total;
}

QString Conversation::displayTitle() const
{
    if (!fileName.isEmpty())
        return fileName;

    for (const auto &m : messages)
    {
        if (!m.text.isEmpty())
        {
            QString preview = m.text.left(80);
            if (m.text.length() > 80)
                preview += "...";
            return preview;
        }
    }
    return "Untitled";
}

QDateTime Conversation::lastMessageTime() const
{
    if (messages.isEmpty())
        return QDateTime();
    return messages.last().createTime;
}
