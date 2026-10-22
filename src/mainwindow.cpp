#include "mainwindow.h"

#include <algorithm>
#include <functional>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTextEdit>
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

class SelectableTaskRow : public QWidget
{
public:
    SelectableTaskRow(QListWidget *list, QListWidgetItem *item, QWidget *parent)
        : QWidget(parent), m_list(list), m_item(item)
    {
    }

    void setDoubleClickHandler(std::function<void()> handler) { m_doubleClickHandler = std::move(handler); }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QMetaObject::invokeMethod(m_list, [list = m_list, item = m_item] { list->setCurrentItem(item); }, Qt::QueuedConnection);
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        QMetaObject::invokeMethod(m_list, [list = m_list, item = m_item] { list->setCurrentItem(item); }, Qt::QueuedConnection);
        if (m_doubleClickHandler) QMetaObject::invokeMethod(m_list, m_doubleClickHandler, Qt::QueuedConnection);
        event->accept();
    }

private:
    QListWidget *m_list;
    QListWidgetItem *m_item;
    std::function<void()> m_doubleClickHandler;
};

QWidget *taskRow(const Task &task, QListWidget *list, QListWidgetItem *item,
                 const std::function<void(bool)> &onChanged, const std::function<void()> &onDoubleClicked)
{
    auto *row = new SelectableTaskRow(list, item, list);
    row->setDoubleClickHandler(onDoubleClicked);
    row->setObjectName(QStringLiteral("taskRow"));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(14, 11, 14, 11);
    layout->setSpacing(12);
    auto *done = new QCheckBox(row);
    done->setObjectName(QStringLiteral("taskCompletion"));
    done->setAccessibleName(QStringLiteral("Mark %1 %2").arg(task.title, task.completed ? QStringLiteral("active") : QStringLiteral("complete")));
    done->setChecked(task.completed);
    done->setFocusPolicy(Qt::StrongFocus);
    QObject::connect(done, &QCheckBox::toggled, row, onChanged);
    layout->addWidget(done, 0, Qt::AlignTop);

    auto *copy = new QVBoxLayout;
    copy->setSpacing(4);
    auto *title = new QLabel(task.title, row);
    title->setObjectName(QStringLiteral("taskTitle"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    title->setWordWrap(true);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    title->setProperty("completed", task.completed);
    copy->addWidget(title);
    if (!task.notes.trimmed().isEmpty()) {
        auto *notes = new QLabel(task.notes, row);
        notes->setObjectName(QStringLiteral("taskNotes"));
        notes->setAttribute(Qt::WA_TransparentForMouseEvents);
        notes->setWordWrap(true);
        notes->setProperty("completed", task.completed);
        copy->addWidget(notes);
    } else {
        auto *notes = new QLabel(QStringLiteral("No notes"), row);
        notes->setObjectName(QStringLiteral("taskNotes"));
        notes->setAttribute(Qt::WA_TransparentForMouseEvents);
        notes->setProperty("placeholder", true);
        copy->addWidget(notes);
    }
    layout->addLayout(copy, 1);
    row->setMinimumHeight(68);
    return row;
}
}

MainWindow::MainWindow(const QString &dataFile, QWidget *parent)
    : QMainWindow(parent), m_store(dataFile)
{
    setWindowTitle(QStringLiteral("Task Manager"));
    resize(880, 620);
    setMinimumSize(560, 420);
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f4f5f2; }"
        "QWidget#page { color: #20251f; }"
        "QLabel#eyebrow { color: #637064; font-size: 10pt; font-weight: 600; }"
        "QLabel#pageTitle { color: #20251f; font-size: 23pt; font-weight: 700; }"
        "QLabel#summary { color: #59655b; font-size: 10pt; }"
        "QPushButton { min-height: 34px; padding: 0 12px; border: 1px solid #d5dbd4; border-radius: 6px; background: #ffffff; color: #263129; }"
        "QPushButton:hover { background: #edf1ec; }"
        "QPushButton:focus { border: 2px solid #387653; }"
        "QPushButton#addTask { background: #286342; border-color: #286342; color: white; font-weight: 600; padding: 0 16px; }"
        "QPushButton#addTask:hover { background: #1f5235; }"
        "QPushButton:disabled { color: #929992; background: #eef0ed; }"
        "QLineEdit, QComboBox { min-height: 36px; padding: 0 10px; border: 1px solid #ccd3cb; border-radius: 6px; background: white; color: #20251f; }"
        "QLineEdit:focus, QComboBox:focus { border: 2px solid #387653; }"
        "QFrame#taskPanel { background: white; border: 1px solid #dfe4dd; border-radius: 9px; }"
        "QListWidget { background: transparent; border: 0; outline: 0; }"
        "QListWidget::item { background: white; border: 1px solid #e0e5df; border-radius: 7px; margin: 4px 5px; }"
        "QListWidget::item:selected { background: #edf5ee; border: 2px solid #4d805e; }"
        "QLabel#taskNotes { color: #657066; }"
        "QLabel#taskNotes[placeholder=true] { color: #929b92; font-style: italic; }"
        "QLabel#taskTitle[completed=true] { color: #7b857c; text-decoration: line-through; }"
        "QLabel#taskNotes[completed=true] { color: #909991; }"
        "QLabel#emptyTitle { font-size: 15pt; font-weight: 650; color: #303a31; }"
        "QLabel#emptyBody { color: #68736a; }"
        "QLabel#selectionHint { color: #69746b; }"
    ));

    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("page"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(16);

    auto *header = new QHBoxLayout;
    auto *heading = new QVBoxLayout;
    heading->setSpacing(3);
    auto *eyebrow = new QLabel(QStringLiteral("PERSONAL WORKSPACE"), page);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto *title = new QLabel(QStringLiteral("Tasks"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    heading->addWidget(eyebrow);
    heading->addWidget(title);
    header->addLayout(heading, 1);
    auto *addButton = new QPushButton(QStringLiteral("+  Add task"), page);
    addButton->setObjectName(QStringLiteral("addTask"));
    addButton->setAccessibleName(QStringLiteral("Add task"));
    addButton->setMinimumWidth(116);
    header->addWidget(addButton, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    m_summary = new QLabel(page);
    m_summary->setObjectName(QStringLiteral("summary"));
    layout->addWidget(m_summary);

    auto *filters = new QHBoxLayout;
    filters->setSpacing(10);
    m_search = new QLineEdit(page);
    m_search->setClearButtonEnabled(true);
    m_search->setObjectName(QStringLiteral("taskSearch"));
    m_search->setPlaceholderText(QStringLiteral("Search titles and notes"));
    m_search->setAccessibleName(QStringLiteral("Search tasks by title or notes"));
    m_status = new QComboBox(page);
    m_status->setObjectName(QStringLiteral("taskStatus"));
    m_status->addItems({QStringLiteral("All tasks"), QStringLiteral("Active"), QStringLiteral("Completed")});
    m_status->setAccessibleName(QStringLiteral("Filter tasks by status"));
    m_status->setMinimumWidth(132);
    filters->addWidget(m_search, 1);
    filters->addWidget(m_status);
    layout->addLayout(filters);

    auto *panel = new QFrame(page);
    panel->setObjectName(QStringLiteral("taskPanel"));
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    m_tasks = new QListWidget(panel);
    m_tasks->setObjectName(QStringLiteral("taskList"));
    m_tasks->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tasks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tasks->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tasks->setSpacing(2);
    panelLayout->addWidget(m_tasks);
    m_emptyState = new QLabel(panel);
    m_emptyState->setObjectName(QStringLiteral("taskEmptyState"));
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setWordWrap(true);
    panelLayout->addWidget(m_emptyState, 1);
    layout->addWidget(panel, 1);

    auto *footer = new QHBoxLayout;
    m_selectionHint = new QLabel(QStringLiteral("Select a task to edit, complete, or delete."), page);
    m_selectionHint->setObjectName(QStringLiteral("selectionHint"));
    m_selectionHint->setWordWrap(true);
    footer->addWidget(m_selectionHint, 1);
    m_editButton = new QPushButton(QStringLiteral("Edit"), page);
    m_editButton->setObjectName(QStringLiteral("editTask"));
    m_completeButton = new QPushButton(QStringLiteral("Complete"), page);
    m_completeButton->setObjectName(QStringLiteral("toggleTask"));
    m_deleteButton = new QPushButton(QStringLiteral("Delete"), page);
    m_deleteButton->setObjectName(QStringLiteral("deleteTask"));
    footer->addWidget(m_editButton);
    footer->addWidget(m_completeButton);
    footer->addWidget(m_deleteButton);
    layout->addLayout(footer);
    setCentralWidget(page);

    connect(addButton, &QPushButton::clicked, this, &MainWindow::addTask);
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::editSelectedTask);
    connect(m_completeButton, &QPushButton::clicked, this, &MainWindow::toggleSelectedTask);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTask);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::refreshTasks);
    connect(m_status, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::refreshTasks);
    connect(m_tasks, &QListWidget::itemSelectionChanged, this, &MainWindow::refreshTasks);
    connect(new QShortcut(QKeySequence::New, this), &QShortcut::activated, this, &MainWindow::addTask);
    connect(new QShortcut(QKeySequence(Qt::Key_Delete), this), &QShortcut::activated, this, &MainWindow::deleteSelectedTask);
    connect(new QShortcut(QKeySequence(Qt::Key_Return), this), &QShortcut::activated, this, &MainWindow::editSelectedTask);
    connect(new QShortcut(QKeySequence(Qt::Key_Enter), this), &QShortcut::activated, this, &MainWindow::editSelectedTask);

    QString error;
    if (!m_store.load(&error)) {
        QMessageBox::critical(this, QStringLiteral("Could not load tasks"),
                              QStringLiteral("Your task file could not be read. It has not been changed.\n\n%1").arg(error));
    } else {
        m_storageAvailable = true;
    }
    addButton->setEnabled(m_storageAvailable);
    refreshTasks();
}

