#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTimerEvent>
#include <QStandardPaths>
#include <QProcess>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMetaType>

#include "kdiskmanager.hpp"

#include <libudev.h>
#include <sys/mount.h>
#include <errno.h>

static const QStringList s_knownfstypes = QStringList()
        << "ext2"
        << "ext3"
        << "ext4"
        << "jfs"
        << "xfs"
        << "btrfs"
        << "ntfs"
        << "vfat"
        << "minix"
        << "reiserfs";

KDiskInfo::KDiskInfo()
    : size(0),
    type(KDiskType::None) {
}

KDiskInfo::KDiskInfo(const KDiskInfo &info)
    : name(info.name),
    label(info.label),
    fstype(info.fstype),
    fsuuid(info.fsuuid),
    size(info.size),
    type(info.type) {
}

QString KDiskInfo::fancyName() const {
    if (!label.isEmpty()) {
        return label + " (" + fancySize() + ")";
    }
    return fsuuid + " (" + fancySize() + ")";
}

QString KDiskInfo::fancySize() const {
    if (size < 1) {
        return QString("unknown");
    }

    if (size < 999) {
        return QString("%1 Kb").arg(size);
    } else if (size < 999999) {
        return QString("%1 Mb").arg(size / 1000);
    } else {
        return QString("%1 Gb").arg(size / 1000000);
    }

    Q_UNREACHABLE();
    return QString();
}

QString KDiskInfo::fancyType() const {
    switch (type) {
        case KDiskType::None: {
            return "None";
        }
        case KDiskType::Disk: {
            return "Disk";
        }
        case KDiskType::Partition: {
            return "Partition";
        }
    }

    Q_UNREACHABLE();
    return QString();
}

bool KDiskInfo::isNull() const {
    if (name.isEmpty() || fsuuid.isEmpty() || type == KDiskType::None) {
        return true;
    }
    return false;
}

bool KDiskInfo::operator==(const KDiskInfo &info) const {
    return name == info.name;
}

KDiskInfo& KDiskInfo::operator=(const KDiskInfo &info) {
    name = info.name;
    label = info.label;
    fstype = info.fstype;
    fsuuid = info.fsuuid;
    size = info.size;
    type = info.type;
    return *this;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const KDiskInfo &disk)
{
    d << "KDiskInfo( name:" << disk.name
        << ", label:" << disk.label
        << ", fstype:" << disk.fstype
        << ", fsuuid:" << disk.fsuuid
        << ", size:" << disk.fancySize()
        << ", type:" << disk.fancyType()
        << ")";
    return d;
}
#endif

const QDBusArgument &operator<<(QDBusArgument &argument, const KDiskInfo &disk) {
    argument.beginStructure();
    argument << QString(disk.name);
    argument << QString(disk.label);
    argument << QString(disk.fstype);
    argument << QString(disk.fsuuid);
    argument << disk.size;
    argument << int(disk.type);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, KDiskInfo &disk) {
    int typebuff;
    argument.beginStructure();
    argument >> disk.name;
    argument >> disk.label;
    argument >> disk.fstype;
    argument >> disk.fsuuid;
    argument >> disk.size;
    argument >> typebuff;
    disk.type = KDiskInfo::KDiskType(typebuff);
    argument.endStructure();

    return argument;
}

class KDiskManagerPrivate : public QObject {
    Q_OBJECT

    public:
        KDiskManagerPrivate(QObject *parent = Q_NULLPTR);
        ~KDiskManagerPrivate();

        QList<KDiskInfo> m_disks;

        KDiskInfo info(const QString &disk);
        bool call(const QString &method, const QString &argument);

    Q_SIGNALS:
        void addedDisk(const KDiskInfo &disk);
        void changedDisk(const KDiskInfo &disk);
        void removedDisk(const KDiskInfo &disk);

    protected:
        // reimplementation
        void timerEvent(QTimerEvent *event);

    private:
        udev *m_udev;
        udev_monitor *m_monitor;
        QDBusInterface *m_interface;
};
Q_GLOBAL_STATIC(KDiskManagerPrivate, diskManager);

