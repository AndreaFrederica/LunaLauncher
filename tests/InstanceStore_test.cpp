#include "InstanceStore.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

class InstanceStoreTest : public QObject {
    Q_OBJECT

   private slots:
    void normalizesPortableCandidateWithoutEnablingInUserMode();
    void enablesPortableCandidateInPortableMode();
    void deduplicatesStoresByCanonicalPath();
    void roundTripsRegistry();
};

void InstanceStoreTest::normalizesPortableCandidateWithoutEnablingInUserMode()
{
    QTemporaryDir launcher;
    QVERIFY(launcher.isValid());

    InstanceStoreRegistry registry;
    registry.normalize(launcher.path(), false);

    QCOMPARE(registry.stores().size(), 1);
    const auto& store = registry.stores().constFirst();
    QCOMPARE(store.path, InstanceStoreRegistry::portableInstancesPath(launcher.path()));
    QVERIFY(!store.enabled);
    QVERIFY(store.autoEnableWhenPortable);
    QVERIFY(store.isPrism());
}

void InstanceStoreTest::enablesPortableCandidateInPortableMode()
{
    QTemporaryDir launcher;
    QVERIFY(launcher.isValid());

    InstanceStoreRegistry registry;
    registry.normalize(launcher.path(), true);

    QCOMPARE(registry.stores().size(), 1);
    const auto& store = registry.stores().constFirst();
    QVERIFY(store.enabled);
    QVERIFY(store.defaultForNewInstances);
}

void InstanceStoreTest::deduplicatesStoresByCanonicalPath()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    InstanceStore first;
    first.id = QStringLiteral("first");
    first.name = QStringLiteral("First");
    first.path = root.path();
    first.enabled = true;

    InstanceStore second;
    second.id = QStringLiteral("second");
    second.name = QStringLiteral("Second");
    second.path = QDir(root.path()).filePath(QStringLiteral("."));
    second.enabled = false;

    InstanceStoreRegistry registry;
    registry.addOrUpdate(first);
    registry.addOrUpdate(second);
    registry.normalize({}, false);

    QCOMPARE(registry.stores().size(), 1);
    QCOMPARE(registry.stores().constFirst().id, QStringLiteral("first"));
    QVERIFY(registry.stores().constFirst().enabled);
}

void InstanceStoreTest::roundTripsRegistry()
{
    InstanceStoreRegistry registry;
    InstanceStore store;
    store.id = QStringLiteral("hmcl");
    store.name = QStringLiteral("HMCL");
    store.path = QStringLiteral("D:/Projects/HMCL");
    store.format = InstanceStoreFormat::VanillaCompatible;
    store.origin = InstanceStoreOrigin::User;
    store.enabled = true;
    store.writable = false;
    registry.addOrUpdate(store);

    QString error;
    const auto restored = InstanceStoreRegistry::fromJson(registry.toJson(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(restored.stores().size(), 1);
    const auto& actual = restored.stores().constFirst();
    QCOMPARE(actual.id, store.id);
    QCOMPARE(actual.name, store.name);
    QCOMPARE(actual.path, store.path);
    QVERIFY(actual.isVanillaCompatible());
    QVERIFY(!actual.writable);
}

QTEST_GUILESS_MAIN(InstanceStoreTest)
#include "InstanceStore_test.moc"

