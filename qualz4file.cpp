#include "qualz4file.h"

#include <QFile>
#include <QtEndian>
#include "lz4.h"

QuaLz4File::QuaLz4File(QObject *parent) :
    QBuffer(parent), _compressed_size(-1)
{
}

QuaLz4File::QuaLz4File(const QString& fileName, QObject *parent) :
    QBuffer(parent), _filename(fileName), _compressed_size(-1)
{
}

void QuaLz4File::setFileName(const QString &fileName)
{
    _filename = fileName;
}

QString QuaLz4File::fileName() const
{
    return _filename;
}

bool QuaLz4File::open(OpenMode openMode)
{
    if(_filename.isEmpty()) { return false; }
    QFile file(_filename);
    if (!file.open(QFile::ReadOnly)) { return false; }
    qint64 assumedcompressed_size = file.size() - 8; if(assumedcompressed_size < 4)  { return false; }
    QByteArray carray = file.readAll();
    qint32 declaredUncompressedSize = qFromLittleEndian<qint32>(*reinterpret_cast<const qint32*>(carray.constData()));
    carray.remove(0, 4);
    qint32 declaredCompressedSize = qFromLittleEndian<qint32>(*reinterpret_cast<const qint32*>(carray.constData()));
    carray.remove(0, 4);
    if (declaredCompressedSize == 0 || declaredUncompressedSize == 0 || declaredCompressedSize > assumedcompressed_size) { return false; }
    _compressed_size = declaredCompressedSize;  // assume trailing data is garbage
#if 1
    // size check for our specific purposes
    if (declaredUncompressedSize > (1LL << 30)) { _compressed_size = -1; return false; }
#endif

    // assume it is a simple copy if sizes match (uncompressible)
    if (declaredCompressedSize == declaredUncompressedSize) {
        carray.resize(declaredCompressedSize);
        setData(carray);
        return QBuffer::open(openMode);
    }
    carray.resize(declaredCompressedSize);
    QByteArray darray;
    darray.reserve(declaredUncompressedSize);

    int const decSize = LZ4_decompress_safe(carray.data(), darray.data(), declaredCompressedSize, declaredUncompressedSize);
    if(decSize < 0) { _compressed_size = -1; return false; }
    darray.resize(decSize);
    setData(darray);
    return QBuffer::open(openMode);
}
qint64 QuaLz4File::csize() const
{
    if (_filename.isEmpty() || !isOpen()) return -1;
    return _compressed_size;
}
