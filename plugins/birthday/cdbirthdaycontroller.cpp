/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

#include "cdbirthdaycontroller.h"
#include "cdbirthdaycalendar.h"
#include "debug.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>

#include <cubi.h>
#include <ontologies.h>

#include <QContactBirthday>
#include <QContactDetailFilter>
#include <QContactFetchRequest>
#include <QContactLocalIdFilter>


QTM_USE_NAMESPACE

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

using namespace Contactsd;

// The logic behind this class was strongly influenced by QctTrackerChangeListener in
// qtcontacts-tracker.
CDBirthdayController::CDBirthdayController(QSparqlConnection &connection,
                                           QObject *parent)
    : QObject(parent)
    , mSparqlConnection(connection)
    , mCalendar(0)
    , mManager(0)
{
    const QLatin1String trackerManagerName = QLatin1String("tracker");

    mManager = new QContactManager(trackerManagerName, QMap<QString, QString>(), this);

    if (mManager->managerName() != trackerManagerName) {
        debug() << Q_FUNC_INFO << "Tracker plugin not found";
        return;
    }

    fetchTrackerIds();

    if (not stampFileExists()) {
        // Delete the calendar database.
        mCalendar = new CDBirthdayCalendar(true, this);

        updateAllBirthdays();
    } else {
        // Use the existing calendar database.
        mCalendar = new CDBirthdayCalendar(false, this);
    }
}

CDBirthdayController::~CDBirthdayController()
{
}

void
CDBirthdayController::fetchTrackerIds()
{
    // keep in sync with the enum in the header and NTrackerIds
    const QList<ResourceValue> resources = QList<ResourceValue>()
                                           << nco::birthDate::resource()
                                           << rdf::type::resource()
                                           << nco::PersonContact::resource()
                                           << nco::ContactGroup::resource();

    Select select;

    foreach (const ResourceValue &value, resources) {
        select.addProjection(Functions::trackerId.apply(value));
    }

    if (not mSparqlConnection.isValid()) {
        debug() << Q_FUNC_INFO << "SPARQL connection is not valid";
        return;
    }

    QScopedPointer<QSparqlResult> result(mSparqlConnection.exec(QSparqlQuery(select.sparql())));

    if (result->hasError()) {
        debug() << Q_FUNC_INFO << "Could not fetch Tracker IDs:" << result->lastError().message();
        return;
    }

    connect(result.take(), SIGNAL(finished()), this, SLOT(onTrackerIdsFetched()));
}

bool CDBirthdayController::stampFileExists()
{
    const QFile cacheFile(stampFilePath(), this);

    return cacheFile.exists();
}

void CDBirthdayController::createStampFile()
{
    QFile cacheFile(stampFilePath(), this);

    if (not cacheFile.exists()) {
        if (not cacheFile.open(QIODevice::WriteOnly)) {
            warning() << Q_FUNC_INFO << "Unable to create birthday plugin stamp file "
                                     << cacheFile.fileName() << " with error " << cacheFile.errorString();
        } else {
            cacheFile.close();
        }
    }
}

QString CDBirthdayController::stampFilePath() const
{
    QString cacheLocation = QDir::cleanPath(QDesktopServices::storageLocation(QDesktopServices::CacheLocation));

    if (cacheLocation.isEmpty()) {
        cacheLocation = QDir::home().filePath(QLatin1String(".cache"));
    }

    return cacheLocation.append(QLatin1String("/contactsd/calendar.stamp"));
}

void
CDBirthdayController::updateAllBirthdays()
{
    QContactFetchHint fetchHint;
    static const QStringList detailDefinitions = QStringList() << QContactBirthday::DefinitionName
                                                               << QContactDisplayLabel::DefinitionName;
    fetchHint.setDetailDefinitionsHint(detailDefinitions);

    // Filter on any contact with a birthday.
    QContactDetailFilter fetchFilter;
    fetchFilter.setDetailDefinitionName(QContactBirthday::DefinitionName);

    QContactFetchRequest * const fetchRequest = new QContactFetchRequest(this);
    fetchRequest->setManager(mManager);
    fetchRequest->setFetchHint(fetchHint);
    fetchRequest->setFilter(fetchFilter);

    connect(fetchRequest,
            SIGNAL(stateChanged(QContactAbstractRequest::State)),
            SLOT(onFullSyncRequestStateChanged(QContactAbstractRequest::State)));

    if (not fetchRequest->start()) {
        warning() << Q_FUNC_INFO << "Unable to start contact birthdays fetch request";
        delete fetchRequest;
        return;
    }

    debug() << "Contact birthdays fetch request started";
}