void MainWindow::refreshTasks()
{
    const TaskStatusFilter filter = m_status->currentIndex() == 1 ? TaskStatusFilter::Active
        : m_status->currentIndex() == 2 ? TaskStatusFilter::Completed : TaskStatusFilter::All;
    const QList<Task> visible = m_storageAvailable ? m_store.filtered(m_search->text(), filter) : QList<Task>();
    const QUuid selected = selectedTaskId();
    const QSignalBlocker blockSignals(m_tasks);
    m_tasks->clear();
    for (const Task &task : visible) {
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, task.id.toString(QUuid::WithoutBraces));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_tasks->addItem(item);
        QWidget *row = taskRow(task, m_tasks, item,
            [this, id = task.id, item](bool completed) {
                QMetaObject::invokeMethod(this, [this, id, item, completed] {
                    m_tasks->setCurrentItem(item);
                    toggleTask(id, completed);
                }, Qt::QueuedConnection);
            }, [this] { editSelectedTask(); });
        m_tasks->setItemWidget(item, row);
        item->setSizeHint(row->sizeHint());
        if (task.id == selected) m_tasks->setCurrentItem(item);
    }

    const bool noTasks = m_store.tasks().isEmpty();
    const bool noResults = !noTasks && visible.isEmpty();
    m_tasks->setVisible(m_storageAvailable && !visible.isEmpty());
    m_emptyState->setVisible(!m_storageAvailable || visible.isEmpty());
    if (!m_storageAvailable) {
        m_emptyState->setText(QStringLiteral("Task storage is unavailable. Your saved file has not been changed."));
    } else if (noTasks) {
        m_emptyState->setText(QStringLiteral("No tasks yet\nAdd a task to start keeping track of your day."));
    } else if (noResults && !m_search->text().trimmed().isEmpty()) {
        m_emptyState->setText(QStringLiteral("No matching tasks\nTry another search or clear the search field."));
    } else if (noResults) {
        m_emptyState->setText(QStringLiteral("No tasks in this view\nChoose another status filter to see more tasks."));
    }
    const qsizetype completed = std::count_if(m_store.tasks().cbegin(), m_store.tasks().cend(), [](const Task &task) { return task.completed; });
    const qsizetype active = m_store.tasks().size() - completed;
    m_summary->setText(QStringLiteral("%1 active   ·   %2 completed").arg(active).arg(completed));
    const QUuid currentId = selectedTaskId();
    const auto current = std::find_if(m_store.tasks().cbegin(), m_store.tasks().cend(), [&currentId](const Task &task) { return task.id == currentId; });
    const bool hasSelection = current != m_store.tasks().cend();
    m_editButton->setEnabled(m_storageAvailable && hasSelection);
    m_completeButton->setEnabled(m_storageAvailable && hasSelection);
    m_deleteButton->setEnabled(m_storageAvailable && hasSelection);
    m_completeButton->setText(hasSelection && current->completed ? QStringLiteral("Reopen") : QStringLiteral("Complete"));
    m_selectionHint->setText(hasSelection
        ? QStringLiteral("Selected: %1").arg(current->title)
        : QStringLiteral("Select a task to edit, complete, or delete."));
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
    m_search->clear();
    m_status->setCurrentIndex(0);
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
                              QStringLiteral("Delete ‘%1’? This cannot be undone.").arg(it->title),
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
    if (it != m_store.tasks().cend()) toggleTask(id, !it->completed);
}

void MainWindow::toggleTask(const QUuid &id, bool completed)
{
    QString error;
    if (!m_store.setCompleted(id, completed, &error)) showStoreError(error);
    refreshTasks();
}

QUuid MainWindow::selectedTaskId() const
{
    const QListWidgetItem *item = m_tasks->currentItem();
    return item ? QUuid(item->data(Qt::UserRole).toString()) : QUuid();
}

void MainWindow::showStoreError(const QString &message)
{
    QMessageBox::critical(this, QStringLiteral("Could not save task"),
                          QStringLiteral("Your saved tasks were left unchanged.\n\n%1").arg(message));
}
