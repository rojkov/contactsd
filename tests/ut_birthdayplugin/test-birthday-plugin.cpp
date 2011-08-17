/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "test-birthday-plugin.h"

#include <extendedstorage.h>
#include <extendedcalendar.h>

#include <MGConfDataStore>
#include <MLocale>

#include <QContactBirthday>
#include <QContactName>

// A random ID, from plugins/birthday/cdbirthdaycalendar.cpp.
const QLatin1String calNotebookID("b1376da7-5555-1111-2222-227549c4e570");

static void loopWait(int ms)
{
    QTimer timer;
    timer.setInterval(ms);

    QEventLoop loop;
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start();
    loop.exec();
}

TestBirthdayPlugin::TestBirthdayPlugin(QObject *parent) :
    QObject(parent),
    mManager(0)
{
}

void TestBirthdayPlugin::init()
{
    const QLatin1String trackerManagerName = QLatin1String("tracker");

    mManager = new QContactManager(trackerManagerName, QMap<QString, QString>(), this);
    QVERIFY2(mManager->managerName() == trackerManagerName, "Unable to initialise QContactManager");
}

void TestBirthdayPlugin::initTestCase()
{
}

void TestBirthdayPlugin::testAddAndRemoveBirthday()
{
    const QString contactID = QUuid::createUuid().toString();
    const QDateTime contactBirthDate = QDateTime::currentDateTime();

    // Add contact with birthday to tracker.
    QContactName contactName;
    contactName.setFirstName(contactID);
    QContactBirthday contactBirthday;
    contactBirthday.setDateTime(contactBirthDate);
    QContact contact;
    contact.saveDetail(&contactName);
    contact.saveDetail(&contactBirthday);
    QVERIFY2(mManager->saveContact(&contact), "Error saving contact to tracker");

    // Wait until calendar event gets to calendar.
    loopWait(3000);

    // Open calendar database, which should have been created by the birthday plugin.
    mKCal::ExtendedCalendar::Ptr calendar =
        mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mKCal::ExtendedStorage::Ptr storage =
        mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();
    QVERIFY2(not storage->notebook(calNotebookID).isNull(), "No calendar database found");

    // Check calendar database for contact.
    QVERIFY2(storage->loadNotebookIncidences(calNotebookID), "Unable to load events from notebook");
    KCalCore::Event::List eventList = calendar->events();
    QCOMPARE(countCalendarEvents(eventList, contact), 1);

    // Delete the contact and see if the birthday is also deleted.
    QVERIFY2(mManager->removeContact(contact.localId()), "Unable to delete test contact from tracker database");

    // Wait until calendar event gets to calendar.
    loopWait(3000);

    // Search for any events in the calendar.
    QVERIFY2(storage->loadNotebookIncidences(calNotebookID), "Unable to load events from notebook");
    eventList = calendar->events();
    QCOMPARE(countCalendarEvents(eventList, contact), 0);

    // Close the calendar.
    QVERIFY2(storage->close(), "Error closing the calendar");
}

void TestBirthdayPlugin::testChangeBirthday()
{
    const QString contactID = QUuid::createUuid().toString();
    const QDateTime contactBirthDate = QDateTime::currentDateTime();

    // Add contact with birthday to tracker.
    QContactName contactName;
    contactName.setFirstName(contactID);
    QContactBirthday contactBirthday;
    contactBirthday.setDateTime(contactBirthDate);
    QContact contact;
    contact.saveDetail(&contactName);
    contact.saveDetail(&contactBirthday);
    QVERIFY2(mManager->saveContact(&contact), "Error saving contact to tracker");

    // Wait until calendar event gets to calendar.
    loopWait(3000);

    // Open calendar database.
    mKCal::ExtendedCalendar::Ptr calendar =
        mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mKCal::ExtendedStorage::Ptr storage =
        mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();
    QVERIFY2(not storage->notebook(calNotebookID).isNull(), "No calendar database found");

    // Check calendar database for contact.
    QVERIFY2(storage->loadNotebookIncidences(calNotebookID), "Unable to load events from notebook");
    KCalCore::Event::List eventList = calendar->events();
    QCOMPARE(countCalendarEvents(eventList, contact), 1);

    // Change the contact and see if the birthday is updated.
    contactBirthday.setDateTime(contactBirthDate.addDays(-1));
    contact.saveDetail(&contactBirthday);
    QVERIFY2(mManager->saveContact(&contact), "Unable to update test contact in tracker");

    // Wait until calendar event gets to calendar.
    loopWait(3000);

    // Search for any events in the calendar.
    QVERIFY2(storage->loadNotebookIncidences(calNotebookID), "Unable to load events from notebook");
    eventList = calendar->events();
    QCOMPARE(countCalendarEvents(eventList, contact), 1);

    // Close the calendar.
    QVERIFY2(storage->close(), "Error closing the calendar");
}

