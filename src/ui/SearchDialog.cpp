#include "SearchDialog.h"
#include <QVBoxLayout>
#include <QLabel>

SearchDialog::SearchDialog(QWidget *parent)
    : QDialog(parent)
    , m_searchInput(new QLineEdit(this))
    , m_resultsList(new QListWidget(this))
    , m_debounceTimer(new QTimer(this))
{
    setWindowTitle("Search Conversations");
    resize(500, 400);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Search across all conversations (Google Drive):", this));
    layout->addWidget(m_searchInput);
    layout->addWidget(m_resultsList);

    m_searchInput->setPlaceholderText("Type to search...");

    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(500);

    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchDialog::onSearchTextChanged);
    connect(m_debounceTimer, &QTimer::timeout, this, &SearchDialog::onDebounceTimeout);
    connect(m_resultsList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item)
    {
        if (!item)
            return;
        QString fileId = item->data(Qt::UserRole).toString();
        if (fileId.isEmpty())
            return;
        QDateTime modTime = item->data(Qt::UserRole + 1).toDateTime();
        emit resultClicked(fileId, item->text(), modTime);
    });
}

void SearchDialog::onSearchTextChanged(const QString &text)
{
    m_debounceTimer->start();

    if (text.trimmed().length() < 2)
    {
        m_debounceTimer->stop();
        m_resultsList->clear();
    }
}

void SearchDialog::onDebounceTimeout()
{
    QString text = m_searchInput->text().trimmed();
    if (text.length() < 2)
        return;

    m_resultsList->clear();
    m_resultsList->addItem("Searching...");
    emit searchRequested(text);
}

void SearchDialog::setResults(const QList<DriveFile> &results)
{
    m_resultsList->clear();

    if (results.isEmpty())
    {
        m_resultsList->addItem("No matches found.");
        return;
    }

    for (const auto &file : results)
    {
        auto *item = new QListWidgetItem(file.fileName);
        item->setData(Qt::UserRole, file.fileId);
        item->setData(Qt::UserRole + 1, file.modifiedTime);
        QString dateStr = file.modifiedTime.isValid()
            ? file.modifiedTime.toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
            : "";
        if (!dateStr.isEmpty())
            item->setToolTip(dateStr);
        m_resultsList->addItem(item);
    }
}
