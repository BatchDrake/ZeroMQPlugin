//
//    MultiChannelTreeModel.h: Tree model of the multichannel forwarder
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

#ifndef MULTICHANNELTREEMODEL_H
#define MULTICHANNELTREEMODEL_H

#include <QAbstractItemModel>
#include <MultiChannelForwarder.h>
#include <QHash>

class QTreeView;

enum MultiChannelTreeItemType
{
  MULTI_CHANNEL_TREE_ITEM_ROOT,
  MULTI_CHANNEL_TREE_ITEM_MASTER,
  MULTI_CHANNEL_TREE_ITEM_CHANNEL
};

struct MultiChannelTreeItem
{
  MultiChannelTreeItemType type;

  union {
    MasterChannel *master;
    ChannelDescription *channel;
  };

  MultiChannelTreeItem *parent = nullptr;
  bool enabled = true;
  int index = -1;
  QVector<MultiChannelTreeItem *> children;
};

class MultiChannelTreeModel : public QAbstractItemModel
{
  Q_OBJECT

  MultiChannelForwarder *m_forwarder = nullptr;
  QHash<int, MultiChannelTreeItem> m_treeStructure;
  QHash<QString, MultiChannelTreeItem *> m_masterHash;
  MultiChannelTreeItem *m_rootItem;

public:
  static MultiChannelTreeItem *indexData(const QModelIndex &);
  void fastExpand(QTreeView *);
  void rebuildStructure();

  MultiChannelTreeItem *allocItem(MultiChannelTreeItemType type);

  explicit MultiChannelTreeModel(MultiChannelForwarder *, QObject *parent = nullptr);

  ~MultiChannelTreeModel();

  bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);
  QVariant data(const QModelIndex &index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
};

#endif // MULTICHANNELTREEMODEL_H
