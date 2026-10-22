#pragma once

#include "task.h"

class TaskStore
{
public:
    explicit TaskStore(QString filePath);

    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;

    const QList<Task> &tasks() const;
    bool add(const QString &title, const QString &notes, QUuid *createdId = nullptr,
             QString *error = nullptr);
    bool edit(const QUuid &id, const QString &title, const QString &notes,
              QString *error = nullptr);
    bool remove(const QUuid &id, QString *error = nullptr);
    bool setCompleted(const QUuid &id, bool completed, QString *error = nullptr);
    QList<Task> filtered(const QString &query, TaskStatusFilter status) const;

private:
    Task *find(const QUuid &id);
    QString m_filePath;
    QList<Task> m_tasks;
};
