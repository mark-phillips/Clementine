/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "libraryfilterwidget.h"
#include "librarymodel.h"
#include "groupbydialog.h"
#include "ui_libraryfilterwidget.h"
#include "ui/iconloader.h"
#include "ui/settingsdialog.h"
#include "widgets/maclineedit.h"

#include <QActionGroup>
#include <QKeyEvent>
#include <QMenu>
#include <QSettings>
#include <QSignalMapper>
#include <QTimer>

LibraryFilterWidget::LibraryFilterWidget(QWidget *parent)
  : QWidget(parent),
    ui_(new Ui_LibraryFilterWidget),
    model_(NULL),
    group_by_dialog_(new GroupByDialog),
    filter_delay_(new QTimer(this))
{
  ui_->setupUi(this);
  connect(ui_->filter, SIGNAL(returnPressed()), SIGNAL(ReturnPressed()));
  connect(filter_delay_, SIGNAL(timeout()), SLOT(FilterDelayTimeout()));

  filter_delay_->setInterval(kFilterDelay);
  filter_delay_->setSingleShot(true);

  // Icons
  ui_->options->setIcon(IconLoader::Load("configure"));

  // Filter by age
  QActionGroup* filter_age_group = new QActionGroup(this);
  filter_age_group->addAction(ui_->filter_age_all);
  filter_age_group->addAction(ui_->filter_age_today);
  filter_age_group->addAction(ui_->filter_age_week);
  filter_age_group->addAction(ui_->filter_age_month);
  filter_age_group->addAction(ui_->filter_age_three_months);
  filter_age_group->addAction(ui_->filter_age_year);

  filter_age_menu_ = new QMenu(tr("Show"), this);
  filter_age_menu_->addActions(filter_age_group->actions());

  filter_age_mapper_ = new QSignalMapper(this);
  filter_age_mapper_->setMapping(ui_->filter_age_all, -1);
  filter_age_mapper_->setMapping(ui_->filter_age_today, 60*60*24);
  filter_age_mapper_->setMapping(ui_->filter_age_week, 60*60*24*7);
  filter_age_mapper_->setMapping(ui_->filter_age_month, 60*60*24*30);
  filter_age_mapper_->setMapping(ui_->filter_age_three_months, 60*60*24*30*3);
  filter_age_mapper_->setMapping(ui_->filter_age_year, 60*60*24*365);

  connect(ui_->filter_age_all, SIGNAL(triggered()), filter_age_mapper_, SLOT(map()));
  connect(ui_->filter_age_today, SIGNAL(triggered()), filter_age_mapper_, SLOT(map()));
  connect(ui_->filter_age_week, SIGNAL(triggered()), filter_age_mapper_, SLOT(map()));
  connect(ui_->filter_age_month, SIGNAL(triggered()), filter_age_mapper_, SLOT(map()));
  connect(ui_->filter_age_three_months, SIGNAL(triggered()), filter_age_mapper_, SLOT(map()));
  connect(ui_->filter_age_year, SIGNAL(triggered()), filter_age_mapper_, SLOT(map()));

  // "Group by ..."
  ui_->group_by_artist->setProperty("group_by", QVariant::fromValue(
      LibraryModel::Grouping(LibraryModel::GroupBy_Artist)));
  ui_->group_by_artist_album->setProperty("group_by", QVariant::fromValue(
      LibraryModel::Grouping(LibraryModel::GroupBy_Artist, LibraryModel::GroupBy_Album)));
  ui_->group_by_artist_yearalbum->setProperty("group_by", QVariant::fromValue(
      LibraryModel::Grouping(LibraryModel::GroupBy_Artist, LibraryModel::GroupBy_YearAlbum)));
  ui_->group_by_album->setProperty("group_by", QVariant::fromValue(
      LibraryModel::Grouping(LibraryModel::GroupBy_Album)));
  ui_->group_by_genre_album->setProperty("group_by", QVariant::fromValue(
      LibraryModel::Grouping(LibraryModel::GroupBy_Genre, LibraryModel::GroupBy_Album)));
  ui_->group_by_genre_artist_album->setProperty("group_by", QVariant::fromValue(
      LibraryModel::Grouping(LibraryModel::GroupBy_Genre, LibraryModel::GroupBy_Artist, LibraryModel::GroupBy_Album)));

  group_by_group_ = new QActionGroup(this);
  group_by_group_->addAction(ui_->group_by_artist);
  group_by_group_->addAction(ui_->group_by_artist_album);
  group_by_group_->addAction(ui_->group_by_artist_yearalbum);
  group_by_group_->addAction(ui_->group_by_album);
  group_by_group_->addAction(ui_->group_by_genre_album);
  group_by_group_->addAction(ui_->group_by_genre_artist_album);
  group_by_group_->addAction(ui_->group_by_advanced);

  group_by_menu_ = new QMenu(tr("Group by"), this);
  group_by_menu_->addActions(group_by_group_->actions());

  connect(group_by_group_, SIGNAL(triggered(QAction*)), SLOT(GroupByClicked(QAction*)));

  // Library config menu
  library_menu_ = new QMenu(this);
  library_menu_->addMenu(filter_age_menu_);
  library_menu_->addMenu(group_by_menu_);
  library_menu_->addSeparator();
  ui_->options->setMenu(library_menu_);

#ifdef Q_OS_DARWIN
  delete ui_->filter;
  MacLineEdit* lineedit = new MacLineEdit(this);
  ui_->horizontalLayout->insertWidget(1, lineedit);
  filter_ = lineedit;
#else
  filter_ = ui_->filter;
#endif

  connect(filter_->widget(), SIGNAL(textChanged(QString)), SLOT(FilterTextChanged(QString)));
}

