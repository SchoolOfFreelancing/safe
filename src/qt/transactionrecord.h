// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONRECORD_H
#define BITCOIN_QT_TRANSACTIONRECORD_H

#include "amount.h"
#include "uint256.h"
#include "app/app.h"
#include "wallet/wallet.h"

#include <QList>
#include <QString>
#include <QMap>
#include <vector>

class CWallet;
class CWalletTx;
class CAppData;
class CTxOut;

enum{SHOW_TX = 0, SHOW_LOCKED_TX = 1, SHOW_CANDY_TX = 2,SHOW_ASSETS_DISTRIBUTE=3,SHOW_APPLICATION_REGIST=4,SHOW_ALL=99};

enum{APP_Dev_Type_Company = 1,APP_Dev_Type_Personal = 2};

enum{ASSETS_FIRST_DISTRIBUTE=1,ASSETS_ADD_DISTIRBUE=2};

class AssetsDisplayInfo
{
public:
    AssetsDisplayInfo()
    {
        strAssetsUnit = "";
        bInMainChain = false;
    }
    ~AssetsDisplayInfo(){}

	friend inline bool operator==(const AssetsDisplayInfo& a, const AssetsDisplayInfo& b) { return a.strAssetName == b.strAssetName; }

	friend inline bool operator==(const AssetsDisplayInfo& a, const QString& b) { return a.strAssetName == b; }

	friend inline bool operator==(const QString& a, const AssetsDisplayInfo& b) { return a == b.strAssetName; }

public:
	QString strAssetName;
    QString strAssetsUnit;
    bool bInMainChain;
};

/** UI model for transaction status. The transaction status is the part of a transaction that will change over time.
 */
class TransactionStatus
{
public:
    TransactionStatus():
        countsForBalance(false), sortKey(""),
        matures_in(0), status(Offline), depth(0), open_for(0), cur_num_blocks(-1)
    { }

    enum Status {
        Confirmed,          /**< Have 6 or more confirmations (normal tx) or fully mature (mined tx) **/
        /// Normal (sent/received) transactions
        OpenUntilDate,      /**< Transaction not yet final, waiting for date */
        OpenUntilBlock,     /**< Transaction not yet final, waiting for block */
        Offline,            /**< Not sent to any other nodes **/
        Unconfirmed,        /**< Not yet mined into a block **/
        Confirming,         /**< Confirmed, but waiting for the recommended number of confirmations **/
        Conflicted,         /**< Conflicts with other transaction or mempool **/
        Abandoned,          /**< Abandoned from the wallet **/
        /// Generated (mined) transactions
        Immature,           /**< Mined but waiting for maturity */
        MaturesWarning,     /**< Transaction will likely not mature because no nodes have confirmed */
        NotAccepted         /**< Mined but not accepted */
    };

    /// Transaction counts towards available balance
    bool countsForBalance;
    /// Sorting key based on status
    std::string sortKey;

    /** @name Generated (mined) transactions
       @{*/
    int matures_in;
    /**@}*/

    /** @name Reported status
       @{*/
    Status status;
    qint64 depth;
    qint64 open_for; /**< Timestamp if status==OpenUntilDate, otherwise number
                      of additional blocks that need to be mined before
                      finalization */
    /**@}*/

    /** Current number of blocks (to know whether cached status is still valid) */
    int cur_num_blocks;

    //** Know when to update transaction for ix locks **/
    int cur_num_ix_locks;
};

/** UI model for a transaction. A core transaction can be represented by multiple UI transactions if it has
    multiple outputs.
 */
class TransactionRecord
{
public:
    enum Type
    {
        Other,
        Generated,
        SendToAddress,
        SendToOther,
        RecvWithAddress,
        RecvFromOther,
        SendToSelf,
        RecvWithPrivateSend,
        PrivateSendDenominate,
        PrivateSendCollateralPayment,
        PrivateSendMakeCollaterals,
        PrivateSendCreateDenominations,
        PrivateSend,
        FirstDistribute,
        AddDistribute,
        PUTCandy,
        GETCandy
    };

    /** Number of confirmation recommended for accepting a transaction */
    static const int RecommendedNumConfirmations = 6;

    TransactionRecord():
        hash(), time(0), type(Other), address(""), debit(0), assetDebit(0),credit(0),assetCredit(0),bSAFETransaction(true),bLocked(false), nLockedAmount(0),nLastLockStatus(false),nUnlockedHeight(0)
        ,bApp(false),appData(),bAssets(false),assetsData(),commonData(),bPutCandy(false),putCandyData(),bGetCandy(false),getCandyData(),appHeader(),bIssueAsset(false),transferSafeData()
        ,bForbidDash(false),idx(0),nTxHeight(0),strLockedMonth(""), nVersion(0)
    {
        vtShowType.push_back(SHOW_TX);
    }