void TestBirthdayPlugin::testChangeName()
{
    const QString contactID = QUuid::createUuid().toString();
    const QDateTime contactBirthDate = QDateTime::currentDateTime();

    // Add contact with birthday to tracker.
    QContactName contactName;
    contactName.setFirstName(contactID);
    QContactBirthday contactBirthday;
    contactBirthday.setDateTime(contactBirthDate);
    QContact contact;
    contact.saveDetail(&contactName);
    contact.saveDetail(&contactBirthday);
    QVERIFY2(mManager->saveContact(&contact), "Error saving contact to tracker");

    // Wait until calendar event gets to calendar.
    loopWait(3000);

    // Open calendar database.
    mKCal::ExtendedCalendar::Ptr calendar =
        mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mKCal::ExtendedStorage::Ptr storage =
        mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();
    QVERIFY2(not storage->notebook(calNotebookID).isNull(), "No calendar database found");

    // Check calendar database for contact.
    QVERIFY2(storage->loadNotebookIncidences(calNotebookID), "Unable to load events from notebook");
    KCalCore::Event::List eventList = calendar->events();
    QCOMPARE(countCalendarEvents(eventList, contact), 1);

    // Change the contact name and see if the calendar is updated.
    const QString newContactID = QUuid::createUuid().toString();
    contactName.setFirstName(newContactID);
    contact.saveDetail(&contactName);
    // TODO: Should it be necessary to refetch the contact to get the synthesised displayLabel?
    contact = mManager->contact(contact.localId());
    QVERIFY2(mManager->saveContact(&contact), "Unable to update test contact in tracker");

    // Wait until calendar event gets to calendar.
    loopWait(3000);

    // Search for any events in the calendar.
    QVERIFY2(storage->loadNotebookIncidences(calNotebookID), "Unable to load events from notebook");
    eventList = calendar->events();
    QCOMPARE(countCalendarEvents(eventList, contact), 1);

    // Close the calendar.
    QVERIFY2(storage->close(), "Error closing the calendar");
}

void TestBirthdayPlugin::testLocaleChange()
{
    MGConfDataStore store(QLatin1String("/meegotouch/i18n"));

    store.setValue(QLatin1String("language"), QLatin1String("en"));

    // Leave the time to react to locale change
    loopWait(1000);

    // Use the C locale so it can be changed to a different locale later.
    MLocale locale;
    QVERIFY2(locale.isValid(), "Invalid locale");

    if (not locale.isInstalledTrCatalog(QLatin1String("calendar"))) {
        locale.installTrCatalog(QLatin1String("calendar"));
    }

    locale.connectSettings();
    MLocale::setDefault(locale);

    // Open calendar database, which should have been created by the birthday plugin.
    mKCal::ExtendedCalendar::Ptr calendar =
        mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(KDateTime::Spec::LocalZone()));
    mKCal::ExtendedStorage::Ptr storage =
        mKCal::ExtendedCalendar::defaultStorage(calendar);
    storage->open();
    QVERIFY2(not storage->notebook(calNotebookID).isNull(), "No calendar database found");

    // Check if locale name for calendar matches calendar name.
    //QVERIFY2(locale.isInstalledTrCatalog(QLatin1String("calendar")), "Calendar locale catalog not installed");
    const QString cLocaleCalendarName = qtTrId("qtn_caln_birthdays");
    QVERIFY2(storage->notebook(calNotebookID)->name() == cLocaleCalendarName, "Locale name does not match calendar name");

    // Change locale and check name again.
    store.setValue(QLatin1String("language"), QLatin1String("fi"));

    loopWait(1000);

    const QString finnishLocaleCalendarName = qtTrId("qtn_caln_birthdays");

    QVERIFY2(storage->notebook(calNotebookID)->name() != cLocaleCalendarName, "Calendar name was not updated on locale change");
    QVERIFY2(storage->notebook(calNotebookID)->name() == finnishLocaleCalendarName, "Locale name does not match calendar name");

    // Close the calendar.
    QVERIFY2(storage->close(), "Error closing the calendar");
}

void TestBirthdayPlugin::cleanupTestCase()
{
}

void TestBirthdayPlugin::cleanup()
{
    // Remove all contacts modified during the test run.
    // This could fail if the contacts were already removed, so the response is ignored.
    mManager->removeContacts(mContactLocalIDs.toList());

    mContactLocalIDs.clear();

    delete mManager;

    mManager = 0;
}

int TestBirthdayPlugin::countCalendarEvents(const KCalCore::Event::List &eventList,
                                            const QContact &contact) const
{
    int found = 0;
    Q_FOREACH(const QSharedPointer<KCalCore::Event> event, eventList) {
        if(event->dtStart().date() == contact.detail<QContactBirthday>().date()) {
            if(event->summary() == contact.detail<QContactDisplayLabel>().label()) {
                found++;
            }
        }
    }

    return found;
}

bool TestBirthdayPlugin::saveContact(QContact &contact)
{
    mContactLocalIDs.insert(contact.localId());

    return mManager->saveContact(&contact);
}

QTEST_MAIN(TestBirthdayPlugin)