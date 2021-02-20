#ifndef KDISKMANAGER_H
#define KDISKMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMetaType>
#include <QDBusArgument>

/*!
    Disk information holder, valid object is obtained via @p KDiskManager::info. If the device
    does not have a name, UUID or type it is not considered valid. Label and size are optional.
    The size of the device is reported in Kilobytes

    @note It is up to the programmer to keep the integrity of the structure
    @note D-Bus signature for the type is <b>(ssssii)</b>
    @ingroup Types

    @see KDiskManager
    @see https://dbus.freedesktop.org/doc/dbus-specification.html#container-types
*/
class KDiskInfo {

    public:
        enum KDiskType {
            None = 0,
            Disk = 1,
            Partition = 2,
        };

        KDiskInfo();
        KDiskInfo(const KDiskInfo &info);

        QByteArray name;
        QByteArray label;
        QByteArray fstype;
        QByteArray fsuuid;
        int size;
        KDiskType type;

        //! @brief Fancy name for the purpose of widgets
        QString fancyName() const;
        //! @brief Fancy size for the purpose of widgets
        QString fancySize() const;
        //! @brief Fancy type for the purpose of widgets
        QString fancyType() const;

        //! @brief Returns if the info is valid or not
        bool isNull() const;

        bool operator==(const KDiskInfo &i) const;
        KDiskInfo &operator=(const KDiskInfo &i);
};
#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug, const KDiskInfo &);
#endif
const QDBusArgument &operator<<(QDBusArgument &, const KDiskInfo &);
const QDBusArgument &operator>>(const QDBusArgument &, KDiskInfo &);

/*!
    Block device (disk) manager, operates mostly with with device names, e.g. /dev/sda1 and
    disk information type

    @see KDiskInfo

    @todo error reporting
*/
class KDiskManager : public QObject {
    Q_OBJECT

    public:
        KDiskManager(QObject *parent = Q_NULLPTR);

        //! @brief Returns supported filesystem fsck/mkfs types
        static QStringList supported();
        //! @brief Returns the information for all valid disks
        static QList<KDiskInfo> disks();
        //! @brief Returns the information for disk
        static KDiskInfo info(const QString &disk);
        //! @brief Returns if disk is mounted or not
        static bool mounted(const QString &disk);
        //! @brief Returns the mount point for disk, empty string if not mounted
        static QString mountpoint(const QString &disk);

        //! @brief Scan for disk changes
        static bool rescan();
        //! @brief Check disk
        static bool fsck(const KDiskInfo &disk);
        //! @brief Mount disk, default mountpoint directory is <b>/mnt/\<uuid\></b>
        static bool mount(const KDiskInfo &disk, const QString &directory = QString());
        //! @brief Unmount disk
        static bool unmount(const KDiskInfo &disk);
        //! @brief Format disk
        static bool mkfs(const KDiskInfo &disk, const QString &fstype);

        //! @brief Mount disk, does not assume adminstration priviledges
        static bool userMount(const KDiskInfo &disk);
        //! @brief Unmount disk, does not assume adminstration priviledges
        static bool userUnmount(const KDiskInfo &disk);

    Q_SIGNALS:
        //! @brief Signals a block device was added
        void added(const KDiskInfo &disk);
        //! @brief Signals a block device changed its properties
        void changed(const KDiskInfo &disk);
        //! @brief Signals a block device was removed
        void removed(const KDiskInfo &disk);

    private Q_SLOTS:
        void emitAdded(const KDiskInfo &disk);
        void emitChanged(const KDiskInfo &disk);
        void emitRemoved(const KDiskInfo &disk);
};

Q_DECLARE_METATYPE(KDiskInfo);
Q_DECLARE_METATYPE(QList<KDiskInfo>);

#endif // KDISKMANAGER_H