void
CDBirthdayController::onTrackerIdsFetched()
{
    QSparqlResult *result = qobject_cast<QSparqlResult*>(sender());

    if (result == 0) {
        debug() << Q_FUNC_INFO << "Invalid result";
        return;
    }

    // We will delete the result anyway
    result->deleteLater();

    if (result->hasError()) {
        debug() << Q_FUNC_INFO << "Could not fetch Tracker IDs:" << result->lastError().message();
        return;
    }

    if (not result->next()) {
        debug() << Q_FUNC_INFO << "No results returned";
        return;
    }

    const QSparqlResultRow row = result->current();

    for (int i = 0; i < NTrackerIds; ++i) {
        mTrackerIds[i] = row.value(i).toInt();
    }

    debug() << Q_FUNC_INFO << "Tracker IDs fetched, connecting the change notifier";

    connectChangeNotifier();
}

void
CDBirthdayController::connectChangeNotifier()
{
    const QStringList contactClassIris = QStringList()
                                      << nco::PersonContact::iri()
                                      << nco::ContactGroup::iri();

    foreach (const QString &iri, contactClassIris) {
        connect(new TrackerChangeNotifier(iri, this),
            SIGNAL(changed(QList<TrackerChangeNotifier::Quad>,
                           QList<TrackerChangeNotifier::Quad>)),
            SLOT(onGraphChanged(QList<TrackerChangeNotifier::Quad>,
                                QList<TrackerChangeNotifier::Quad>)));
    }
}

void CDBirthdayController::onGraphChanged(const QList<TrackerChangeNotifier::Quad>& deletions,
                                          const QList<TrackerChangeNotifier::Quad>& insertions)
{
    mDeleteNotifications += deletions;
    mInsertNotifications += insertions;

    if (isDebugEnabled()) {
        TrackerChangeNotifier const * const notifier = qobject_cast<TrackerChangeNotifier *>(sender());

        if (notifier == 0) {
            warning() << Q_FUNC_INFO << "Error casting birthday change notifier";
            return;
        }

        debug() << notifier->watchedClass() << "birthday: deletions:" << deletions;
        debug() << notifier->watchedClass() << "birthday: insertions:" << insertions;
    }

    processNotificationQueues();
}

void
CDBirthdayController::onFullSyncRequestStateChanged(QContactAbstractRequest::State newState)
{
    QContactFetchRequest * const fetchRequest = qobject_cast<QContactFetchRequest*>(sender());

    if (not processFetchRequest(fetchRequest, newState)) {
        return;
    }

    updateBirthdays(fetchRequest->contacts());

    // Create the stamp file only after a successful full sync.
    createStampFile();
}

void
CDBirthdayController::onFetchRequestStateChanged(QContactAbstractRequest::State newState)
{
    QContactFetchRequest * const fetchRequest = qobject_cast<QContactFetchRequest*>(sender());

    if (not processFetchRequest(fetchRequest, newState)) {
        return;
    }

    updateBirthdays(fetchRequest->contacts());
}

bool
CDBirthdayController::processFetchRequest(QContactFetchRequest * const fetchRequest,
                                          QContactAbstractRequest::State newState)
{
    if (fetchRequest == 0) {
        warning() << Q_FUNC_INFO << "Invalid fetch request";
        return false;
    }

    switch (newState) {
    case QContactAbstractRequest::FinishedState:
        break;
    case QContactAbstractRequest::CanceledState:
        fetchRequest->deleteLater();
        return false;
    default:
        return false;
    }

    fetchRequest->deleteLater();

    debug() << "Birthday contacts fetch request finished";

    if (fetchRequest->error() != QContactManager::NoError) {
        warning() << Q_FUNC_INFO << "Error during birthday contact fetch request, code: "
                  << fetchRequest->error();
        return false;
    }

    return true;
}

