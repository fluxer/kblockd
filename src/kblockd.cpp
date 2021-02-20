#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QDBusError>
#include <QDBusConnection>
#include <QDBusAbstractAdaptor>
#include <QDBusMetaType>

#include "kdiskmanager.hpp"

class KBlockdInterfaceAdaptor: public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.kblockd.Block")
    Q_CLASSINFO("D-Bus Introspection",
"  <interface name=\"com.kblockd.Block\">\n"
"    <property name=\"disks\" type=\"a(ssssii)\" access=\"read\"/>\n"
"    <property name=\"supported\" type=\"a(s)\" access=\"read\"/>\n"
"    <method name=\"rescan\">\n"
"      <arg name=\"result\" type=\"b\" direction=\"out\"/>\n"
"    </method>\n"
"    <method name=\"info\">\n"
"      <arg name=\"result\" type=\"(ssssii)\" direction=\"out\"/>\n"
"      <arg name=\"disk\" type=\"s\" direction=\"in\"/>\n"
"      <annotation name=\"org.qtproject.QtDBus.QtTypeName.Out0\" value=\"KDiskInfo\"/>\n"
"    </method>\n"
"    <method name=\"unmount\">\n"
"      <arg name=\"result\" type=\"b\" direction=\"out\"/>\n"
"      <arg name=\"disk\" type=\"s\" direction=\"in\"/>\n"
"    </method>\n"
"    <method name=\"mount\">\n"
"      <arg name=\"result\" type=\"b\" direction=\"out\"/>\n"
"      <arg name=\"disk\" type=\"s\" direction=\"in\"/>\n"
"    </method>\n"
"  </interface>\n")
    Q_PROPERTY(QList<KDiskInfo> disks READ disks)
    Q_PROPERTY(QStringList supported READ supported)

    public:
        KBlockdInterfaceAdaptor(QObject *parent);
        ~KBlockdInterfaceAdaptor();

        QList<KDiskInfo> disks() const;
        QStringList supported() const;

    public Q_SLOTS:
        bool rescan() const;
        KDiskInfo info(const QString &disk) const;
        bool mount(const QString &disk) const;
        bool unmount(const QString &disk) const;
};

KBlockdInterfaceAdaptor::KBlockdInterfaceAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent) {
}

KBlockdInterfaceAdaptor::~KBlockdInterfaceAdaptor() {
}

QList<KDiskInfo> KBlockdInterfaceAdaptor::disks() const {
    return KDiskManager::disks();
}

QStringList KBlockdInterfaceAdaptor::supported() const {
    return KDiskManager::supported();
}

bool KBlockdInterfaceAdaptor::rescan() const {
    return KDiskManager::rescan();
}

KDiskInfo KBlockdInterfaceAdaptor::info(const QString &disk) const {
    return KDiskManager::info(disk);
}

bool KBlockdInterfaceAdaptor::mount(const QString &disk) const {
    const KDiskInfo info = KDiskManager::info(disk);
    return KDiskManager::mount(info);
}

bool KBlockdInterfaceAdaptor::unmount(const QString &disk) const {
    const KDiskInfo info = KDiskManager::info(disk);
    return KDiskManager::unmount(info);
}


int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    app.setApplicationName("kblockd");

    qRegisterMetaType<KDiskInfo>();
    qRegisterMetaType<QList<KDiskInfo> >();
    qDBusRegisterMetaType<KDiskInfo>();
    qDBusRegisterMetaType<QList<KDiskInfo> >();

    new KBlockdInterfaceAdaptor(&app);
    QDBusConnection connection = QDBusConnection::systemBus();
    const bool object = connection.registerObject("/com/kblockd/Block", &app);
    if (object) {
        const bool service = connection.registerService("com.kblockd.Block");
        if (service) {
            qDebug() << "kblockd is online";
            return app.exec();
        } else {
            qWarning() << "could not register object" << connection.lastError().message();
            return 2;
        }
    } else {
        qWarning() << "could not register object" << connection.lastError().message();
        return 1;
    }
    return 3;
}

#include "kblockd.moc"
