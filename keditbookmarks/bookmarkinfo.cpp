/* This file is part of the KDE project
   Copyright (C) 2003 Alexander Kellett <lypanov@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "bookmarkinfo.h"
#include "bookmarklistview.h"
#include "commands.h"
#include "commandhistory.h"
#include "toplevel.h" // for CurrentMgr
#include "bookmarkmodel.h"

#include <stdlib.h>

#include <QtCore/QTimer>
#include <QtGui/QLabel>
#include <QtGui/QHBoxLayout>
#include <QtGui/QFormLayout>

#include <klocale.h>
#include <kdebug.h>

#include <klineedit.h>

#include <kbookmark.h>

// SHUFFLE all these functions around, the order is just plain stupid
void BookmarkInfoWidget::showBookmark(const KBookmark &bk) {
    commitChanges();
    m_bk = bk;

    if (m_bk.isNull()) {
        // all read only and blank

        m_title_le->setReadOnly(true);
        m_title_le->setText(QString());

        m_url_le->setReadOnly(true);
        m_url_le->setText(QString());

        m_comment_le->setReadOnly(true);
        m_comment_le->setText(QString());

        m_visitdate_le->setReadOnly(true);
        m_visitdate_le->setText(QString());

        m_credate_le->setReadOnly(true);
        m_credate_le->setText(QString());

        m_visitcount_le->setReadOnly(true);
        m_visitcount_le->setText(QString());

        return;
    }

    // read/write fields
    m_title_le->setReadOnly( (bk.isSeparator()|| !bk.hasParent() )? true : false);
    if (bk.fullText() != m_title_le->text())
        m_title_le->setText(bk.fullText());

    m_url_le->setReadOnly(bk.isGroup() || bk.isSeparator());
    if (bk.isGroup()) {
         m_url_le->setText(QString());
    }
    else {
        // Update the text if and only if the text represents a different URL to that
        // of the current bookmark - the old method, "m_url_le->text() != bk.url().pathOrUrl()",
        // created difficulties due to the ambiguity of converting URLs to text. (#172647)
        if (KUrl(m_url_le->text()) != bk.url()) {
            const int cursorPosition = m_url_le->cursorPosition();
            m_url_le->setText(bk.url().pathOrUrl());
            m_url_le->setCursorPosition(cursorPosition);
        }
    }

    m_comment_le->setReadOnly((bk.isSeparator()|| !bk.hasParent()) ? true : false );
    QString commentText = EditCommand::getNodeText(bk, QStringList() << "desc");
    if (m_comment_le->text() != commentText) {
        const int cursorPosition = m_comment_le->cursorPosition();
        m_comment_le->setText(commentText);
        m_comment_le->setCursorPosition(cursorPosition);
    }

    // readonly fields
    updateStatus();

}

void BookmarkInfoWidget::updateStatus()
{
   //FIXME we don't want every metadata element, but only that with owner "http://www.kde.org"
   QString visitDate =
        CurrentMgr::makeTimeStr( EditCommand::getNodeText(m_bk, QStringList() << "info" << "metadata"
                                                             << "time_visited" ));
    m_visitdate_le->setReadOnly(true);
    m_visitdate_le->setText(visitDate);

    QString creationDate =
        CurrentMgr::makeTimeStr( EditCommand::getNodeText(m_bk, QStringList() << "info" << "metadata"
                                                             << "time_added" ));
    m_credate_le->setReadOnly(true);
    m_credate_le->setText(creationDate);

    // TODO - get the actual field name from the spec if it exists, else copy galeon
    m_visitcount_le->setReadOnly(true);
    m_visitcount_le->setText(
            EditCommand::getNodeText(m_bk, QStringList() << "info" << "metadata"
                                                           << "visit_count" ));
}

void BookmarkInfoWidget::commitChanges()
{
    commitTitle();
    commitURL();
    commitComment();
}

void BookmarkInfoWidget::commitTitle()
{
    if(titlecmd)
    {
        CurrentMgr::self()->notifyManagers(CurrentMgr::bookmarkAt(titlecmd->affectedBookmarks()).toGroup());
        titlecmd = 0;
    }
}

void BookmarkInfoWidget::slotTextChangedTitle(const QString &str)
{
    if (m_bk.isNull() || !m_title_le->isModified())
        return;

    timer->start(1000);

    if(titlecmd)
    {
        titlecmd->modify(str);
        titlecmd->redo();
    }
    else
    {
        titlecmd = new EditCommand(m_bk.address(), 0, str);
        titlecmd->redo();
        CmdHistory::self()->addInFlightCommand(titlecmd);
    }
}

void BookmarkInfoWidget::commitURL()
{
    if(urlcmd)
    {
        CurrentMgr::self()->notifyManagers(CurrentMgr::bookmarkAt(urlcmd->affectedBookmarks()).toGroup());
        urlcmd = 0;
    }
}

void BookmarkInfoWidget::slotTextChangedURL(const QString &str) {
    if (m_bk.isNull() || !m_url_le->isModified())
        return;

    timer->start(1000);

    if(urlcmd)
    {
        urlcmd->modify(str);
        urlcmd->redo();
    }
    else
    {
        urlcmd = new EditCommand(m_bk.address(), 1, str);
        urlcmd->redo();
        CmdHistory::self()->addInFlightCommand(urlcmd);
    }
}

void BookmarkInfoWidget::commitComment()
{
    if(commentcmd)
    {
        CurrentMgr::self()->notifyManagers( CurrentMgr::bookmarkAt( commentcmd->affectedBookmarks() ).toGroup());
        commentcmd = 0;
    }
}

void BookmarkInfoWidget::slotTextChangedComment(const QString &str) {
    if (m_bk.isNull() || !m_comment_le->isModified())
        return;

    timer->start(1000);

    if(commentcmd)
    {
        commentcmd->modify(str);
        commentcmd->redo();
    }
    else
    {
        commentcmd = new EditCommand(m_bk.address(), 2, str);
        commentcmd->redo();
        CmdHistory::self()->addInFlightCommand(commentcmd);
    }
}

void BookmarkInfoWidget::slotUpdate()
{
    const QModelIndexList & list = mBookmarkListView->selectionModel()->selectedRows();
    if( list.count() == 1)
    {
        QModelIndex index = *list.constBegin();
        showBookmark( mBookmarkListView->bookmarkModel()->bookmarkForIndex(index) );
    }
    else
        showBookmark( KBookmark() );
}

BookmarkInfoWidget::BookmarkInfoWidget(BookmarkListView * lv, QWidget *parent)
    : QWidget(parent), mBookmarkListView(lv) {

    connect(mBookmarkListView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
            SLOT( slotUpdate()));

    connect(mBookmarkListView->model(), SIGNAL(dataChanged( const QModelIndex &, const QModelIndex &)),
            SLOT( slotUpdate()));

    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, SIGNAL( timeout() ), SLOT( commitChanges()));


    titlecmd = 0;
    urlcmd = 0;
    commentcmd = 0;

    QHBoxLayout *layout = new QHBoxLayout(this);
    QFormLayout *form1 = new QFormLayout();
    QFormLayout *form2 = new QFormLayout();
    layout->addLayout(form1);
    layout->addLayout(form2);

    m_title_le = new KLineEdit(this);
    m_title_le->setClearButtonShown(true);
    form1->addRow(i18n("Name:"), m_title_le);

    connect(m_title_le, SIGNAL( textChanged(const QString &) ),
                        SLOT( slotTextChangedTitle(const QString &) ));
    connect(m_title_le, SIGNAL( editingFinished() ), SLOT( commitTitle() ));

    m_url_le = new KLineEdit(this);
    m_url_le->setClearButtonShown(true);
    form1->addRow(i18n("Location:"), m_url_le);

    connect(m_url_le, SIGNAL( textChanged(const QString &) ),
                      SLOT( slotTextChangedURL(const QString &) ));
    connect(m_url_le, SIGNAL( editingFinished() ), SLOT( commitURL() ));

    m_comment_le = new KLineEdit(this);
    m_comment_le->setClearButtonShown(true);
    form1->addRow(i18n("Comment:"), m_comment_le);

    connect(m_comment_le, SIGNAL( textChanged(const QString &) ),
                          SLOT( slotTextChangedComment(const QString &) ));
    connect(m_comment_le, SIGNAL( editingFinished() ), SLOT( commitComment() ));

    m_credate_le = new KLineEdit(this);
    form2->addRow(i18n("First viewed:"), m_credate_le);

    m_visitdate_le = new KLineEdit(this);
    form2->addRow(i18n("Viewed last:"), m_visitdate_le);

    m_visitcount_le = new KLineEdit(this);
    form2->addRow(i18n("Times visited:"), m_visitcount_le);

    showBookmark(KBookmark());
}

#include "bookmarkinfo.moc"

