#ifndef GPXTRIM_H
#define GPXTRIM_H

#include <QDateTime>
#include <QString>

class QDomDocument;

class GPXTrim
{
public:
	static bool range(const QString &path, QDateTime &first,
	  QDateTime &last, QString &error);
	static bool save(const QString &source, const QString &target,
	  const QDateTime &first, const QDateTime &last, QString &error,
	  const QString &name = QString());

private:
	static bool read(const QString &path, QDomDocument &document,
	  QDateTime &first, QDateTime &last, QString &error);
};

#endif // GPXTRIM_H
