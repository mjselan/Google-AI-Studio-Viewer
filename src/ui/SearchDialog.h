#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include "api/DriveApi.h"

class SearchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SearchDialog(QWidget *parent = nullptr);

    void setResults(const QList<DriveFile> &results);

signals:
    void searchRequested(const QString &query);
    void resultClicked(const QString &fileId, const QString &fileName, const QDateTime &modTime);

private slots:
    void onSearchTextChanged(const QString &text);
    void onDebounceTimeout();

private:
    QLineEdit *m_searchInput;
    QListWidget *m_resultsList;
    QTimer *m_debounceTimer;
};
