#include "taskstore.h"
#include "mainwindow.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QtTest>

class TaskTests : public QObject
{
    Q_OBJECT

private slots:
    void addEditCompleteAndDelete();
    void rejectBlankTitlesWithoutChangingSavedData();
    void filterByQueryAndStatus();
    void persistAcrossStoreInstances();
    void rejectCorruptFileWithoutReplacingLoadedTasks();
    void failedSaveRollsBackOperation();
    void mainWindowWorkflow();
};

void TaskTests::addEditCompleteAndDelete()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    TaskStore store(directory.filePath(QStringLiteral("tasks.json")));
    QVERIFY(store.load());
    QUuid id;
    QVERIFY(store.add(QStringLiteral("  Draft plan  "), QStringLiteral("  First pass  "), &id));
    QCOMPARE(store.tasks().size(), 1);
    QCOMPARE(store.tasks().first().title, QStringLiteral("Draft plan"));
    QCOMPARE(store.tasks().first().notes, QStringLiteral("First pass"));
    QVERIFY(store.edit(id, QStringLiteral("Review plan"), QStringLiteral("With the team")));
    QCOMPARE(store.tasks().first().title, QStringLiteral("Review plan"));
    QVERIFY(store.setCompleted(id, true));
    QVERIFY(store.tasks().first().completed);
    QVERIFY(store.setCompleted(id, false));
    QVERIFY(!store.tasks().first().completed);
    QVERIFY(store.remove(id));
    QVERIFY(store.tasks().isEmpty());
}

void TaskTests::rejectBlankTitlesWithoutChangingSavedData()
{
    QTemporaryDir directory;
    TaskStore store(directory.filePath(QStringLiteral("tasks.json")));
    QString error;
    QUuid id;
    QVERIFY(store.add(QStringLiteral("Keep this"), QStringLiteral("Saved"), &id));
    const QByteArray before = [&] {
        QFile file(directory.filePath(QStringLiteral("tasks.json")));
        if (!file.open(QIODevice::ReadOnly)) return QByteArray();
        return file.readAll();
    }();
    QVERIFY(!store.add(QStringLiteral(" \t "), QString(), nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!store.edit(id, QStringLiteral("  "), QStringLiteral("Changed"), &error));
    QCOMPARE(store.tasks().size(), 1);
    QCOMPARE(store.tasks().first().title, QStringLiteral("Keep this"));
    QFile file(directory.filePath(QStringLiteral("tasks.json")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), before);
}

void TaskTests::filterByQueryAndStatus()
{
    QTemporaryDir directory;
    TaskStore store(directory.filePath(QStringLiteral("tasks.json")));
    QVERIFY(store.add(QStringLiteral("Buy oats"), QStringLiteral("Grocery list")));
    QUuid completedId;
    QVERIFY(store.add(QStringLiteral("Send report"), QStringLiteral("For planning"), &completedId));
    QVERIFY(store.setCompleted(completedId, true));
    QCOMPARE(store.filtered(QString(), TaskStatusFilter::All).size(), 2);
    QCOMPARE(store.filtered(QString(), TaskStatusFilter::Active).size(), 1);
    QCOMPARE(store.filtered(QString(), TaskStatusFilter::Completed).size(), 1);
    QCOMPARE(store.filtered(QStringLiteral("GROCERY"), TaskStatusFilter::All).size(), 1);
    QCOMPARE(store.filtered(QStringLiteral("planning"), TaskStatusFilter::Completed).size(), 1);
    QVERIFY(store.filtered(QStringLiteral("missing"), TaskStatusFilter::All).isEmpty());
}

void TaskTests::persistAcrossStoreInstances()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("tasks.json"));
    QUuid id;
    {
        TaskStore store(path);
        QVERIFY(store.load());
        QVERIFY(store.add(QStringLiteral("Saved task"), QStringLiteral("Saved notes"), &id));
        QVERIFY(store.setCompleted(id, true));
    }
    TaskStore reopened(path);
    QVERIFY(reopened.load());
    QCOMPARE(reopened.tasks().size(), 1);
    QCOMPARE(reopened.tasks().first().id, id);
    QCOMPARE(reopened.tasks().first().title, QStringLiteral("Saved task"));
    QCOMPARE(reopened.tasks().first().notes, QStringLiteral("Saved notes"));
    QVERIFY(reopened.tasks().first().completed);
}

