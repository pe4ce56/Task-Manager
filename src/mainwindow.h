#pragma once

#include "taskstore.h"

#include <QMainWindow>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &dataFile, QWidget *parent = nullptr);

private slots:
    void refreshTasks();
    void addTask();
    void editSelectedTask();
    void deleteSelectedTask();
    void toggleSelectedTask();
    void toggleTask(const QUuid &id, bool completed);

private:
    QUuid selectedTaskId() const;
    void showStoreError(const QString &message);

    TaskStore m_store;
    QLineEdit *m_search = nullptr;
    QComboBox *m_status = nullptr;
    QListWidget *m_tasks = nullptr;
    QLabel *m_emptyState = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_selectionHint = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_completeButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    bool m_storageAvailable = false;
};
