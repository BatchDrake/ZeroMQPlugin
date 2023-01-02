#include "MultiChannelTreeModel.h"

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
  m_treeStructure.push_back(MultiChannelTreeItem());

  auto lastItem = &*m_treeStructure.rbegin();
  lastItem->type = type;

  return lastItem;
}

void
MultiChannelTreeModel::rebuildStructure()
{
  m_treeStructure.clear();
  m_rootItem = allocItem(MULTI_CHANNEL_TREE_ITEM_ROOT);

  for (MasterChannel *p : *m_forwarder) {
    auto *masterItem = allocItem(MULTI_CHANNEL_TREE_ITEM_MASTER);

    masterItem->parent = m_rootItem;
    masterItem->master = p;
    masterItem->index  = m_rootItem->children.size();
    m_rootItem->children.push_back(masterItem);

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
              return master->frequency;

            case 2:
              return master->bandwidth;
          }
          break;

        case MULTI_CHANNEL_TREE_ITEM_CHANNEL:
          channel = item->channel;

          switch (index.column()) {
            case 0:
              return QString::fromStdString(channel->name);

            case 1:
              return channel->offset + channel->parent->frequency;

            case 2:
              return channel->bandwidth;

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
  if (parent.column() > 0)
      return 0;

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