    TransactionRecord(uint256 hash, qint64 time, const int32_t& nVersion):
            hash(hash), time(time), type(Other), address(""), debit(0),assetDebit(0),credit(0),assetCredit(0),bSAFETransaction(true), bLocked(false), nLockedAmount(0),nLastLockStatus(false),nUnlockedHeight(0)
            ,bApp(false),appData(),bAssets(false),assetsData(),commonData(),bPutCandy(false),putCandyData(),bGetCandy(false),getCandyData(),appHeader(),bIssueAsset(false),transferSafeData()
            ,bForbidDash(false),idx(0),nTxHeight(0),strLockedMonth(""), nVersion(nVersion)
    {
        vtShowType.push_back(SHOW_TX);
    }

    TransactionRecord(uint256 hash, qint64 time,
                Type type, const std::string &address,
                const CAmount& debit, const CAmount& credit, const int32_t& nVersion):
            hash(hash), time(time), type(type), address(address), debit(debit), assetDebit(0),credit(credit),assetCredit(0),bSAFETransaction(true),bLocked(false), nLockedAmount(0),nLastLockStatus(false)
            ,nUnlockedHeight(0),bApp(false),appData(),bAssets(false),assetsData(),commonData(),bPutCandy(false),putCandyData(),bGetCandy(false),getCandyData(),bIssueAsset(false),appHeader(),transferSafeData()
            ,bForbidDash(false),idx(0),nTxHeight(0),strLockedMonth(""), nVersion(nVersion)
    {
        vtShowType.push_back(SHOW_TX);
    }

	friend inline bool operator==(const TransactionRecord& a, const TransactionRecord& b) { return a.hash == b.hash; }

	friend inline bool operator==(const TransactionRecord& a, const uint256& b) { return a.hash == b; }

	friend inline bool operator==(const uint256& a, const TransactionRecord& b) { return a == b.hash; }

    /** Decompose CWallet transaction to model transaction records.
     */
    static bool showTransaction(const CWalletTx &wtx);

    int64_t getRealUnlockHeight()const;

	static void addAssetDisplay(const CWalletTx &wtx, const CAssetData &stAssetData, QList<AssetsDisplayInfo> &listMyAsset);

	static void addIssueAsset(const CAssetData &stAssetData, QList<CAssetData> &listIssueAsset);

	static bool decomposeAppAsset(const CWallet *wallet,
        const CWalletTx &wtx,
		TransactionRecord &sub,
        const CTxOut &txout,
		QList<AssetsDisplayInfo> &listMyAsset,
        QList<CAssetData> &listIssueAsset,
        isminetype fAllFromMe);

    static bool decomposeAppAssetSafe(const CWallet *wallet,const CWalletTx &wtx,const CTxOut& txout,TransactionRecord& sub,int nOut,const CAmount& nDebit,
                                      isminetype fAllFromMe, isminetype fAllToMe,std::map<std::string, std::string>& mapValue);

    static bool decomposeTransferSafeData(const CWalletTx &wtx, TransactionRecord &sub, const CTxOut &txout);

    static bool decomposeLockTx(const CWalletTx &wtx, TransactionRecord &sub, const CTxOut &txout,isminetype fAllFromMe);

	static void setAddressType(isminetype fAllFromMe, isminetype fAllToMe, const CWalletTx &wtx, TransactionRecord &sub, const CTxOut &txout);

	static bool decomposeTransaction(const CWallet *wallet, 
		const CWalletTx &wtx,
		QList<TransactionRecord> &listTransaction,
		QList<AssetsDisplayInfo> &listMyAsset,
		QList<CAssetData> &listIssueAsset);

    /** @name Immutable transaction attributes
      @{*/
    uint256 hash;
    qint64 time;
    Type type;
    std::string address;
    CAmount debit;
    CAmount assetDebit;
    CAmount credit;
    CAmount assetCredit;
    bool bSAFETransaction;
    bool bLocked;
    CAmount nLockedAmount;
    bool nLastLockStatus;
    int64_t nUnlockedHeight;
    bool bApp;
    CAppData appData;
    bool bAssets;
    CAssetData assetsData;
    CCommonData commonData;
    bool bPutCandy;
    CPutCandyData putCandyData;
    bool bGetCandy;
    CGetCandyData getCandyData;
    bool bIssueAsset;
    CAppHeader appHeader;
    CTransferSafeData transferSafeData;
    /**@}*/

    //forbid dash candy
    bool bForbidDash;

    /** Subtransaction index, for sort key */
    int idx;

	std::vector<int> vtShowType;

    int64_t nTxHeight;
    QString strLockedMonth;

    int32_t nVersion;

    /** Status: can change with block chain update */
    TransactionStatus status;

    /** Whether the transaction was sent/received with a watch-only address */
    bool involvesWatchAddress;

    /** Return the unique identifier for this transaction (part) */
    QString getTxID() const;

    /** Format subtransaction id */
    static QString formatSubTxId(const uint256 &hash, int vout);

    /** Update status from core wallet tx.
     */
    bool updateStatus(const CWalletTx &wtx);

    /** Return whether a status update is needed.
     */
    bool statusUpdateNeeded();

    void updateLockedMonth();
};


bool TRTimeGreaterCompartor(const TransactionRecord& a, const TransactionRecord& b);

bool TRTimeLessCompartor(const TransactionRecord& a, const TransactionRecord& b);


#endif // BITCOIN_QT_TRANSACTIONRECORD_H
