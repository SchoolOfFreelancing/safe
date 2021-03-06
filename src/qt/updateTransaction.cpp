#include "updateTransaction.h"
#include "transactionrecord.h"
#include "validation.h"
#include "txmempool.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"
#include <qdebug.h>


extern CTxMemPool mempool;

// queue notifications to show a non freezing progress dialog e.g. for rescan
struct TransactionNotification {
public:
    TransactionNotification() {}
    TransactionNotification(uint256 hash, ChangeType status, bool showTransaction) : hash(hash), status(status), showTransaction(showTransaction) {}

    void invoke(QObject* tt)
    {
        QString strHash = QString::fromStdString(hash.GetHex());
        qDebug() << "NotifyTransactionChanged: " + strHash + " status= " + QString::number(status);
        QMetaObject::invokeMethod(tt, "updateTransaction", Qt::QueuedConnection,
            Q_ARG(QString, strHash),
            Q_ARG(int, status),
            Q_ARG(bool, showTransaction));
    }

private:
    uint256 hash;
    ChangeType status;
    bool showTransaction;
};


static bool fQueueNotifications = false;
static std::vector< TransactionNotification > vQueueNotifications;

static void NotifyTransactionChanged(CUpdateTransaction *tt, CWallet *wallet, const uint256 &hash, ChangeType status)
{
	// Find transaction in wallet
	std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(hash);
	// Determine whether to show transaction or not (determine this here so that no relocking is needed in GUI thread)
	bool inWallet = mi != wallet->mapWallet.end();

	bool showTransaction = (inWallet && TransactionRecord::showTransaction(mi->second));

	TransactionNotification notification(hash, status, showTransaction);

	if (fQueueNotifications)
	{
		vQueueNotifications.push_back(notification);
		return;
	}
	notification.invoke(tt);
}

static void ShowProgress(CUpdateTransaction *tt, const std::string &title, int nProgress)
{
	if (nProgress == 0)
		fQueueNotifications = true;

	if (nProgress == 100)
	{
		fQueueNotifications = false;
		if (vQueueNotifications.size() > 10) // prevent balloon spam, show maximum 10 balloons
			QMetaObject::invokeMethod(tt, "setProcessingQueuedTransactions", Qt::QueuedConnection, Q_ARG(bool, true));
		for (unsigned int i = 0; i < vQueueNotifications.size(); ++i)
		{
			if (vQueueNotifications.size() - i <= 10)
				QMetaObject::invokeMethod(tt, "setProcessingQueuedTransactions", Qt::QueuedConnection, Q_ARG(bool, false));

			vQueueNotifications[i].invoke(tt);
		}
		std::vector<TransactionNotification >().swap(vQueueNotifications); // clear
	}
}


CUpdateTransaction::CUpdateTransaction(QThread* pParent) : QThread(pParent)
{
    m_pWallet = NULL;
    m_bIsExit = true;
	m_bProcessingQueuedTransactions = false;
	m_pWalletModel = NULL;
}

CUpdateTransaction::~CUpdateTransaction()
{
	uninit();
    stopMonitor();
}


void CUpdateTransaction::subscribeToCoreSignals()
{
    // Connect signals to wallet
    if (m_pWallet != NULL)
	{
        m_pWallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
		m_pWallet->ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2));
    }
}

void CUpdateTransaction::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    if (m_pWallet != NULL)
	{
        m_pWallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
		m_pWallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
    }
}

void CUpdateTransaction::startMonitor()
{
    stopMonitor();
    m_bIsExit = false;
    start(QThread::LowPriority);
}

void CUpdateTransaction::stopMonitor()
{
	m_bIsExit = true;
    if (isRunning())
	{        
        wait();
    }
}

void CUpdateTransaction::updateTransaction(const QString& hash, int status, bool showTransaction)
{
	NewTxData stTxData;
	stTxData.strHash = hash;
	stTxData.nStatus = status;
	stTxData.bShowTx = showTransaction;

	{
		LOCK(m_txLock);
		m_vtNewTx.push_back(stTxData);
	}
}


bool CUpdateTransaction::RefreshOverviewPageData(const QList<AssetsDisplayInfo> &listAssetDisplay)
{
    if (listAssetDisplay.size() <= 0)
	{
        return false;
    }

	QList<AssetBalance> listAssetBalance;

	for (int i = 0; i < listAssetDisplay.size(); i++)
	{
		uint256 assetId;
		if (GetAssetIdByAssetName(listAssetDisplay[i].strAssetName.toStdString(), assetId, false))
		{
			CAssetId_AssetInfo_IndexValue assetsInfo;
			AssetBalance assetBalance;
			if (GetAssetInfoByAssetId(assetId, assetsInfo, false))
			{
				CAmount totalBalance = 0, unconfirmedBalance = 0, lockBalance = 0;
				m_pWallet->GetAssetBalance(&assetId, true, totalBalance, unconfirmedBalance, lockBalance);

				assetBalance.amount = totalBalance;
				assetBalance.unconfirmAmount = unconfirmedBalance;
				assetBalance.lockedAmount = lockBalance;
				assetBalance.strUnit = QString::fromStdString(assetsInfo.assetData.strAssetUnit);
				assetBalance.nDecimals = assetsInfo.assetData.nDecimals;
				assetBalance.strAssetName = listAssetDisplay[i].strAssetName;

				listAssetBalance.push_back(assetBalance);
			}
		}
	}

	if (listAssetBalance.size() > 0)
	{
		Q_EMIT updateOverviePage(listAssetBalance);
		return true;
	}

    return false;
}