void TaskTests::rejectCorruptFileWithoutReplacingLoadedTasks()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("tasks.json"));
    TaskStore store(path);
    QVERIFY(store.add(QStringLiteral("Safe in memory"), QStringLiteral("")));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("not json");
    file.close();
    QString error;
    QVERIFY(!store.load(&error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.tasks().size(), 1);
    QCOMPARE(store.tasks().first().title, QStringLiteral("Safe in memory"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("not json"));
}

void TaskTests::failedSaveRollsBackOperation()
{
    QTemporaryDir directory;
    TaskStore store(directory.path());
    QString error;
    QVERIFY(!store.add(QStringLiteral("Cannot save"), QString(), nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(store.tasks().isEmpty());
}

void TaskTests::mainWindowWorkflow()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("tasks.json"));
    MainWindow window(path);
    window.show();
    auto *toolbar = window.findChild<QToolBar *>();
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("taskTable"));
    auto *search = window.findChild<QLineEdit *>(QStringLiteral("taskSearch"));
    auto *status = window.findChild<QComboBox *>(QStringLiteral("taskStatus"));
    auto *emptyState = window.findChild<QLabel *>(QStringLiteral("taskEmptyState"));
    QVERIFY(toolbar && table && search && status && emptyState);
    QVERIFY(emptyState->isVisible());

    auto action = [toolbar](const QString &label) {
        for (QAction *candidate : toolbar->actions()) {
            if (candidate->text() == label) return candidate;
        }
        return static_cast<QAction *>(nullptr);
    };

    QAction *add = action(QStringLiteral("Add task"));
    QVERIFY(add);
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog) qFatal("Add task dialog did not open");
        auto *title = dialog->findChild<QLineEdit *>();
        auto *notes = dialog->findChild<QTextEdit *>();
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        if (!title || !notes || !buttons) qFatal("Add task controls are missing");
        QAbstractButton *save = buttons->button(QDialogButtonBox::Save);
        save->click();
        const auto labels = dialog->findChildren<QLabel *>();
        bool hasValidation = false;
        for (QLabel *label : labels) hasValidation |= label->isVisible() && label->text().contains(QStringLiteral("title"), Qt::CaseInsensitive);
        if (!hasValidation) qFatal("Blank title was not rejected with a visible message");
        title->setText(QStringLiteral("Plan review"));
        notes->setPlainText(QStringLiteral("Bring the draft"));
        save->click();
    });
    add->trigger();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Plan review"));

    table->selectRow(0);
    QAction *edit = action(QStringLiteral("Edit"));
    QVERIFY(edit);
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog) qFatal("Edit task dialog did not open");
        auto *title = dialog->findChild<QLineEdit *>();
        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        if (!title || !buttons) qFatal("Edit task controls are missing");
        title->setText(QStringLiteral("Review revised plan"));
        buttons->button(QDialogButtonBox::Save)->click();
    });
    edit->trigger();
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("Review revised plan"));

    search->setText(QStringLiteral("draft"));
    QCOMPARE(table->rowCount(), 1);
    search->setText(QStringLiteral("not present"));
    QCOMPARE(table->rowCount(), 0);
    QVERIFY(emptyState->text().contains(QStringLiteral("No tasks match")));
    search->clear();
    table->item(0, 0)->setCheckState(Qt::Checked);
    QCOMPARE(table->item(0, 0)->checkState(), Qt::Checked);
    status->setCurrentIndex(1);
    QCOMPARE(table->rowCount(), 0);
    status->setCurrentIndex(2);
    QCOMPARE(table->rowCount(), 1);

    table->selectRow(0);
    QAction *remove = action(QStringLiteral("Delete"));
    QVERIFY(remove);
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!dialog) qFatal("Delete confirmation did not open");
        dialog->button(QMessageBox::Yes)->click();
    });
    remove->trigger();
    QCOMPARE(table->rowCount(), 0);

    TaskStore reopened(path);
    QVERIFY(reopened.load());
    QVERIFY(reopened.tasks().isEmpty());
}

QTEST_MAIN(TaskTests)
#include "task_tests.moc"