KDiskManagerPrivate::KDiskManagerPrivate(QObject *parent)
    : QObject(parent),
    m_udev(Q_NULLPTR),
    m_monitor(Q_NULLPTR),
    m_interface(Q_NULLPTR) {
    qRegisterMetaType<KDiskInfo>();
    qRegisterMetaType<QList<KDiskInfo> >();
    qDBusRegisterMetaType<KDiskInfo>();
    qDBusRegisterMetaType<QList<KDiskInfo> >();

    m_udev = udev_new();
    if (m_udev) {
        m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");

        if (m_monitor) {
            udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "block", Q_NULLPTR);
            udev_monitor_enable_receiving(m_monitor);
        }


        const QDir dir("/sys/class/block");
        foreach (const QString &entry, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const KDiskInfo di = info(entry);
            if (!di.isNull()) {
                m_disks.append(di);
            }
        }
    }

    if (!m_udev || !m_monitor) {
        qWarning() << "could not setup disk monitor";
    } else {
        startTimer(1000);
    }

    const QDBusConnection connection = QDBusConnection::systemBus();
    if (connection.isConnected()) {
        m_interface = new QDBusInterface("com.kblockd.Block",
            "/com/kblockd/Block", "com.kblockd.Block", connection);
    } else {
        qWarning() << "Cannot connect to the D-Bus session bus";
    }
}

KDiskManagerPrivate::~KDiskManagerPrivate() {
    if (m_interface) {
        m_interface->deleteLater();
    }

    udev_unref(m_udev);
    udev_monitor_unref(m_monitor);
}

KDiskInfo KDiskManagerPrivate::info(const QString &disk) {
    KDiskInfo result;

    if (!m_udev) {
        qWarning() << "cannot get info for device because no udev";
        return result;
    }

    const QFileInfo devinfo = QFileInfo(disk);
    const QByteArray path = "/sys/class/block/" + devinfo.fileName().toUtf8();

    udev_device *dev = udev_device_new_from_syspath(m_udev, path);
    if (dev) {
        result.name = udev_device_get_property_value(dev, "DEVNAME");
        result.label = udev_device_get_property_value(dev, "ID_FS_LABEL");
        result.fstype = udev_device_get_property_value(dev, "ID_FS_TYPE");
        result.fsuuid = udev_device_get_property_value(dev, "ID_FS_UUID");
        const QByteArray devtype = udev_device_get_property_value(dev, "DEVTYPE");
        if (devtype == "disk") {
            result.type = KDiskInfo::KDiskType::Disk;
        } else if (devtype == "partition") {
            result.type = KDiskInfo::KDiskType::Partition;
        }
        const QByteArray size = udev_device_get_property_value(dev, "ID_PART_ENTRY_SIZE");
        result.size = size.toInt() / 2;
    } else {
        qWarning() << "cannot get info for device because no dev for" << disk;
    }

    udev_device_unref(dev);

    return result;
}

bool KDiskManagerPrivate::call(const QString &method, const QString &argument) {
    if (m_interface && m_interface->isValid()) {
        const QDBusMessage result = m_interface->call(method, argument);
        QDBusReply<bool> reply = QDBusReply<bool>(result);
        if (reply.isValid()) {
            return reply.value();
        } else {
            qWarning() << reply.error().message();
            return false;
        }
    } else {
        qWarning() << QDBusConnection::systemBus().lastError().message();
        return false;
    }
}