bool CUpdateTransaction::RefreshAssetData(const QList<CAssetData> &listIssueAsset)
{
    if (listIssueAsset.size() <= 0)
	{
        return false;
    }

    std::vector<std::string> vtAssetName;

    for (QList<CAssetData>::const_iterator iter = listIssueAsset.begin(); iter != listIssueAsset.end(); ++iter)
	{
		vtAssetName.push_back(iter->strAssetName);
    }

    std::sort(vtAssetName.begin(), vtAssetName.end());

    QStringList stringList;
    for (unsigned int i = 0; i < vtAssetName.size(); i++)
	{
        stringList.append(QString::fromStdString(vtAssetName[i]));
    }

    if (stringList.size() > 0)
	{
        Q_EMIT updateAssetPage(stringList);
        return true;
    }

    return false;
}

bool CUpdateTransaction::RefreshCandyPageData(const QList<CAssetData> &listIssueAsset)
{
    if (listIssueAsset.size() <= 0)
	{
        return false;
    }

    std::vector<std::string> assetNameVec;

    for (QList<CAssetData>::const_iterator iter = listIssueAsset.begin(); iter != listIssueAsset.end(); ++iter)
	{
        const CAssetData &assetData = *iter;
		uint256 assetId = assetData.GetHash();

        int memoryPutCandyCount = mempool.get_PutCandy_count(assetId);
        int dbPutCandyCount = 0;
        map<COutPoint, CCandyInfo> mapCandyInfo;
        if (GetAssetIdCandyInfo(assetId, mapCandyInfo))
		{
            dbPutCandyCount = mapCandyInfo.size();
        }

        int putCandyCount = memoryPutCandyCount + dbPutCandyCount;
        if (putCandyCount < MAX_PUTCANDY_VALUE)
		{
            assetNameVec.push_back(assetData.strAssetName);
        }
    }
    std::sort(assetNameVec.begin(), assetNameVec.end());

    QStringList stringList;
    for (unsigned int i = 0; i < assetNameVec.size(); i++)
	{
        QString assetName = QString::fromStdString(assetNameVec[i]);
        stringList.append(assetName);
    }
    
	if (stringList.size() > 0)
	{
		Q_EMIT updateCandyPage(stringList);
		return true;
	}

	return false;
}

void CUpdateTransaction::run()
{
	int nMax = 0;
	bool bSleep = false;

    while (!m_bIsExit)
	{
		if (bSleep)
		{
			bSleep = false;
			msleep(1000);
		}

        QMap<uint256, NewTxData> mapNewTxData;
		
		{
			LOCK(m_txLock);
			if (m_vtNewTx.size() <= 0)
			{
				bSleep = true;
				continue;
			}

			nMax = 50;
			if (m_vtNewTx.size() < nMax)
			{
				nMax = m_vtNewTx.size();
			}

			for (int i = 0; i < nMax; i++)
			{
				uint256 updatedHash;
				updatedHash.SetHex(m_vtNewTx[i].strHash.toStdString());
				mapNewTxData.insert(updatedHash, m_vtNewTx[i]);
			}

			m_vtNewTx.erase(m_vtNewTx.begin(), m_vtNewTx.begin() + nMax);
		}

        
		QList<AssetsDisplayInfo> listMyAsset;
		QMap<uint256, QList<TransactionRecord> > mapListTx;
		QList<CAssetData> listIssueAsset;

		{
			int nIndex = 0;
			LOCK2(cs_main, m_pWallet->cs_wallet);
			for (QMap<uint256, NewTxData>::iterator itNew = mapNewTxData.begin(); itNew != mapNewTxData.end() && !m_bIsExit; itNew++)
			{
				qDebug() << "CUpdateTransaction::decomposeTransaction: txHash" + QString::number(++nIndex) + ":  " + QString::fromStdString(itNew.key().ToString());
				std::map<uint256, CWalletTx>::iterator mi = m_pWallet->mapWallet.find(itNew.key());
				if (mi != m_pWallet->mapWallet.end())
				{
					QList<TransactionRecord> listTemp;
					if (TransactionRecord::decomposeTransaction(m_pWallet, mi->second, listTemp, listMyAsset, listIssueAsset))
					{
						mapListTx.insert(itNew.key(), listTemp);
					}
				}
			}

		}


		if (mapListTx.size() > 0)
		{
			Q_EMIT updateAllTransaction(mapListTx, mapNewTxData);
		}


		if (listMyAsset.size() > 0)
		{
			// update send coin page
			Q_EMIT updateAssetDisplayInfo(listMyAsset);

			// update overview page
			RefreshOverviewPageData(listMyAsset);
		}

		if (listIssueAsset.size() > 0)
		{
			// update asset page
			RefreshAssetData(listIssueAsset);

			// update candy page
			RefreshCandyPageData(listIssueAsset);
		}

		QString strDebug = "CUpdateTransaction::decomposeTransaction result: ";
		strDebug += "txCount: " + QString::number(mapListTx.size());
		strDebug += ", assetCount: " + QString::number(listMyAsset.size());
		strDebug += ", issueAssetCount: " + QString::number(listIssueAsset.size());
		qDebug() << strDebug;
    }
}

void CUpdateTransaction::setProcessingQueuedTransactions(bool value)
{ 
	m_bProcessingQueuedTransactions = value;
}

bool CUpdateTransaction::processingQueuedTransactions()
{
	return m_bProcessingQueuedTransactions;
}

void CUpdateTransaction::init(const WalletModel *pWalletModel, const CWallet *pWallet)
{
	m_pWallet = (CWallet *)pWallet;
	m_pWalletModel = (WalletModel *)pWalletModel;
	subscribeToCoreSignals();
}

void CUpdateTransaction::uninit()
{
	unsubscribeFromCoreSignals();
}