LibraryFilterWidget::~LibraryFilterWidget() {
  delete ui_;
}

void LibraryFilterWidget::SetLibraryModel(LibraryModel *model) {
  if (model_) {
    disconnect(model_, 0, this, 0);
    disconnect(model_, 0, group_by_dialog_.get(), 0);
    disconnect(group_by_dialog_.get(), 0, model_, 0);
    disconnect(filter_age_mapper_, 0, model_, 0);
  }

  model_ = model;

  // Connect signals
  connect(model_, SIGNAL(GroupingChanged(LibraryModel::Grouping)),
          group_by_dialog_.get(), SLOT(LibraryGroupingChanged(LibraryModel::Grouping)));
  connect(model_, SIGNAL(GroupingChanged(LibraryModel::Grouping)),
          SLOT(GroupingChanged(LibraryModel::Grouping)));
  connect(group_by_dialog_.get(), SIGNAL(Accepted(LibraryModel::Grouping)),
          model_, SLOT(SetGroupBy(LibraryModel::Grouping)));
  connect(filter_age_mapper_, SIGNAL(mapped(int)), model_, SLOT(SetFilterAge(int)));

  // Load settings
  if (!settings_group_.isEmpty()) {
    QSettings s;
    s.beginGroup(settings_group_);
    model_->SetGroupBy(LibraryModel::Grouping(
        LibraryModel::GroupBy(s.value("group_by1", int(LibraryModel::GroupBy_Artist)).toInt()),
        LibraryModel::GroupBy(s.value("group_by2", int(LibraryModel::GroupBy_Album)).toInt()),
        LibraryModel::GroupBy(s.value("group_by3", int(LibraryModel::GroupBy_None)).toInt())));
  }
}

void LibraryFilterWidget::GroupByClicked(QAction* action) {
  if (action->property("group_by").isNull()) {
    group_by_dialog_->show();
    return;
  }

  LibraryModel::Grouping g = action->property("group_by").value<LibraryModel::Grouping>();
  model_->SetGroupBy(g);
}

void LibraryFilterWidget::GroupingChanged(const LibraryModel::Grouping& g) {
  if (!settings_group_.isEmpty()) {
    // Save the settings
    QSettings s;
    s.beginGroup(settings_group_);
    s.setValue("group_by1", int(g[0]));
    s.setValue("group_by2", int(g[1]));
    s.setValue("group_by3", int(g[2]));
  }

  // Now make sure the correct action is checked
  foreach (QAction* action, group_by_group_->actions()) {
    if (action->property("group_by").isNull())
      continue;

    if (g == action->property("group_by").value<LibraryModel::Grouping>()) {
      action->setChecked(true);
      return;
    }
  }
  ui_->group_by_advanced->setChecked(true);
}

void LibraryFilterWidget::SetFilterHint(const QString& hint) {
  filter_->set_hint(hint);
}

void LibraryFilterWidget::SetDuplicatesOnly(bool duplicates_only) {
  // no filtering in duplicates_only mode
  filter_->clear();
  // TODO: won't work on Mac
  ui_->filter->setEnabled(!duplicates_only);

  model_->SetFilterDuplicatesOnly(duplicates_only);
}

void LibraryFilterWidget::SetAgeFilterEnabled(bool enabled) {
  filter_age_menu_->setEnabled(enabled);
}

void LibraryFilterWidget::SetGroupByEnabled(bool enabled) {
  group_by_menu_->setEnabled(enabled);
}

void LibraryFilterWidget::AddMenuAction(QAction* action) {
  library_menu_->addAction(action);
}

void LibraryFilterWidget::keyReleaseEvent(QKeyEvent* e) {
  switch (e->key()) {
    case Qt::Key_Up:
      emit UpPressed();
      e->accept();
      break;

    case Qt::Key_Down:
      emit DownPressed();
      e->accept();
      break;
  }

  QWidget::keyReleaseEvent(e);
}

void LibraryFilterWidget::FilterTextChanged(const QString& text) {
  // Searching with one or two characters can be very expensive on the database
  // even with FTS, so if there are a large number of songs in the database
  // introduce a small delay before actually filtering the model, so if the
  // user is typing the first few characters of something it will be quicker.
  if (!text.isEmpty() && text.length() < 3 && model_->total_song_count() >= 100000) {
    filter_delay_->start();
  } else {
    filter_delay_->stop();
    model_->SetFilterText(text);
  }
}

void LibraryFilterWidget::FilterDelayTimeout() {
  model_->SetFilterText(filter_->text());
}
