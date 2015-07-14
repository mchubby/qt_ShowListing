#ifndef QUALZ4FILE_H
#define QUALZ4FILE_H

#include <QBuffer>

class QuaLz4File : public QBuffer
{
    Q_OBJECT
public:
    // these are not supported nor implemented
    QuaLz4File(const QuaLz4File& that);
    QuaLz4File& operator=(const QuaLz4File& that);
public:
    /// Constructs a QuaLz4File instance.
    /** \a parent argument specifies this object's parent object.
    *
    * You should use setFileName() before
    * trying to call open() on the constructed object.
    **/
    QuaLz4File(QObject *parent = 0);
    /// Constructs a QuaLz4File instance.
    /** \a parent argument specifies this object's parent object and \a
    * fileName specifies compressed file name.
    *
    * QuaLz4File constructed by this constructor can be used for read
    * only access.
    **/
    QuaLz4File(const QString& fileName, QObject *parent = 0);

    ///
    /** To be called before open()
     **/
    void setFileName(const QString &fileName);
    /// Returns file name.
    /** This function returns file name you passed to this object either
     * by using a constructor or by calling setFileName().
     *
     * Returns blank string if there is no file name set yet.
     **/
    QString fileName() const;
    /// Opens and decompresses an LZ4 file into the internal QByteArray.
    /** Returns \c true on success, \c false otherwise.
     *
     * \note Only QIODevice::ReadOnly is supported.
     *
     * \sa QuaLz4File::isOpen
     **/
    virtual bool open(OpenMode mode);
    /// Returns compressed file size.
    /** File must be open for reading before calling this function.
     *
     * Returns -1 on error.
     **/
    qint64 csize()const;

protected:
    //QByteArray _arr;
    QString _filename;
    qint64 _compressed_size;

};

#endif // QUALZ4FILE_H