void KDiskManagerPrivate::timerEvent(QTimerEvent *event) {
    udev_device *dev = udev_monitor_receive_device(m_monitor);
    while (dev) {
        const char* name = udev_device_get_property_value(dev, "DEVNAME");
        const char* action = udev_device_get_action(dev);

        if (qstrcmp(action, "add") == 0) {
            const KDiskInfo info = KDiskManagerPrivate::info(name);
            if (!info.isNull()) {
                qDebug() << "added" << name;
                m_disks.append(info);
                emit addedDisk(info);
            }
        } else if (qstrcmp(action, "change") == 0) {
            const KDiskInfo info = KDiskManagerPrivate::info(name);
            if (!info.isNull()) {
                qDebug() << "changed" << name;
                m_disks.removeAll(info);
                m_disks.append(info);
                emit changedDisk(info);
            }
        } else if (qstrcmp(action, "remove") == 0) {
            /*
                reusing disk info from already tracked disks since info cannot be obtained once
                the device is gone
            */
            foreach (const KDiskInfo &info, m_disks) {
                if (info.name == name) {
                    qDebug() << "removed" << name;
                    m_disks.removeAll(info);
                    emit removedDisk(info);
                    break;
                }
            }
        } else if (qstrcmp(action, "bind") != 0 && qstrcmp(action, "unbind") != 0) {
            // bind/unbind are driver changing for device type of event
            qWarning() << "unknown action" << action;
        }

        dev = udev_monitor_receive_device(m_monitor);
    }
    udev_device_unref(dev);

    event->ignore();
}

KDiskManager::KDiskManager(QObject *parent)
    : QObject(parent) {
    connect(diskManager(), SIGNAL(addedDisk(KDiskInfo)),
        this, SLOT(emitAdded(KDiskInfo)));
    connect(diskManager(), SIGNAL(changedDisk(KDiskInfo)),
        this, SLOT(emitChanged(KDiskInfo)));
    connect(diskManager(), SIGNAL(removedDisk(KDiskInfo)),
        this, SLOT(emitRemoved(KDiskInfo)));
}

QStringList KDiskManager::supported() {
    QStringList result;
    foreach (const QString &fstype, s_knownfstypes) {
        if (!QStandardPaths::findExecutable("fsck." + fstype).isEmpty()
            && !QStandardPaths::findExecutable("mkfs." + fstype).isEmpty()) {
            result << fstype;
        }
    }
    if (!QStandardPaths::findExecutable("mkswap").isEmpty()) {
        result << "swap";
    }
    return result;
}

QList<KDiskInfo> KDiskManager::disks() {
    return diskManager()->m_disks;
}

KDiskInfo KDiskManager::info(const QString &disk) {
    return diskManager()->info(disk);
}

bool KDiskManager::mounted(const QString &disk) {
    return !mountpoint(disk).isEmpty();
}

QString KDiskManager::mountpoint(const QString &disk) {
    QString result;

    QFile mounts("/proc/mounts");
    if (!mounts.open(QFile::ReadOnly)) {
        qWarning() << "cannot open /proc/mounts";
        return result;
    }

    const QRegExp pattern = QRegExp("^" + disk + "\\s");
    while (true) {
        const QString line = mounts.readLine();
        if (pattern.indexIn(line) >= 0) {
            result = line.split(" ").at(1);
        }

        if (mounts.atEnd()) {
            break;
        }
    }

    return result;
}

bool KDiskManager::rescan() {
    qDebug() << "scanning for disk changes";

    // partprobe is part of parted
    const QString partprobe = QStandardPaths::findExecutable("partprobe");
    // partx is part of util-linux
    const QString partx = QStandardPaths::findExecutable("partx");
    foreach (const KDiskInfo &disk, disks()) {
        if (disk.type == KDiskInfo::KDiskType::Partition) {
            continue;
        }
        if (!partprobe.isEmpty()) {
            QProcess partproc;
            partproc.start(partprobe, QStringList() << disk.name);
            partproc.waitForFinished(-1);
            if (partproc.exitCode() != 0) {
                qWarning() << partproc.readAllStandardError();
                return false;
            }
        } else if (!partx.isEmpty()) {
            QProcess partxproc;
            partxproc.start(partx, QStringList() << "-u" << disk.name);
            partxproc.waitForFinished(-1);
            if (partxproc.exitCode() != 0) {
                qWarning() << partxproc.readAllStandardError();
                return false;
            }
        } else {
            const QFileInfo devinfo = QFileInfo(disk.name);
            const QString rescanpath = "/sys/block/" + devinfo.fileName() + "/device/rescan";
            QFile rescanfile(rescanpath);
            if (rescanfile.open(QFile::WriteOnly)) {
                rescanfile.write("1");
                rescanfile.close();
            } else {
                qWarning() << "could not open rescan file" << rescanpath;
                return false;
            }
        }
    }

    return true;
}

