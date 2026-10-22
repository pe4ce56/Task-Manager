#include "task.h"

#include "taskstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

TaskStore::TaskStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

bool TaskStore::load(QString *error)
{
    const QFileInfo info(m_filePath);
    if (!info.exists()) {
        m_tasks.clear();
        return true;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) *error = QStringLiteral("The task file is invalid: %1").arg(parseError.errorString());
        return false;
    }

    QList<Task> loaded;
    for (const QJsonValue &value : document.array()) {
        if (!value.isObject()) {
            if (error) *error = QStringLiteral("The task file contains an invalid task.");
            return false;
        }
        const QJsonObject object = value.toObject();
        const QUuid id(object.value(QStringLiteral("id")).toString());
        const QString title = object.value(QStringLiteral("title")).toString();
        const QJsonValue notesValue = object.value(QStringLiteral("notes"));
        const QJsonValue completedValue = object.value(QStringLiteral("completed"));
        if (id.isNull() || title.trimmed().isEmpty() || !notesValue.isString()
            || !completedValue.isBool()) {
            if (error) *error = QStringLiteral("The task file contains incomplete or invalid task data.");
            return false;
        }
        loaded.append({id, title, notesValue.toString(), completedValue.toBool()});
    }
    m_tasks = loaded;
    return true;
}

bool TaskStore::save(QString *error) const
{
    const QFileInfo info(m_filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("Could not create the task data folder.");
        return false;
    }

    QJsonArray array;
    for (const Task &task : m_tasks) {
        array.append(QJsonObject{
            {QStringLiteral("id"), task.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("title"), task.title},
            {QStringLiteral("notes"), task.notes},
            {QStringLiteral("completed"), task.completed}
        });
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray contents = QJsonDocument(array).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

const QList<Task> &TaskStore::tasks() const
{
    return m_tasks;
}

bool TaskStore::add(const QString &title, const QString &notes, QUuid *createdId, QString *error)
{
    if (title.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Enter a task title.");
        return false;
    }
    Task task{QUuid::createUuid(), title.trimmed(), notes.trimmed(), false};
    m_tasks.append(task);
    if (!save(error)) {
        m_tasks.removeLast();
        return false;
    }
    if (createdId) *createdId = task.id;
    return true;
}

bool TaskStore::edit(const QUuid &id, const QString &title, const QString &notes, QString *error)
{
    if (title.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Enter a task title.");
        return false;
    }
    Task *task = find(id);
    if (!task) {
        if (error) *error = QStringLiteral("This task no longer exists.");
        return false;
    }
    const Task previous = *task;
    task->title = title.trimmed();
    task->notes = notes.trimmed();
    if (!save(error)) {
        *task = previous;
        return false;
    }
    return true;
}

bool TaskStore::remove(const QUuid &id, QString *error)
{
    const auto it = std::find_if(m_tasks.begin(), m_tasks.end(), [&id](const Task &task) { return task.id == id; });
    if (it == m_tasks.end()) {
        if (error) *error = QStringLiteral("This task no longer exists.");
        return false;
    }
    const int index = std::distance(m_tasks.begin(), it);
    const Task removed = m_tasks.takeAt(index);
    if (!save(error)) {
        m_tasks.insert(index, removed);
        return false;
    }
    return true;
}

bool TaskStore::setCompleted(const QUuid &id, bool completed, QString *error)
{
    Task *task = find(id);
    if (!task) {
        if (error) *error = QStringLiteral("This task no longer exists.");
        return false;
    }
    const bool previous = task->completed;
    task->completed = completed;
    if (!save(error)) {
        task->completed = previous;
        return false;
    }
    return true;
}

QList<Task> TaskStore::filtered(const QString &query, TaskStatusFilter status) const
{
    QList<Task> result;
    const QString needle = query.trimmed();
    for (const Task &task : m_tasks) {
        if (status == TaskStatusFilter::Active && task.completed) continue;
        if (status == TaskStatusFilter::Completed && !task.completed) continue;
        if (!needle.isEmpty() && !task.title.contains(needle, Qt::CaseInsensitive)
            && !task.notes.contains(needle, Qt::CaseInsensitive)) continue;
        result.append(task);
    }
    return result;
}

Task *TaskStore::find(const QUuid &id)
{
    for (Task &task : m_tasks) {
        if (task.id == id) return &task;
    }
    return nullptr;
}
