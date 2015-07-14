/*
    This file is part of procinfo-NG

    procinfo-NG/routines.cpp is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; version 2.1.

    procinfo-NG is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with procinfo-NG; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Procinfo-NG is Copyright tabris@tabris.net 2007, 2008, 2009

#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QLocale>


static inline const QString double2StringPrecision(const qreal &input, const int precision) {
    return QLocale::system().toString(input, 'f', precision);
}

static inline const QString humanizeBigNums(const qulonglong &val, const int precision) {
    int shiftVal = 0;
    QString suffix;
    if(val >= (1ULL << 60)) {
        shiftVal = 60;
        suffix = " EiB";
    }
    else if(val >= (1ULL << 50)) {
        shiftVal = 50;
        suffix = " PiB";
    }
    else if(val >= (1ULL << 40)) {
        shiftVal = 40;
        suffix = " TiB";
    }
    else if(val >= (1ULL << 25)) {
        shiftVal = 30;
        suffix = " GiB";
    }
    else if(val >= (1ULL << 20)) {
        return "(-) MiB";
    }
    else {
        return "-";
    }
    return double2StringPrecision(qreal(val) / (1ULL << shiftVal), precision) + suffix;
}
#endif // UTIL_H