bool KDiskManager::fsck(const KDiskInfo &disk) {
    if (disk.isNull()) {
        qWarning() << "invalid disk" << disk;
        return false;
    }

    if (mounted(disk.name)) {
        qWarning() << "device is mounted" << disk;
        return false;
    }

    qDebug() << "checking" << disk;
    QProcess fsckproc;
    fsckproc.start("fsck", QStringList() << "-p" << disk.name);
    fsckproc.waitForStarted(-1);
    while (fsckproc.state() == QProcess::Running) {
        QCoreApplication::processEvents();
    }
    if (fsckproc.exitCode() != 0) {
        qWarning() << fsckproc.readAllStandardError();
        return false;
    }

    return true;
}

bool KDiskManager::mount(const KDiskInfo &disk, const QString &directory) {
    if (disk.isNull()) {
        qWarning() << "invalid disk" << disk;
        return false;
    }

    if (mounted(disk.name)) {
        qDebug() << "already mounted" << disk;
        return true;
    }

    QByteArray mountdir = directory.toUtf8();
    if (mountdir.isEmpty()) {
        mountdir = "/mnt/" + disk.fsuuid;
    }

    const QFileInfo mountinfo = QFileInfo(mountdir);
    if (!mountinfo.exists() && !mountinfo.dir().mkdir(mountinfo.fileName())) {
        qWarning() << "could not create mount point" << mountdir;
        return false;
    }

    qDebug() << "mounting" << disk << "to" << mountdir;
    const int rv = ::mount(disk.name.constData(), mountdir.constData(), disk.fstype.constData(), 0, Q_NULLPTR);
    if (rv != 0) {
        qWarning() << qt_error_string(errno);
        return false;
    }

    return true;
}

bool KDiskManager::unmount(const KDiskInfo &disk) {
    if (disk.isNull()) {
        qWarning() << "invalid disk" << disk;
        return false;
    }

    const QByteArray mountdir = mountpoint(disk.name).toUtf8();
    if (mountdir.isEmpty()) {
        qDebug() << "not mounted" << disk;
        return true;
    }

    qDebug() << "unmounting" << disk;
    const int rv = ::umount2(mountdir.constData(), MNT_DETACH);
    if (rv != 0) {
        qWarning() << qt_error_string(errno);
        return false;
    }

    return true;
}

bool KDiskManager::mkfs(const KDiskInfo &disk, const QString &fstype) {
    if (disk.isNull()) {
        qWarning() << "invalid disk" << disk;
        return false;
    } else if (!supported().contains(fstype)) {
        qWarning() << "invalid filesystem type" << fstype;
        return false;
    }

    if (mounted(disk.name)) {
        qWarning() << "device is mounted" << disk;
        return false;
    }

    qDebug() << "formatting" << disk;
    QString program = "mkfs." + fstype;
    if (fstype == "swap") {
        program = "mkswap";
    }
    QProcess mkfsproc;
    mkfsproc.start(program, QStringList() << disk.name);
    mkfsproc.waitForStarted(-1);
    while (mkfsproc.state() == QProcess::Running) {
        QCoreApplication::processEvents();
    }
    if (mkfsproc.exitCode() != 0) {
        qWarning() << mkfsproc.readAllStandardError();
        return false;
    }

    return true;
}

bool KDiskManager::userMount(const KDiskInfo &disk) {
    qDebug() << "user mounting" << disk.name;

    return diskManager()->call("mount", disk.name);
}

bool KDiskManager::userUnmount(const KDiskInfo &disk) {
    qDebug() << "user unmounting" << disk.name;

    return diskManager()->call("unmount", disk.name);
}

void KDiskManager::emitAdded(const KDiskInfo &disk) {
    emit added(disk);
}

void KDiskManager::emitChanged(const KDiskInfo &disk) {
    emit changed(disk);
}

void KDiskManager::emitRemoved(const KDiskInfo &disk) {
    emit removed(disk);
}

#include "kdiskmanager.moc"
