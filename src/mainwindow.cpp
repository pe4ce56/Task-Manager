#include "mainwindow.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

namespace {
struct TaskFields {
    QString title;
    QString notes;
};

bool promptForTask(QWidget *parent, const QString &heading, const Task *task, TaskFields *fields)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(heading);
    dialog.setModal(true);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *title = new QLineEdit(task ? task->title : QString(), &dialog);
    title->setMaxLength(500);
    title->setPlaceholderText(QStringLiteral("What needs to get done?"));
    auto *notes = new QTextEdit(&dialog);
    notes->setPlaceholderText(QStringLiteral("Optional details"));
    notes->setPlainText(task ? task->notes : QString());
    notes->setMinimumHeight(100);
    auto *validation = new QLabel(&dialog);
    validation->setStyleSheet(QStringLiteral("color: #a12d24"));
    validation->setWordWrap(true);
    validation->hide();
    form->addRow(QStringLiteral("Title"), title);
    form->addRow(QStringLiteral("Notes"), notes);
    layout->addLayout(form);
    layout->addWidget(validation);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (title->text().trimmed().isEmpty()) {
            validation->setText(QStringLiteral("Enter a task title to save this task."));
            validation->show();
            title->setFocus();
            return;
        }
        fields->title = title->text();
        fields->notes = notes->toPlainText();
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.resize(440, 300);
    title->setFocus();
    return dialog.exec() == QDialog::Accepted;
}
}

MainWindow::MainWindow(const QString &dataFile, QWidget *parent)
    : QMainWindow(parent), m_store(dataFile)
{
    setWindowTitle(QStringLiteral("Task Manager"));
    resize(860, 560);
    setMinimumSize(560, 380);

    auto *toolbar = addToolBar(QStringLiteral("Tasks"));
    toolbar->setMovable(false);
    QAction *addAction = toolbar->addAction(QStringLiteral("Add task"));
    QAction *editAction = toolbar->addAction(QStringLiteral("Edit"));
    QAction *toggleAction = toolbar->addAction(QStringLiteral("Complete / reopen"));
    QAction *deleteAction = toolbar->addAction(QStringLiteral("Delete"));
    connect(addAction, &QAction::triggered, this, &MainWindow::addTask);
    connect(editAction, &QAction::triggered, this, &MainWindow::editSelectedTask);
    connect(toggleAction, &QAction::triggered, this, &MainWindow::toggleSelectedTask);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedTask);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *heading = new QLabel(QStringLiteral("Your tasks"), central);
    QFont headingFont = heading->font();
    headingFont.setPointSize(18);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto *filters = new QHBoxLayout;
    m_search = new QLineEdit(central);
    m_search->setClearButtonEnabled(true);
    m_search->setObjectName(QStringLiteral("taskSearch"));
    m_search->setPlaceholderText(QStringLiteral("Search titles and notes…"));
    m_search->setAccessibleName(QStringLiteral("Search tasks"));
    m_status = new QComboBox(central);
    m_status->setObjectName(QStringLiteral("taskStatus"));
    m_status->addItems({QStringLiteral("All tasks"), QStringLiteral("Active"), QStringLiteral("Completed")});
    m_status->setAccessibleName(QStringLiteral("Filter by status"));
    filters->addWidget(m_search, 1);
    filters->addWidget(m_status);
    layout->addLayout(filters);

    auto *tableHost = new QWidget(central);
    auto *tableLayout = new QVBoxLayout(tableHost);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableWidget(tableHost);
    m_table->setObjectName(QStringLiteral("taskTable"));
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Done"), QStringLiteral("Task"), QStringLiteral("Notes")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setAlternatingRowColors(true);
    tableLayout->addWidget(m_table);
    m_emptyState = new QLabel(tableHost);
    m_emptyState->setObjectName(QStringLiteral("taskEmptyState"));
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setWordWrap(true);
    tableLayout->addWidget(m_emptyState);
    layout->addWidget(tableHost, 1);

    m_count = new QLabel(central);
    layout->addWidget(m_count);
    setCentralWidget(central);

    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::refreshTasks);
    connect(m_status, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTasks);
    connect(m_table, &QTableWidget::cellChanged, this, &MainWindow::taskCellChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { editSelectedTask(); });
    connect(new QShortcut(QKeySequence::New, this), &QShortcut::activated, this, &MainWindow::addTask);
    connect(new QShortcut(QKeySequence(Qt::Key_Delete), this), &QShortcut::activated, this, &MainWindow::deleteSelectedTask);
    connect(new QShortcut(QKeySequence(Qt::Key_Return), this), &QShortcut::activated, this, &MainWindow::editSelectedTask);
    connect(new QShortcut(QKeySequence(Qt::Key_Enter), this), &QShortcut::activated, this, &MainWindow::editSelectedTask);

    QString error;
    if (!m_store.load(&error)) {
        QMessageBox::critical(this, QStringLiteral("Could not load tasks"),
                              QStringLiteral("Your task file could not be read. It has not been changed.\n\n%1").arg(error));
        for (QAction *action : toolbar->actions()) action->setEnabled(false);
    } else {
        m_storageAvailable = true;
    }
    refreshTasks();
}