void
CDBirthdayController::processNotificationQueues()
{
    QSet<QContactLocalId> insertedContacts;
    QSet<QContactLocalId> deletedContacts;
    QSet<QContactLocalId> birthdayChangedIds;

    // Process notification queues to determine contacts with changed birthdays.
    processNotifications(mDeleteNotifications, birthdayChangedIds, deletedContacts);
    processNotifications(mInsertNotifications, birthdayChangedIds, insertedContacts);

    if (isDebugEnabled()) {
        debug() << "changed birthdates: " << birthdayChangedIds.count() << birthdayChangedIds;
    }

    // Remove the birthdays for contacts that are not there anymore
    foreach (QContactLocalId id, deletedContacts) {
        mCalendar->deleteBirthday(id);
    }

    // Update the calendar with the birthday changes.
    if (not birthdayChangedIds.isEmpty()) {
        fetchContacts(birthdayChangedIds.toList());
    }
}

void
CDBirthdayController::processNotifications(QList<TrackerChangeNotifier::Quad> &notifications,
                                           QSet<QContactLocalId> &propertyChanges,
                                           QSet<QContactLocalId> &resourceChanges)
{
    foreach (const TrackerChangeNotifier::Quad &quad, notifications) {
        if (quad.predicate == mTrackerIds[NcoBirthDate]) {
            propertyChanges += quad.subject;
            continue;
        }

        if (quad.predicate == mTrackerIds[RdfType]) {
            if (quad.object == mTrackerIds[NcoPersonContact]
             || quad.object == mTrackerIds[NcoContactGroup]) {
            resourceChanges += quad.subject;
            }
        }
    }

    // Remove the processed notifications.
    notifications.clear();
}

void
CDBirthdayController::fetchContacts(const QList<QContactLocalId> &contactIds)
{
    QContactFetchHint fetchHint;
    static const QStringList detailDefinitions = QStringList() << QContactBirthday::DefinitionName
                                                               << QContactDisplayLabel::DefinitionName;
    fetchHint.setDetailDefinitionsHint(detailDefinitions);

    QContactLocalIdFilter fetchFilter;
    fetchFilter.setIds(contactIds);

    QContactFetchRequest * const fetchRequest = new QContactFetchRequest(this);
    fetchRequest->setManager(mManager);
    fetchRequest->setFetchHint(fetchHint);
    fetchRequest->setFilter(fetchFilter);

    connect(fetchRequest,
            SIGNAL(stateChanged(QContactAbstractRequest::State)),
            SLOT(onFetchRequestStateChanged(QContactAbstractRequest::State)));

    if (not fetchRequest->start()) {
        warning() << Q_FUNC_INFO << "Unable to start birthday contact fetch request";
        delete fetchRequest;
        return;
    }

    debug() << "Birthday contacts fetch request started";
}

void
CDBirthdayController::updateBirthdays(const QList<QContact> &changedBirthdays)
{
    foreach (const QContact &contact, changedBirthdays) {
        const QContactBirthday contactBirthday = contact.detail<QContactBirthday>();
        const QDate calendarBirthday = mCalendar->birthdayDate(contact);

        // birthDate was changed on the contact, so update the calendar.
        if (not contactBirthday.date().isNull() && contactBirthday.date() != calendarBirthday) {
            if (isDebugEnabled()) {
                debug() << "Contact with calendar birthday: " << contactBirthday.date()
                        << "changed details to: " << contact << ", so update the calendar event";
            }

            mCalendar->updateBirthday(contact);
        // birthDate was removed from the contact, so delete it from the calendar.
        } else if (contactBirthday.date().isNull() && not calendarBirthday.isNull()) {
            if (isDebugEnabled()) {
                debug() << "Contact: " << contact << " removed birthday, so delete the calendar event";
            }

            mCalendar->deleteBirthday(contact.localId());
        }
    }
}