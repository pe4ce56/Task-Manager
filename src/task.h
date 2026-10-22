#pragma once

#include <QList>
#include <QString>
#include <QUuid>

struct Task
{
    QUuid id;
    QString title;
    QString notes;
    bool completed = false;
};
enum class TaskStatusFilter
{
    All,
    Active,
    Completed
};

