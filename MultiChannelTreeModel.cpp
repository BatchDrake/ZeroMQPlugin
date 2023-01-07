//
//    MultiChannelTreeModel.cpp: Tree model of the multichannel forwarder
//    Copyright (C) 2023 Gonzalo José Carracedo Carballal
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this program.  If not, see
//    <http://www.gnu.org/licenses/>
//

#include "MultiChannelTreeModel.h"
#include <SuWidgetsHelpers.h>
#include <QTreeView>

MultiChannelTreeItem *
MultiChannelTreeModel::indexData(const QModelIndex &index)
{
  MultiChannelTreeItem *item;

  if (!index.isValid())
    return nullptr;

  item = reinterpret_cast<MultiChannelTreeItem *>(index.internalPointer());

  return item;
}

void
MultiChannelTreeModel::fastExpand(QTreeView *view)
{
  QModelIndex parent = createIndex(0, 0, m_rootItem);

  for (auto p : m_masterHash)
    view->expand(index(p->index, 0, parent));
}

MultiChannelTreeModel::MultiChannelTreeModel(
    MultiChannelForwarder *forwarder,
    QObject *parent)
  : QAbstractItemModel{parent}
{
  m_forwarder = forwarder;

  rebuildStructure();
}

MultiChannelTreeModel::~MultiChannelTreeModel()
{

}

MultiChannelTreeItem *
MultiChannelTreeModel::allocItem(MultiChannelTreeItemType type)
{
  auto item = m_treeStructure.insert(
        m_treeStructure.size(),
        MultiChannelTreeItem());

  auto lastItem = &item.value();
  lastItem->type = type;

  return lastItem;
}

void
MultiChannelTreeModel::rebuildStructure()
{
  beginResetModel();

  m_treeStructure.clear();
  m_masterHash.clear();

  m_rootItem = allocItem(MULTI_CHANNEL_TREE_ITEM_ROOT);

  for (MasterChannel *p : *m_forwarder) {
    auto *masterItem = allocItem(MULTI_CHANNEL_TREE_ITEM_MASTER);

    masterItem->parent = m_rootItem;
    masterItem->master = p;
    masterItem->index  = m_rootItem->children.size();
    m_rootItem->children.push_back(masterItem);

    m_masterHash[p->name.c_str()] = masterItem;

    auto i = p->channels.begin();

    while (i != p->channels.end()) {
      auto *channelItem = allocItem(MULTI_CHANNEL_TREE_ITEM_CHANNEL);

      channelItem->parent = masterItem;
      channelItem->channel = &*i;
      channelItem->index   = masterItem->children.size();
      masterItem->children.push_back(channelItem);
      ++i;
    }
  }

  endResetModel();
}

QVariant
MultiChannelTreeModel::data(const QModelIndex &index, int role) const
{
  if (index.isValid()) {
    MultiChannelTreeItem *item = static_cast<MultiChannelTreeItem*>(
          index.internalPointer());
    MasterChannel *master;
    ChannelDescription *channel;

    if (role == Qt::DisplayRole) {
      switch (item->type) {
        case MULTI_CHANNEL_TREE_ITEM_MASTER:
          master = item->master;

          switch (index.column()) {
            case 0:
              return QString::fromStdString(master->name);

            case 1:
              return SuWidgetsHelpers::formatQuantity(
                    master->frequency,
                    "Hz");

            case 2:
              return SuWidgetsHelpers::formatQuantity(
                    master->bandwidth,
                    "Hz");

            case 3:
              return "(Master)";
          }

          break;

        case MULTI_CHANNEL_TREE_ITEM_CHANNEL:
          channel = item->channel;

          switch (index.column()) {
            case 0:
              return QString::fromStdString(channel->name);

            case 1:
              return SuWidgetsHelpers::formatQuantity(
                    channel->offset + channel->parent->frequency,
                    "Hz");

            case 2:
              return SuWidgetsHelpers::formatQuantity(
                    channel->bandwidth,
                    "Hz");

            case 3:
              return QString::fromStdString(channel->inspClass);
          }
          break;

        default:
          break;
      }
    }
  }

  return QVariant();
}

Qt::ItemFlags
MultiChannelTreeModel::flags(const QModelIndex &index) const
{
  if (!index.isValid())
      return Qt::NoItemFlags;

  return QAbstractItemModel::flags(index);
}

QVariant
MultiChannelTreeModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
      case 0:
        return "Name";

      case 1:
        return "Frequency";

      case 2:
        return "Bandwidth";

      case 3:
        return "Modulation";
    }
  }

  return QVariant();
}

QModelIndex
MultiChannelTreeModel::index(
    int row,
    int column,
    const QModelIndex &parent) const
{
  if (!hasIndex(row, column, parent))
      return QModelIndex();

  MultiChannelTreeItem *parentItem;
  MultiChannelTreeItem *childItem;

  if (!parent.isValid())
      parentItem = m_rootItem;
  else
      parentItem = static_cast<MultiChannelTreeItem*>(parent.internalPointer());

  if (row < 0 || row >= parentItem->children.size())
    return QModelIndex();

  childItem = parentItem->children[row];

  return createIndex(row, column, childItem);
}

QModelIndex
MultiChannelTreeModel::parent(const QModelIndex &index) const
{
  if (!index.isValid())
      return QModelIndex();

  MultiChannelTreeItem *childItem = static_cast<MultiChannelTreeItem*>(index.internalPointer());
  MultiChannelTreeItem *parentItem = childItem->parent;

  if (parentItem == m_rootItem)
      return QModelIndex();

  return createIndex(parentItem->index, 0, parentItem);
}

int
MultiChannelTreeModel::rowCount(const QModelIndex &parent) const
{
  MultiChannelTreeItem *parentItem;

  if (!parent.isValid())
      parentItem = m_rootItem;
  else
      parentItem = static_cast<MultiChannelTreeItem*>(parent.internalPointer());

  return parentItem->children.size();
}

int
MultiChannelTreeModel::columnCount(const QModelIndex &) const
{
  // 1. Name
  // 2. Frequency
  // 3. Bandwidth
  // 4. Modulation

  return 4;
}
