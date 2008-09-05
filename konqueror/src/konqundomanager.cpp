/* This file is part of the KDE project
   Copyright 2007 David Faure <faure@kde.org>
   Copyright 2007 Eduardo Robles Elvira <edulix@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "konqsettingsxt.h"
#include "konqundomanager.h"
#include "konqcloseditem.h"
#include "konqclosedwindowsmanager.h"
#include <QAction>
#include <QByteArray>
#include <QFile>
#include <QTimer>
#include <QVariant>
#include <kio/fileundomanager.h>
#include <kdebug.h>
#include <klocale.h>


KonqUndoManager::KonqUndoManager(QWidget* parent)
    : QObject(parent)
{
    connect( KIO::FileUndoManager::self(), SIGNAL(undoAvailable(bool)),
             this, SLOT(slotFileUndoAvailable(bool)) );
    connect( KIO::FileUndoManager::self(), SIGNAL(undoTextChanged(QString)),
             this, SLOT(slotFileUndoTextChanged(QString)) );

    connect(KonqClosedWindowsManager::self(),
        SIGNAL(addWindowInOtherInstances(KonqUndoManager *, KonqClosedWindowItem *)), this,
        SLOT( slotAddClosedWindowItem(KonqUndoManager *, KonqClosedWindowItem *) ));
    connect(KonqClosedWindowsManager::self(),
        SIGNAL(removeWindowInOtherInstances(KonqUndoManager *, const KonqClosedWindowItem *)), this,
        SLOT( slotRemoveClosedWindowItem(KonqUndoManager *, const KonqClosedWindowItem *) ));
    m_populated = false;
}

KonqUndoManager::~KonqUndoManager()
{
    disconnect( KIO::FileUndoManager::self(), SIGNAL(undoAvailable(bool)),
             this, SLOT(slotFileUndoAvailable(bool)) );
    disconnect( KIO::FileUndoManager::self(), SIGNAL(undoTextChanged(QString)),
             this, SLOT(slotFileUndoTextChanged(QString)) );

    disconnect(KonqClosedWindowsManager::self(),
        SIGNAL(addWindowInOtherInstances(KonqUndoManager *, KonqClosedWindowItem *)), this,
        SLOT( slotAddClosedWindowItem(KonqUndoManager *, KonqClosedWindowItem *) ));
    disconnect(KonqClosedWindowsManager::self(),
        SIGNAL(removeWindowInOtherInstances(KonqUndoManager *, const KonqClosedWindowItem *)), this,
        SLOT( slotRemoveClosedWindowItem(KonqUndoManager *, const KonqClosedWindowItem *) ));

    // Clear the closed item lists but only remove closed windows items
    // in this window
    clearClosedItemsList(true);
}

void KonqUndoManager::populate()
{
    if(m_populated)
        return;
    m_populated = true;

    const QList<KonqClosedWindowItem *> closedWindowItemList =
        KonqClosedWindowsManager::self()->closedWindowItemList();

    QListIterator<KonqClosedWindowItem *> i(closedWindowItemList);

    // This loop is done backwards because slotAddClosedWindowItem prepends the
    // elements to the list, so if we do it forwards the we would get an inverse
    // order of closed windows
    for(i.toBack(); i.hasPrevious(); )
        slotAddClosedWindowItem(0L, i.previous());
}

void KonqUndoManager::slotFileUndoAvailable(bool)
{
    emit undoAvailable(this->undoAvailable());
}

bool KonqUndoManager::undoAvailable() const
{
    if (!m_closedItemList.isEmpty() || KonqClosedWindowsManager::self()->undoAvailable())
        return true;
    else
        return (m_supportsFileUndo && KIO::FileUndoManager::self()->undoAvailable());
}

QString KonqUndoManager::undoText() const
{
    if (!m_closedItemList.isEmpty()) {
        const KonqClosedItem* closedItem = m_closedItemList.first();
        if (closedItem->serialNumber() > KIO::FileUndoManager::self()->currentCommandSerialNumber()) {
            const KonqClosedTabItem* closedTabItem =
                dynamic_cast<const KonqClosedTabItem *>(closedItem);
            if(closedTabItem)
                return i18n("Und&o: Closed Tab");
            else
                return i18n("Und&o: Closed Window");
        }
    } else if(KonqClosedWindowsManager::self()->undoAvailable())
        return i18n("Und&o: Closed Window");

    return KIO::FileUndoManager::self()->undoText();
}

void KonqUndoManager::undo()
{
    populate();
    KIO::FileUndoManager* fileUndoManager = KIO::FileUndoManager::self();
    if (!m_closedItemList.isEmpty()) {
        KonqClosedItem* closedItem = m_closedItemList.first();

        // Check what to undo
        if (closedItem->serialNumber() > fileUndoManager->currentCommandSerialNumber()) {
            undoClosedItem(0);
            return;
        }
    }
    fileUndoManager->uiInterface()->setParentWidget(qobject_cast<QWidget*>(parent()));
    fileUndoManager->undo();
}

void KonqUndoManager::slotAddClosedWindowItem(KonqUndoManager *real_sender, KonqClosedWindowItem *closedWindowItem)
{
    if(real_sender == this)
        return;

    populate();

    if(m_closedItemList.size() >= KonqSettings::maxNumClosedItems())
    {
        const KonqClosedItem* last = m_closedItemList.last();
        const KonqClosedTabItem* lastTab =
            dynamic_cast<const KonqClosedTabItem *>(last);
        m_closedItemList.removeLast();

        // Delete only if it's a tab
        if(lastTab)
            delete lastTab;
    }

    m_closedItemList.prepend(closedWindowItem);
    emit undoTextChanged(i18n("Und&o: Closed Window"));
    emit undoAvailable(true);
    emit closedItemsListChanged();
}

void KonqUndoManager::addClosedWindowItem(KonqClosedWindowItem *closedWindowItem)
{
    populate();
    KonqClosedWindowsManager::self()->addClosedWindowItem(this, closedWindowItem);
}

void KonqUndoManager::slotRemoveClosedWindowItem(KonqUndoManager *real_sender, const KonqClosedWindowItem *closedWindowItem)
{
    if(real_sender == this)
        return;

    populate();

    QList<KonqClosedItem *>::iterator it = qFind(m_closedItemList.begin(), m_closedItemList.end(), closedWindowItem);

    // If the item was found, remove it from the list
    if(it != m_closedItemList.end()) {
        m_closedItemList.erase(it);
        emit undoAvailable(this->undoAvailable());
        emit closedItemsListChanged();
    }
}

const QList<KonqClosedItem *>& KonqUndoManager::closedItemsList()
{
    populate();
    return m_closedItemList;
}

void KonqUndoManager::undoClosedItem(int index)
{
    populate();
    Q_ASSERT(!m_closedItemList.isEmpty());
    KonqClosedItem* closedItem = m_closedItemList.at( index );
    m_closedItemList.removeAt(index);

    const KonqClosedTabItem* closedTabItem =
        dynamic_cast<const KonqClosedTabItem *>(closedItem);
    KonqClosedRemoteWindowItem* closedRemoteWindowItem =
        dynamic_cast<KonqClosedRemoteWindowItem *>(closedItem);
    KonqClosedWindowItem* closedWindowItem =
        dynamic_cast<KonqClosedWindowItem *>(closedItem);
    if(closedTabItem)
        emit openClosedTab(*closedTabItem);
    else if(closedRemoteWindowItem) {
        emit openClosedWindow(*closedRemoteWindowItem);
        KonqClosedWindowsManager::self()->removeClosedWindowItem(this, closedRemoteWindowItem);
    } else if(closedWindowItem) {
        emit openClosedWindow(*closedWindowItem);
        KonqClosedWindowsManager::self()->removeClosedWindowItem(this, closedWindowItem);
        closedWindowItem->configGroup().deleteGroup();
    }
    delete closedItem;
    emit undoAvailable(this->undoAvailable());
    emit undoTextChanged(this->undoText());
    emit closedItemsListChanged();
}

void KonqUndoManager::slotClosedItemsActivated(QAction* action)
{
    // Open a closed tab
    const int index = action->data().toInt();
    undoClosedItem(index);
}

void KonqUndoManager::slotFileUndoTextChanged(const QString& text)
{
    // I guess we can always just forward that one?
    emit undoTextChanged(text);
}

quint64 KonqUndoManager::newCommandSerialNumber()
{
    return KIO::FileUndoManager::self()->newCommandSerialNumber();
}

void KonqUndoManager::addClosedTabItem(KonqClosedTabItem* closedTabItem)
{
    populate();

    if(m_closedItemList.size() >= KonqSettings::maxNumClosedItems())
    {
        const KonqClosedItem* last = m_closedItemList.last();
        const KonqClosedTabItem* lastTab =
            dynamic_cast<const KonqClosedTabItem *>(last);
        m_closedItemList.removeLast();

        // Delete only if it's a tab
        if(lastTab)
            delete lastTab;
    }

    m_closedItemList.prepend(closedTabItem);
    emit undoTextChanged(i18n("Und&o: Closed Tab"));
    emit undoAvailable(true);
}

void KonqUndoManager::updateSupportsFileUndo(bool enable)
{
    m_supportsFileUndo = enable;
    emit undoAvailable(this->undoAvailable());
}

void KonqUndoManager::clearClosedItemsList(bool onlyInthisWindow)
{
    populate();
// we only DELETE tab items! So we can't do this anymore:
//     qDeleteAll(m_closedItemList);
    QList<KonqClosedItem *>::iterator it = m_closedItemList.begin();
    for (; it != m_closedItemList.end(); ++it)
    {
        KonqClosedItem *closedItem = *it;
        const KonqClosedTabItem* closedTabItem =
            dynamic_cast<const KonqClosedTabItem *>(closedItem);
        const KonqClosedWindowItem* closedWindowItem =
            dynamic_cast<const KonqClosedWindowItem *>(closedItem);

        m_closedItemList.erase(it);
        if(closedTabItem)
            delete closedTabItem;
        else if (closedWindowItem && !onlyInthisWindow) {
            KonqClosedWindowsManager::self()->removeClosedWindowItem(this, closedWindowItem, true);
            delete closedWindowItem;
        }
    }
    emit closedItemsListChanged();
    emit undoAvailable(this->undoAvailable());
}

void KonqUndoManager::undoLastClosedItem()
{
    undoClosedItem(0);
}

#include "konqundomanager.moc"
