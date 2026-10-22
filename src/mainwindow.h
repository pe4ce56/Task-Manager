#pragma once

#include "taskstore.h"

#include <QMainWindow>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

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
    void taskCellChanged(int row, int column);

private:
    QUuid selectedTaskId() const;
    void showStoreError(const QString &message);

    TaskStore m_store;
    QLineEdit *m_search = nullptr;
    QComboBox *m_status = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_emptyState = nullptr;
    QLabel *m_count = nullptr;
    bool m_storageAvailable = false;
};
