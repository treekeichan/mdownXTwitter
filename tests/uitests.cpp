#include <QtTest>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QTemporaryDir>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTimer>
#include "mainwindow.h"

class UiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings{QCoreApplication::applicationDirPath() + QStringLiteral("/ui-settings-XXXXXX")};
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("MdowTests"));
        QCoreApplication::setApplicationName(QStringLiteral("QueueTests"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        // Keep history isolated from the user's actual app data.
        QStandardPaths::setTestModeEnabled(true);
    }
    void multipleVideosAndDownloadQueue() {
        QTimer dismissUnexpectedDialog;
        connect(&dismissUnexpectedDialog, &QTimer::timeout, this, [] {
            for (auto *widget : QApplication::topLevelWidgets()) if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                qWarning() << "Unexpected dialog:" << box->text(); box->reject();
            }
        });
        dismissUnexpectedDialog.start(50);
        MainWindow window;
        window.findChild<QLineEdit *>(QStringLiteral("enginePath"))->setText(qEnvironmentVariable("MDOW_TEST_ENGINE"));
        window.findChild<QLineEdit *>(QStringLiteral("outputPath"))->setText(settings.path() + QStringLiteral("/downloads"));
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("tasks"));
        const int before = table->rowCount();
        window.findChild<QPlainTextEdit *>(QStringLiteral("links"))->setPlainText(QStringLiteral(
            "https://twitter.com/example/status/222?s=20\nhttps://x.com/example/status/222"));
        window.show();
        QTest::mouseClick(window.findChild<QPushButton *>(QStringLiteral("parse")), Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), before + 2, 5000);
        QTRY_COMPARE(table->item(before, 3)->text(), QStringLiteral("就绪"));
        QCOMPARE(table->item(before + 1, 0)->text(), QStringLiteral("second video"));
        auto *format = qobject_cast<QComboBox *>(table->cellWidget(before, 2));
        QVERIFY(format); QVERIFY(format->count() >= 4);
        format->setCurrentIndex(2);
        window.grab().save(QCoreApplication::applicationDirPath() + QStringLiteral("/queue-preview.png"));
        QPushButton *download = nullptr;
        for (auto *button : window.findChildren<QPushButton *>())
            if (button->text() == QStringLiteral("下载所有就绪任务")) download = button;
        QVERIFY(download); QTest::mouseClick(download, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(table->item(before, 3)->text(), QStringLiteral("已完成"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(table->item(before + 1, 3)->text(), QStringLiteral("已完成"), 5000);
        QCOMPARE(format->currentIndex(), 2);
        QTest::mouseClick(window.findChild<QPushButton *>(QStringLiteral("clearRecords")), Qt::LeftButton);
        QCOMPARE(table->rowCount(), 0);
        window.close();
    }
    void selectedDownloadAndClearDuringActiveTask() {
        MainWindow window;
        window.findChild<QLineEdit *>(QStringLiteral("enginePath"))->setText(qEnvironmentVariable("MDOW_TEST_ENGINE"));
        window.findChild<QLineEdit *>(QStringLiteral("outputPath"))->setText(settings.path() + QStringLiteral("/downloads"));
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("tasks"));
        auto *start = window.findChild<QPushButton *>(QStringLiteral("startDownload"));
        auto *clear = window.findChild<QPushButton *>(QStringLiteral("clearRecords"));
        QVERIFY(start); QVERIFY(clear);
        QTest::mouseClick(clear, Qt::LeftButton);
        QCOMPARE(table->rowCount(), 0);
        // With no selection, the new button must not queue every row.
        QTest::mouseClick(start, Qt::LeftButton);
        window.findChild<QPlainTextEdit *>(QStringLiteral("links"))->setPlainText(QStringLiteral("https://x.com/example/status/222"));
        window.show();
        QTest::mouseClick(window.findChild<QPushButton *>(QStringLiteral("parse")), Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 2, 5000);
        QTRY_COMPARE(table->item(0, 3)->text(), QStringLiteral("就绪"));
        table->clearSelection();
        QTest::mouseClick(start, Qt::LeftButton);
        QCOMPARE(table->item(0, 3)->text(), QStringLiteral("就绪"));
        QCOMPARE(table->item(1, 3)->text(), QStringLiteral("就绪"));
        table->selectRow(0);
        QTest::mouseClick(start, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(table->item(0, 3)->text(), QStringLiteral("已完成"), 5000);
        QCOMPARE(table->item(1, 3)->text(), QStringLiteral("就绪"));
        auto *quality = qobject_cast<QComboBox *>(table->cellWidget(1, 2));
        quality->setCurrentIndex(1);
        table->selectRow(1);
        QTest::mouseClick(start, Qt::LeftButton);
        // The first row is terminal; the second is active. Clearing must remap callbacks.
        QTest::mouseClick(clear, Qt::LeftButton);
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("second video"));
        QCOMPARE(qobject_cast<QComboBox *>(table->cellWidget(0, 2))->currentIndex(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(table->item(0, 3)->text(), QStringLiteral("已完成"), 5000);
        QTest::mouseClick(clear, Qt::LeftButton);
        QCOMPARE(table->rowCount(), 0);
        window.close();
        MainWindow reopened;
        QCOMPARE(reopened.findChild<QTableWidget *>(QStringLiteral("tasks"))->rowCount(), 0);
    }
};
QTEST_MAIN(UiTests)
#include "uitests.moc"
