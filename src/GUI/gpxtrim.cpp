#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include "gpxtrim.h"

static QString localName(const QDomElement &element)
{
	QString name = element.tagName();
	int colon = name.indexOf(':');
	return colon < 0 ? name : name.mid(colon + 1);
}

static QDomElement directChild(const QDomElement &parent, const QString &name)
{
	for (QDomElement child = parent.firstChildElement(); !child.isNull();
	  child = child.nextSiblingElement())
		if (localName(child) == name)
			return child;
	return QDomElement();
}

static bool pointTime(const QDomElement &point, QDateTime &time)
{
	QDomElement element = directChild(point, "time");
	if (element.isNull())
		return false;
	time = QDateTime::fromString(element.text().trimmed(), Qt::ISODateWithMs);
	if (!time.isValid())
		time = QDateTime::fromString(element.text().trimmed(), Qt::ISODate);
	return time.isValid();
}

bool GPXTrim::read(const QString &path, QDomDocument &document,
  QDateTime &first, QDateTime &last, QString &error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		error = file.errorString();
		return false;
	}
	int line, column;
	if (!document.setContent(&file, false, &error, &line, &column)) {
		error = QString("%1 (line %2, column %3)").arg(error)
		  .arg(line).arg(column);
		return false;
	}
	QDomElement root = document.documentElement();
	if (localName(root) != "gpx") {
		error = "Not a GPX document";
		return false;
	}
	int count = 0;
	for (QDomElement parent = root.firstChildElement(); !parent.isNull();
	  parent = parent.nextSiblingElement()) {
		QString kind = localName(parent);
		if (kind != "trk" && kind != "rte")
			continue;
		for (QDomElement container = parent.firstChildElement();
		  !container.isNull(); container = container.nextSiblingElement()) {
			if (kind == "trk" && localName(container) != "trkseg")
				continue;
			QDomElement points = kind == "trk" ? container : parent;
			for (QDomElement point = points.firstChildElement();
			  !point.isNull(); point = point.nextSiblingElement()) {
				if (localName(point) != (kind == "trk" ? "trkpt" : "rtept"))
					continue;
				QDateTime time;
				if (!pointTime(point, time)) {
					error = "Every track or route point needs a valid time";
					return false;
				}
				if (!first.isValid() || time < first)
					first = time;
				if (!last.isValid() || time > last)
					last = time;
				count++;
			}
			if (kind == "rte")
				break;
		}
	}
	if (count < 2 || first == last) {
		error = "At least two timed points are required";
		return false;
	}
	return true;
}

bool GPXTrim::range(const QString &path, QDateTime &first,
  QDateTime &last, QString &error)
{
	QDomDocument document;
	return read(path, document, first, last, error);
}

bool GPXTrim::save(const QString &source, const QString &target,
  const QDateTime &first, const QDateTime &last, QString &error,
  const QString &name)
{
	QFileInfo sourceInfo(source), targetInfo(target);
	if (sourceInfo.absoluteFilePath() == targetInfo.absoluteFilePath()
	  || (!targetInfo.canonicalFilePath().isEmpty()
	  && sourceInfo.canonicalFilePath() == targetInfo.canonicalFilePath())) {
		error = "Choose a different output file to keep the original";
		return false;
	}
	if (!first.isValid() || !last.isValid() || first >= last) {
		error = "The start must precede the end";
		return false;
	}
	QDomDocument document;
	QDateTime availableFirst, availableLast;
	if (!read(source, document, availableFirst, availableLast, error))
		return false;
	int kept = 0;
	QDomElement root = document.documentElement();
	for (QDomElement parent = root.firstChildElement(); !parent.isNull();) {
		QDomElement nextParent = parent.nextSiblingElement();
		QString kind = localName(parent);
		if (kind == "trk" || kind == "rte") {
			for (QDomElement container = parent.firstChildElement();
			  !container.isNull();) {
				QDomElement nextContainer = container.nextSiblingElement();
				if (kind == "trk" && localName(container) != "trkseg") {
					container = nextContainer;
					continue;
				}
				QDomElement points = kind == "trk" ? container : parent;
				int segmentKept = 0;
				for (QDomElement point = points.firstChildElement();
				  !point.isNull();) {
					QDomElement nextPoint = point.nextSiblingElement();
					if (localName(point) == (kind == "trk" ? "trkpt" : "rtept")) {
						QDateTime time;
						pointTime(point, time);
						if (time < first || time > last)
							points.removeChild(point);
						else {
							segmentKept++;
							kept++;
						}
					}
					point = nextPoint;
				}
				if (kind == "trk" && !segmentKept)
					parent.removeChild(container);
				if (kind == "rte")
					break;
				container = nextContainer;
			}
			if ((kind == "trk" && directChild(parent, "trkseg").isNull())
			  || (kind == "rte" && directChild(parent, "rtept").isNull()))
				root.removeChild(parent);
		}
		parent = nextParent;
	}
	if (kept < 2) {
		error = "The selected range must contain at least two points";
		return false;
	}
	if (!name.isEmpty()) {
		for (QDomElement parent = root.firstChildElement(); !parent.isNull();
		  parent = parent.nextSiblingElement()) {
			QString kind = localName(parent);
			if (kind != "trk" && kind != "rte" && kind != "metadata")
				continue;
			QDomElement element = directChild(parent, "name");
			if (element.isNull()) {
				element = document.createElement("name");
				parent.insertBefore(element, parent.firstChild());
			}
			while (!element.firstChild().isNull())
				element.removeChild(element.firstChild());
			element.appendChild(document.createTextNode(name));
		}
	}
	QSaveFile file(target);
	if (!file.open(QIODevice::WriteOnly)) {
		error = file.errorString();
		return false;
	}
	QByteArray data = document.toByteArray(2);
	if (file.write(data) != data.size() || !file.commit()) {
		error = file.errorString();
		return false;
	}
	return true;
}