void MainWindow::refreshTasks()
{
    const QSignalBlocker blockTableSignals(m_table);
    if (!m_storageAvailable) {
        m_table->setRowCount(0);
        m_table->hide();
        m_emptyState->setText(QStringLiteral("Tasks could not be loaded. The saved file has not been changed."));
        m_emptyState->show();
        m_count->setText(QStringLiteral("Task storage is unavailable."));
        return;
    }
    const TaskStatusFilter filter = m_status->currentIndex() == 1 ? TaskStatusFilter::Active
        : m_status->currentIndex() == 2 ? TaskStatusFilter::Completed : TaskStatusFilter::All;
    const QList<Task> visible = m_store.filtered(m_search->text(), filter);
    m_table->setRowCount(0);
    for (const Task &task : visible) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *done = new QTableWidgetItem;
        done->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        done->setCheckState(task.completed ? Qt::Checked : Qt::Unchecked);
        done->setData(Qt::UserRole, task.id.toString(QUuid::WithoutBraces));
        auto *title = new QTableWidgetItem(task.title);
        auto *notes = new QTableWidgetItem(task.notes);
        if (task.completed) {
            title->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
            QFont font = title->font();
            font.setStrikeOut(true);
            title->setFont(font);
        }
        m_table->setItem(row, 0, done);
        m_table->setItem(row, 1, title);
        m_table->setItem(row, 2, notes);
    }
    m_emptyState->setVisible(visible.isEmpty());
    m_table->setVisible(!visible.isEmpty());
    if (m_store.tasks().isEmpty()) {
        m_emptyState->setText(QStringLiteral("No tasks yet. Add one to get started."));
    } else if (visible.isEmpty()) {
        m_emptyState->setText(m_search->text().isEmpty()
            ? QStringLiteral("No tasks in this status.")
            : QStringLiteral("No tasks match your search."));
    }
    const qsizetype active = std::count_if(m_store.tasks().cbegin(), m_store.tasks().cend(), [](const Task &task) { return !task.completed; });
    m_count->setText(QStringLiteral("%1 active · %2 completed").arg(active).arg(m_store.tasks().size() - active));
}

void MainWindow::addTask()
{
    if (!m_storageAvailable) return;
    TaskFields fields;
    if (!promptForTask(this, QStringLiteral("Add task"), nullptr, &fields)) return;
    QString error;
    if (!m_store.add(fields.title, fields.notes, nullptr, &error)) {
        showStoreError(error);
        return;
    }
    refreshTasks();
}

void MainWindow::editSelectedTask()
{
    if (!m_storageAvailable) return;
    const QUuid id = selectedTaskId();
    if (id.isNull()) return;
    const auto it = std::find_if(m_store.tasks().cbegin(), m_store.tasks().cend(), [&id](const Task &task) { return task.id == id; });
    if (it == m_store.tasks().cend()) return;
    TaskFields fields;
    if (!promptForTask(this, QStringLiteral("Edit task"), &*it, &fields)) return;
    QString error;
    if (!m_store.edit(id, fields.title, fields.notes, &error)) {
        showStoreError(error);
        return;
    }
    refreshTasks();
}

void MainWindow::deleteSelectedTask()
{
    if (!m_storageAvailable) return;
    const QUuid id = selectedTaskId();
    if (id.isNull()) return;
    const auto it = std::find_if(m_store.tasks().cbegin(), m_store.tasks().cend(), [&id](const Task &task) { return task.id == id; });
    if (it == m_store.tasks().cend()) return;
    if (QMessageBox::question(this, QStringLiteral("Delete task"),
                              QStringLiteral("Delete “%1”? This cannot be undone.").arg(it->title),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QString error;
    if (!m_store.remove(id, &error)) showStoreError(error);
    refreshTasks();
}

void MainWindow::toggleSelectedTask()
{
    if (!m_storageAvailable) return;
    const QUuid id = selectedTaskId();
    if (id.isNull()) return;
    const auto it = std::find_if(m_store.tasks().cbegin(), m_store.tasks().cend(), [&id](const Task &task) { return task.id == id; });
    if (it == m_store.tasks().cend()) return;
    QString error;
    if (!m_store.setCompleted(id, !it->completed, &error)) showStoreError(error);
    refreshTasks();
}

void MainWindow::taskCellChanged(int row, int column)
{
    if (!m_storageAvailable || column != 0 || !m_table->item(row, 0)) return;
    const QUuid id(m_table->item(row, 0)->data(Qt::UserRole).toString());
    const bool completed = m_table->item(row, 0)->checkState() == Qt::Checked;
    QString error;
    if (!m_store.setCompleted(id, completed, &error)) showStoreError(error);
    refreshTasks();
}

QUuid MainWindow::selectedTaskId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) return {};
    return QUuid(m_table->item(row, 0)->data(Qt::UserRole).toString());
}

void MainWindow::showStoreError(const QString &message)
{
    QMessageBox::critical(this, QStringLiteral("Could not save task"),
                          QStringLiteral("Your saved tasks were left unchanged.\n\n%1").arg(message));
}
