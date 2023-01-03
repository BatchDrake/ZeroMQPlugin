#ifndef MULTICHANNELTREEMODEL_H
#define MULTICHANNELTREEMODEL_H

#include <QAbstractItemModel>
#include <MultiChannelForwarder.h>
#include <QHash>

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
  int index = -1;
  QVector<MultiChannelTreeItem *> children;
};

class MultiChannelTreeModel : public QAbstractItemModel
{
  Q_OBJECT

  MultiChannelForwarder *m_forwarder = nullptr;
  QHash<int, MultiChannelTreeItem> m_treeStructure;
  MultiChannelTreeItem *m_rootItem;

public:
  static MultiChannelTreeItem *indexData(const QModelIndex &);
  void rebuildStructure();
  MultiChannelTreeItem *allocItem(MultiChannelTreeItemType type);

  explicit MultiChannelTreeModel(MultiChannelForwarder *, QObject *parent = nullptr);

  ~MultiChannelTreeModel();

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
