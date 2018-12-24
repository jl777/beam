// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "wallet/wallet_db.h"
#include <assert.h>
#include "test_helpers.h"
#include "utility/test_helpers.h"

#include "utility/logger.h"
#include <boost/filesystem.hpp>
#include <numeric>
uint32_t ASSETCHAINS_MAGIC = 0xdeadbeef;

using namespace std;
using namespace ECC;
using namespace beam;

WALLET_TEST_INIT

namespace
{
    IWalletDB::Ptr createSqliteWalletDB()
    {
        const char* dbName = "wallet.db";
        if (boost::filesystem::exists(dbName))
        {
            boost::filesystem::remove(dbName);
        }
        ECC::NoLeak<ECC::uintBig> seed;
        seed.V = Zero;
        auto walletDB = WalletDB::init(dbName, string("pass123"), seed);
        beam::Block::SystemState::ID id = { };
        id.m_Height = 134;
        walletDB->setSystemStateID(id);
        return walletDB;
    }
}

void TestWalletDataBase()
{
    cout << "Wallet database test\n";
    {
        Coin coin{ 1234 };
        coin.m_ID.m_Type = Key::Type::Regular;
        coin.m_ID.m_Idx = 132;
        WALLET_CHECK(coin.isValid());
        WALLET_CHECK(coin.m_ID.m_Idx == 132);
        WALLET_CHECK(coin.m_ID.m_Value == 1234);
        WALLET_CHECK(coin.m_ID.m_Type == Key::Type::Regular);
    }
    auto walletDB = createSqliteWalletDB();

    Coin coin1(5, Coin::Available, 10);
    walletDB->store(coin1);

    Coin coin2(2, Coin::Available, 10);
    walletDB->store(coin2);

    WALLET_CHECK(coin2.m_ID.m_Idx == coin1.m_ID.m_Idx + 1);

    {
        auto coins = walletDB->selectCoins(7);
        WALLET_CHECK(coins.size() == 2);

    
        vector<Coin> localCoins;
        localCoins.push_back(coin1);
        localCoins.push_back(coin2);

        for (size_t i = 0; i < coins.size(); ++i)
        {
            WALLET_CHECK(localCoins[i].m_ID.m_Idx == coins[i].m_ID.m_Idx);
            WALLET_CHECK(localCoins[i].m_ID.m_Value == coins[i].m_ID.m_Value);
            WALLET_CHECK(coins[i].m_status == Coin::Outgoing);
        }
    }

    {
        coin2.m_status = Coin::Spent;
        

        walletDB->save(coin2);

        WALLET_CHECK(walletDB->selectCoins(5).size() == 0);
    }

    {
        Block::SystemState::ID a;
        Hash::Processor() << static_cast<uint32_t>(rand()) >> a.m_Hash;
        a.m_Height = rand();

        const char* name = "SystemStateID";
        wallet::setVar(walletDB, name, "dummy");
        wallet::setVar(walletDB, name, a);

        Block::SystemState::ID b;
        WALLET_CHECK(wallet::getVar(walletDB, name, b));

        WALLET_CHECK(a == b);
    }
}

void TestStoreCoins()
{
    cout << "\nWallet database coins test\n";

    auto walletDB = createSqliteWalletDB();

  
    Coin coin = { 5, Coin::Available, 10, Key::Type::Coinbase };
    walletDB->store(coin);
    coin = { 4, Coin::Available, 10, Key::Type::Comission };
    walletDB->store(coin);
    coin = { 2, Coin::Available, 10, Key::Type::Regular };
    walletDB->store(coin);
    coin = { 5, Coin::Available, 10, Key::Type::Coinbase };
    walletDB->store(coin);
    coin = { 1, Coin::Available, 10, Key::Type::Regular };
    walletDB->store(coin);
    coin = { 5, Coin::Available, 10, Key::Type::Coinbase };
    walletDB->store(coin);
    coin = { 4, Coin::Available, 10, Key::Type::Comission };
    walletDB->store(coin);
    coin = { 1, Coin::Available, 10, Key::Type::Regular };
    walletDB->store(coin);
    coin = { 4, Coin::Available, 10, Key::Type::Comission };
    walletDB->store(coin);
    coin = { 1, Coin::Available, 10, Key::Type::Regular };
    walletDB->store(coin);
    coin = { 1, Coin::Available, 10, Key::Type::Regular };
    walletDB->store(coin);

    auto coins = vector<Coin>{
            Coin{ 5, Coin::Available, 10, Key::Type::Coinbase },
            Coin{ 4, Coin::Available, 10, Key::Type::Comission },
            Coin{ 2, Coin::Available, 10, Key::Type::Regular },
            Coin{ 5, Coin::Available, 10, Key::Type::Coinbase },
            Coin{ 1, Coin::Available, 10, Key::Type::Regular },
            Coin{ 5, Coin::Available, 10, Key::Type::Coinbase },
            Coin{ 4, Coin::Available, 10, Key::Type::Comission },
            Coin{ 1, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Comission },
            Coin{ 1, Coin::Available, 10, Key::Type::Regular },
            Coin{ 1, Coin::Available, 10, Key::Type::Regular } };
    walletDB->store(coins);


    int coinBase = 0;
    int comission = 0;
    int regular = 0;
    walletDB->visit([&coinBase, &comission, &regular](const Coin& coin)->bool
    {
        if (coin.m_ID.m_Type == Key::Type::Coinbase)
        {
            ++coinBase;
        }
        else if (coin.m_ID.m_Type == Key::Type::Comission)
        {
            ++comission;
        }
        else if (coin.m_ID.m_Type == Key::Type::Regular)
        {
            ++regular;
        }
        return true;
    });

    WALLET_CHECK(coinBase == 6);
    WALLET_CHECK(comission == 6);
    WALLET_CHECK(regular == 10);

    coins.clear();
    walletDB->visit([&coins](const auto& coin)->bool
    {
        coins.push_back(coin);
        return false;
    });
    WALLET_CHECK(coins[0].m_confirmHeight == MaxHeight);
    coins[0].m_confirmHeight = 423;
    walletDB->save(coins[0]);
    coins.clear();
    walletDB->visit([&coins](const auto& coin)->bool
    {
        coins.push_back(coin);
        return false;
    });
    beam::Merkle::Hash t;
    t = 12345678U;
    WALLET_CHECK(coins[0].m_confirmHeight == 423);
}
using namespace beam;
using namespace beam::wallet;
void TestStoreTxRecord()
{
    cout << "\nWallet database transactions test\n";
    auto walletDB = createSqliteWalletDB();
    TxID id = {{1, 3, 4, 5 ,65}};
    TxDescription tr;
    tr.m_txId = id;
    tr.m_amount = 34;
    tr.m_peerId.m_Pk = unsigned(23);
    tr.m_peerId.m_Channel = 0U;
    tr.m_myId.m_Pk = unsigned(42);
    tr.m_myId.m_Channel = 0U;
    tr.m_createTime = 123456;
    tr.m_minHeight = 134;
    tr.m_sender = true;
    tr.m_status = TxStatus::InProgress;
    tr.m_change = 5;

    WALLET_CHECK_NO_THROW(walletDB->saveTx(tr));
    WALLET_CHECK_NO_THROW(walletDB->saveTx(tr));
    TxDescription tr2 = tr;
    tr2.m_txId = id;
    tr2.m_amount = 43;
    tr2.m_minHeight = 234;
    tr2.m_createTime = 1234564;
    tr2.m_modifyTime = 12345644;
    tr2.m_status = TxStatus::Completed;
    tr2.m_change = 5;
    WALLET_CHECK_NO_THROW(walletDB->saveTx(tr2));
    
    auto t = walletDB->getTxHistory();
    WALLET_CHECK(t.size() == 1);
    WALLET_CHECK(t[0].m_txId == tr.m_txId);
    WALLET_CHECK(t[0].m_amount == tr.m_amount);
    WALLET_CHECK(t[0].m_minHeight == tr.m_minHeight);
    WALLET_CHECK(t[0].m_peerId == tr.m_peerId);
    WALLET_CHECK(t[0].m_myId == tr.m_myId);
    WALLET_CHECK(t[0].m_createTime == tr.m_createTime);
    WALLET_CHECK(t[0].m_modifyTime == tr2.m_modifyTime);
    WALLET_CHECK(t[0].m_sender == tr.m_sender);
    WALLET_CHECK(t[0].m_status == tr2.m_status);
    WALLET_CHECK(t[0].m_change == tr.m_change);
    TxID id2 = {{ 3,4,5 }};
    WALLET_CHECK_NO_THROW(walletDB->deleteTx(id2));
    WALLET_CHECK_NO_THROW(walletDB->deleteTx(id));

    WALLET_CHECK_NO_THROW(walletDB->saveTx(tr2));
    WALLET_CHECK_NO_THROW(walletDB->saveTx(tr2));
    boost::optional<TxDescription> tr3;
    WALLET_CHECK_NO_THROW(tr3 = walletDB->getTx(tr2.m_txId));
    WALLET_CHECK(tr3.is_initialized());
    WALLET_CHECK(tr3->m_txId == tr2.m_txId);
    WALLET_CHECK(tr3->m_amount == tr2.m_amount);
    WALLET_CHECK(tr3->m_peerId == tr2.m_peerId);
    WALLET_CHECK(tr3->m_myId == tr2.m_myId);
    WALLET_CHECK(tr3->m_message == tr2.m_message);
    WALLET_CHECK(tr3->m_createTime == tr2.m_createTime);
    WALLET_CHECK(tr3->m_modifyTime == tr2.m_modifyTime);
    WALLET_CHECK(tr3->m_sender == tr2.m_sender);
    WALLET_CHECK(tr3->m_status == tr2.m_status);
    WALLET_CHECK_NO_THROW(walletDB->deleteTx(tr2.m_txId));
    WALLET_CHECK(walletDB->getTxHistory().empty());

    for (uint8_t i = 0; i < 100; ++i)
    {
        tr.m_txId[0] = i;
        WALLET_CHECK_NO_THROW(walletDB->saveTx(tr));
    }
    WALLET_CHECK(walletDB->getTxHistory().size() == 100);
    t = walletDB->getTxHistory(50, 2);
    WALLET_CHECK(t.size() == 2);
    t = walletDB->getTxHistory(99, 10);
    WALLET_CHECK(t.size() == 1);
    t = walletDB->getTxHistory(9, 0);
    WALLET_CHECK(t.size() == 0);
    t = walletDB->getTxHistory(50, 2);
    id[0] = 50;
    WALLET_CHECK(t[0].m_txId == id);
    id[0] = 51;
    WALLET_CHECK(t[1].m_txId == id);

    t = walletDB->getTxHistory(0, 1);
    WALLET_CHECK(t.size() == 1 && t[0].m_txId[0] == 0);

    t = walletDB->getTxHistory(99, 1);
    WALLET_CHECK(t.size() == 1 && t[0].m_txId[0] == 99);

    t = walletDB->getTxHistory(100, 1);
    WALLET_CHECK(t.size() == 0);
}

void TestRollback()
{
    cout << "\nWallet database rollback test\n";
    auto db = createSqliteWalletDB();
    for (uint64_t i = 0; i < 9; ++i)
    {
        Coin coin1 = { 5, Coin::Available, i + 10, Key::Type::Regular, Height(i + 1) };
        db->store(coin1);
    }

    for (uint64_t i = 9; i < 10; ++i)
    {
        Coin coin1 = { 5, Coin::Spent, 0, Key::Type::Regular, Height(1), Height(i + 1) };
        db->store(coin1);
    }

    // was created after branch
    {
        Coin coin1 = { 5, Coin::Spent, 7, Key::Type::Regular, Height(8) };
        db->store(coin1);
    }


    // rewards
    // should not be deleted
    {
        Coin coin1 = { 5, Coin::Spent, 8, Key::Type::Regular, Height(8) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Spent, 8, Key::Type::Regular, Height(8) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Available, 9, Key::Type::Regular, Height(9) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Available, 9, Key::Type::Regular, Height(9) };
        db->store(coin1);
    }
    // should be preserved
    {
        Coin coin1 = { 5, Coin::Spent, 7, Key::Type::Regular, Height(7) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Spent, 7, Key::Type::Regular, Height(7) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Spent, 5, Key::Type::Regular, Height(5) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Spent, 5, Key::Type::Regular, Height(5) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Available, 4, Key::Type::Regular, Height(4) };
        db->store(coin1);
    }

    {
        Coin coin1 = { 5, Coin::Available, 4, Key::Type::Regular, Height(4) };
        db->store(coin1);
    }

    db->rollbackConfirmedUtxo(6);

    vector<Coin> coins;
    db->visit([&coins](const auto& c)->bool
    {
        coins.push_back(c);
        return true;
    });

    WALLET_CHECK(coins.size() == 21);

    for (int i = 0; i < 5; ++i)
    {
        auto& c = coins[i];
        WALLET_CHECK(c.m_status == Coin::Available);
        WALLET_CHECK(c.m_confirmHeight != MaxHeight);
        WALLET_CHECK(c.m_lockedHeight == MaxHeight);
    }

    for (int i = 6; i < 9; ++i)
    {
        auto& c = coins[i];
        WALLET_CHECK(c.m_status == Coin::Unavailable);
        WALLET_CHECK(c.m_confirmHeight == MaxHeight);
        WALLET_CHECK(c.m_lockedHeight == MaxHeight);
    }
    for (int i = 9; i < 10; ++i)
    {
        auto& c = coins[i];
        WALLET_CHECK(c.m_status == Coin::Available);
        WALLET_CHECK(c.m_confirmHeight != MaxHeight);
        WALLET_CHECK(c.m_lockedHeight == MaxHeight);
    }

    {
        // for now it is unconfirmed in future we would have to distinguish such coins
        auto& c = coins[10];
        WALLET_CHECK(c.m_status == Coin::Unavailable);
        WALLET_CHECK(c.m_confirmHeight == MaxHeight);
        WALLET_CHECK(c.m_lockedHeight == MaxHeight);
    }

    for (int i = 11; i < 17; ++i)
    {
        auto& c = coins[i];
        WALLET_CHECK(c.m_status == Coin::Unavailable);
        WALLET_CHECK(c.m_confirmHeight == MaxHeight);
        WALLET_CHECK(c.m_lockedHeight == MaxHeight);
    }
    for (int i = 17; i < 19; ++i)
    {
        auto& c = coins[i];
        WALLET_CHECK(c.m_status == Coin::Spent);
        WALLET_CHECK(c.m_confirmHeight != MaxHeight);
        WALLET_CHECK(c.m_lockedHeight == MaxHeight);
    }
    for (int i = 19; i < 21; ++i)
    {
        auto& c = coins[i];
        WALLET_CHECK(c.m_status == Coin::Available);
        WALLET_CHECK(c.m_confirmHeight != MaxHeight);
        WALLET_CHECK(c.m_lockedHeight == MaxHeight);
    }
}

void TestTxRollback()
{
    cout << "\nWallet database transaction rollback test\n";

    auto walletDB = createSqliteWalletDB();
    TxID id = { { 1, 3, 4, 5 ,65 } };
    TxID id2 = { {1, 3, 4} };

    Coin coin1 = { 5, Coin::Available, 10, Key::Type::Coinbase };
    walletDB->store(coin1);
    Coin coin2 = { 6, Coin::Available, 11, Key::Type::Coinbase };
    walletDB->store(coin2);
    
    coin2.m_spentTxId = id;
    coin2.m_status = Coin::Outgoing;
    
    Coin coin3 = { 3, Coin::Incoming, 2 };
    coin3.m_createTxId = id;
    walletDB->store(coin3);
    walletDB->save({ coin2 });

    vector<Coin> coins;
    walletDB->visit([&coins](const Coin& c)->bool
    {
        coins.push_back(c);
        return true;
    });

    WALLET_CHECK(coins.size() == 3);

    WALLET_CHECK(coins[1].m_status == Coin::Outgoing);
    WALLET_CHECK(coins[1].m_spentTxId == id);
    WALLET_CHECK(coins[2].m_status == Coin::Incoming);
    WALLET_CHECK(coins[2].m_createTxId == id);

    walletDB->rollbackTx(id2);

    coins.clear();
    walletDB->visit([&coins](const Coin& c)->bool
    {
        coins.push_back(c);
        return true;
    });
    WALLET_CHECK(coins.size() == 3);

    walletDB->rollbackTx(id);

    coins.clear();
    walletDB->visit([&coins](const Coin& c)->bool
    {
        coins.push_back(c);
        return true;
    });

    WALLET_CHECK(coins.size() == 2);
    WALLET_CHECK(coins[1].m_status == Coin::Available);
    WALLET_CHECK(coins[1].m_spentTxId.is_initialized() == false);
}

void TestAddresses()
{
    cout << "\nWallet database addresses test\n";
    auto db = createSqliteWalletDB();
    auto addresses = db->getAddresses(true);
    WALLET_CHECK(addresses.empty());
    addresses = db->getAddresses(false);
    WALLET_CHECK(addresses.empty());

    WalletAddress a = {};
    a.m_walletID.m_Pk = unsigned(9876543);
    a.m_walletID.m_Channel = 0U;
    a.m_label = "test label";
    a.m_category = "test category";
    a.m_createTime = beam::getTimestamp();
    a.m_duration = 23;
    a.m_OwnID = 44;

    db->saveAddress(a);

    addresses = db->getAddresses(true);
    WALLET_CHECK(addresses.size() == 1);
    WALLET_CHECK(addresses[0].m_walletID == a.m_walletID);
    WALLET_CHECK(addresses[0].m_label == a.m_label);
    WALLET_CHECK(addresses[0].m_category == a.m_category);
    WALLET_CHECK(addresses[0].m_createTime == a.m_createTime);
    WALLET_CHECK(addresses[0].m_duration == a.m_duration);
    WALLET_CHECK(addresses[0].m_OwnID == a.m_OwnID);

    a.m_category = "cat2";

    db->saveAddress(a);

    addresses = db->getAddresses(true);
    WALLET_CHECK(addresses.size() == 1);
    WALLET_CHECK(addresses[0].m_walletID == a.m_walletID);
    WALLET_CHECK(addresses[0].m_label == a.m_label);
    WALLET_CHECK(addresses[0].m_category == a.m_category);
    WALLET_CHECK(addresses[0].m_createTime == a.m_createTime);
    WALLET_CHECK(addresses[0].m_duration == a.m_duration);
    WALLET_CHECK(addresses[0].m_OwnID == a.m_OwnID);

    auto a2 = db->getAddress(a.m_walletID);
    WALLET_CHECK(a2.is_initialized());

    db->deleteAddress(a.m_walletID);
    WALLET_CHECK(db->getAddresses(true).empty());

    a2 = db->getAddress(a.m_walletID);
    WALLET_CHECK(!a2.is_initialized());
}

vector<Coin::ID> ExtractIDs(const vector<Coin>& src)
{
    vector<Coin::ID> res;
    res.reserve(src.size());
    for (const Coin& c : src)
        res.push_back(c.m_ID);
    return res;
}

void TestSelect()
{
    cout << "\nWallet database coin selection 1 test\n";
    auto db = createSqliteWalletDB();
    const Amount c = 10;
    for (Amount i = 1; i <= c; ++i)
    {
        Coin coin{ i, Coin::Available, 10, Key::Type::Regular };
        db->store(coin);
    }
    for (Amount i = 1; i <= c; ++i)
    {
        auto coins = db->selectCoins(i, false);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Value == i);
    }
    {
        auto coins = db->selectCoins(56, false);
        WALLET_CHECK(coins.empty());
    }
    {
        auto coins = db->selectCoins(55, false);
        WALLET_CHECK(coins.size() == 10);
    }
    {
        auto coins = db->selectCoins(15, false);
        WALLET_CHECK(coins.size() == 2);
    }
    for (Amount i = c + 1; i <= 55; ++i)
    {
        auto coins = db->selectCoins(i, false);
        WALLET_CHECK(!coins.empty());
    }
    // double coins
    for (Amount i = 1; i <= c; ++i)
    {
        Coin coin{ i, Coin::Available, 10, Key::Type::Regular };
        db->store(coin);
    }
    for (Amount i = 1; i <= c; ++i)
    {
        auto coins = db->selectCoins(i, false);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Value == i);
    }
    {
        auto coins = db->selectCoins(111, false);
        WALLET_CHECK(coins.empty());
    }
    {
        auto coins = db->selectCoins(110, false);
        WALLET_CHECK(coins.size() == 20);
    }
    for (Amount i = c + 1; i <= 110; ++i)
    {
        auto coins = db->selectCoins(i, false);
        WALLET_CHECK(!coins.empty());
        auto sum = accumulate(coins.begin(), coins.end(), Amount(0), [](const auto& left, const auto& right) {return left + right.m_ID.m_Value; });
        WALLET_CHECK(sum == i);
    }

    {
    db->remove(ExtractIDs(db->selectCoins(110, false)));
    vector<Coin> coins = {
        Coin{ 2, Coin::Available, 10, Key::Type::Regular },
        Coin{ 1, Coin::Available, 10, Key::Type::Regular },
        Coin{ 9, Coin::Available, 10, Key::Type::Regular } };
    db->store(coins);
    coins = db->selectCoins(6, false);
    WALLET_CHECK(coins.size() == 1);
    WALLET_CHECK(coins[0].m_ID.m_Value == 9);
    }
    {
        db->remove(ExtractIDs(db->selectCoins(12, false)));
        vector<Coin> coins = {
            Coin{ 2, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular } };
        db->store(coins);
        coins = db->selectCoins(5, false);
        WALLET_CHECK(coins.size() == 2);
        WALLET_CHECK(coins.back().m_ID.m_Value == 2);
    }
    {
        db->remove(ExtractIDs(db->selectCoins(18, false)));
        vector<Coin> coins = {
            Coin{ 4, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular } };
        db->store(coins);
        coins = db->selectCoins(1, false);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Value == 4);
    }
    {
        db->remove(ExtractIDs(db->selectCoins(16, false)));
        vector<Coin> coins = {
            Coin{ 3, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular },
            Coin{ 5, Coin::Available, 10, Key::Type::Regular },
            Coin{ 7, Coin::Available, 10, Key::Type::Regular } };
        db->store(coins);
        coins = db->selectCoins(6, false);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Value == 7);
    }
    {
        db->remove(ExtractIDs(db->selectCoins(19, false)));
        vector<Coin> coins = {
            Coin{ 1, Coin::Available, 10, Key::Type::Regular },
            Coin{ 2, Coin::Available, 10, Key::Type::Regular },
            Coin{ 3, Coin::Available, 10, Key::Type::Regular },
            Coin{ 4, Coin::Available, 10, Key::Type::Regular } };
        db->store(coins);
        coins = db->selectCoins(4, false);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Value == 4);

        coins = db->selectCoins(7, false);
        WALLET_CHECK(coins.size() == 2);
        WALLET_CHECK(coins[0].m_ID.m_Value == 4);
        WALLET_CHECK(coins[1].m_ID.m_Value == 3);
    }
    {
        db->remove(ExtractIDs(db->selectCoins(10, false)));
        vector<Coin> coins = {
            Coin{ 2, Coin::Available, 10, Key::Type::Regular },
            Coin{ 5, Coin::Available, 10, Key::Type::Regular },
            Coin{ 7, Coin::Available, 10, Key::Type::Regular } };
        db->store(coins);
        coins = db->selectCoins(6, false);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Value == 7);
    }
    {
        db->remove(ExtractIDs(db->selectCoins(14, false)));
        vector<Coin> coins = {
            Coin{ 235689, Coin::Available, 10, Key::Type::Regular },
            Coin{ 2999057, Coin::Available, 10, Key::Type::Regular },
            Coin{ 500000, Coin::Available, 10, Key::Type::Regular },
            Coin{ 5000000, Coin::Available, 10, Key::Type::Regular },
            Coin{ 40000000, Coin::Available, 10, Key::Type::Regular },
            Coin{ 40000000, Coin::Available, 10, Key::Type::Regular },
            Coin{ 40000000, Coin::Available, 10, Key::Type::Regular },
        };
        db->store(coins);
        coins = db->selectCoins(41000000, false);
        WALLET_CHECK(coins.size() == 2);
        WALLET_CHECK(coins[1].m_ID.m_Value == 2999057);
        WALLET_CHECK(coins[0].m_ID.m_Value == 40000000);
        coins = db->selectCoins(4000000, false);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Value == 5000000);
    }
}

vector<Coin> SelectCoins(IWalletDB::Ptr db, Amount amount, bool printCoins = true)
{
    helpers::StopWatch sw;
    cout << "Selecting " << amount << " Groths\n";
    sw.start();
    auto selectedCoins = db->selectCoins(amount, false);
    sw.stop();
    cout << "Elapsed time: " << sw.milliseconds() << " ms\n";

    Amount sum = 0;
    for (auto coin : selectedCoins)
    {
        sum += coin.m_ID.m_Value;
        if (printCoins)
        {
            LOG_INFO() << coin.m_ID.m_Value;
        }
    }
    LOG_INFO() << "sum = " << sum << " change = " << sum - amount;
    WALLET_CHECK(amount <= sum);
#ifdef NDEBUG
    WALLET_CHECK(sw.milliseconds() <= 1000);
#endif // !NDEBUG

    return selectedCoins;
}

void TestSelect2()
{
    cout << "\nWallet database coin selection 2 test\n";
    auto db = createSqliteWalletDB();
    const Amount c = 100000;
    vector<Coin> t;
    t.reserve(c);
    for (Amount i = 1; i <= c; ++i)
    {
        t.push_back(Coin( 40000000, Coin::Available, 10, Key::Type::Regular ));
    }
    db->store(t);
    {
        Coin coin{ 30000000, Coin::Available, 10, Key::Type::Regular };
        db->store(coin);
    }

    auto coins = SelectCoins(db, 347'000'000, false);

    WALLET_CHECK(coins.size() == 9);
    WALLET_CHECK(coins[8].m_ID.m_Value == 30000000);
}

void TestTxParameters()
{
    cout << "\nWallet database transaction parameters test\n";
    auto db = createSqliteWalletDB();
    TxID txID = { {1, 3, 5} };
    // public parameter cannot be overriten
    Amount amount = 0;
    WALLET_CHECK(!wallet::getTxParameter(db, txID, TxParameterID::Amount, amount));
    WALLET_CHECK(amount == 0);
    WALLET_CHECK(wallet::setTxParameter(db, txID, TxParameterID::Amount, 8765, false));
    WALLET_CHECK(wallet::getTxParameter(db, txID, TxParameterID::Amount, amount));
    WALLET_CHECK(amount == 8765);
    WALLET_CHECK(!wallet::setTxParameter(db, txID, TxParameterID::Amount, 786, false));
    WALLET_CHECK(wallet::getTxParameter(db, txID, TxParameterID::Amount, amount));
    WALLET_CHECK(amount == 8765);

    // private parameter can be overriten
    TxStatus status = TxStatus::Pending;
    WALLET_CHECK(!wallet::getTxParameter(db, txID, TxParameterID::Status, status));
    WALLET_CHECK(status == TxStatus::Pending);
    WALLET_CHECK(wallet::setTxParameter(db, txID, TxParameterID::Status, TxStatus::Completed, false));
    WALLET_CHECK(wallet::getTxParameter(db, txID, TxParameterID::Status, status));
    WALLET_CHECK(status == TxStatus::Completed);
    WALLET_CHECK(wallet::setTxParameter(db, txID, TxParameterID::Status, TxStatus::InProgress, false));
    WALLET_CHECK(wallet::getTxParameter(db, txID, TxParameterID::Status, status));
    WALLET_CHECK(status == TxStatus::InProgress);

    // check different types

    ByteBuffer b = { 1, 2, 3 };
    WALLET_CHECK(wallet::setTxParameter(db, txID, TxParameterID::Status, b, false));
    ByteBuffer b2;
    WALLET_CHECK(wallet::getTxParameter(db, txID, TxParameterID::Status, b2));
    WALLET_CHECK(equal(b.begin(), b.end(), b2.begin(), b2.end()));

    ECC::Scalar::Native s, s2;
	s = 123U;
    WALLET_CHECK(wallet::setTxParameter(db, txID, TxParameterID::BlindingExcess, s, false));
    WALLET_CHECK(wallet::getTxParameter(db, txID, TxParameterID::BlindingExcess, s2));
    WALLET_CHECK(s == s2);

    ECC::Point p;
    p.m_X = unsigned(143521);
    p.m_Y = 0;
    ECC::Point::Native pt, pt2;
    pt.Import(p);
    WALLET_CHECK(wallet::setTxParameter(db, txID, TxParameterID::PeerPublicNonce, pt, false));
    WALLET_CHECK(wallet::getTxParameter(db, txID, TxParameterID::PeerPublicNonce, pt2));
    WALLET_CHECK(p == pt2);
}

void TestSelect3()
{
    cout << "\nWallet database coin selection 3 test\n";
    auto db = createSqliteWalletDB();
    const uint32_t count = 2500;
    const uint32_t partCount = 100;
    Amount startAmount = 80'000'000;
    vector<Coin> coins;
    coins.reserve(count);

    for (uint32_t i = 1; i <= count; ++i)
    {
        Amount a = Amount(startAmount / pow(2, i / partCount));

        coins.push_back(Coin(a, Coin::Available, 10, Key::Type::Regular));
    }

    db->store(coins);
    {
        Coin coin{ 30'000'000, Coin::Available, 10, Key::Type::Regular };
        db->store(coin);
    }

    SelectCoins(db, 45'678'910);
}

void TestSelect4()
{
    cout << "\nWallet database coin selection 4 test\n";
    auto db = createSqliteWalletDB();
    vector<Coin> coins;

    coins.push_back(Coin(2, Coin::Available, 10, Key::Type::Regular));

    coins.push_back(Coin(30101, Coin::Available, 10, Key::Type::Regular));
    coins.push_back(Coin(30102, Coin::Available, 10, Key::Type::Regular));
    coins.push_back(Coin(32000, Coin::Available, 10, Key::Type::Regular));

    db->store(coins);

    SelectCoins(db, 60203);
}

void TestSelect5()
{
    cout << "\nWallet database coin selection 5 test\n";
    auto db = createSqliteWalletDB();
    const uint32_t count = 10000;
    const uint32_t partCount = 400;
    Amount startAmount = 40'000'000;
    vector<Coin> coins;
    coins.reserve(count);

    for (uint32_t i = 1; i <= count; ++i)
    {
        Amount a = Amount(startAmount / pow(2, i / partCount));

        coins.push_back(Coin(a, Coin::Available, 10, Key::Type::Regular));
    }

    db->store(coins);
    {
        Coin coin{ 30'000'000, Coin::Available, 10, Key::Type::Regular };
        db->store(coin);
    }

    SelectCoins(db, 45'678'910);
}

void TestSelect6()
{
    cout << "\nWallet database coin selection 6 test\n";
    const uint32_t count = 100000;
    {
        auto db = createSqliteWalletDB();
        
        vector<Coin> coins;
        coins.reserve(count);

        for (uint32_t i = 1; i <= count; ++i)
        {
            coins.push_back(Coin(Amount(i), Coin::Available, 10, Key::Type::Regular));
        }

        db->store(coins);

        SelectCoins(db, 450'678'910, false);
    }

    {
        auto db = createSqliteWalletDB();

        vector<Coin> coins;
        coins.reserve(count);

        for (uint32_t i = 1; i <= count; ++i)
        {
            Amount amount = rand();
            coins.push_back(Coin(amount, Coin::Available, 10, Key::Type::Regular));
        }

        db->store(coins);

        SelectCoins(db, 450'678'910, false);
    }

    {
        auto db = createSqliteWalletDB();

        vector<Coin> coins;
        coins.reserve(count);

        for (uint32_t i = 1; i <= count; ++i)
        {
            Amount amount = rand() % 10000;
            coins.push_back(Coin(amount, Coin::Available, 10, Key::Type::Regular));
        }

        db->store(coins);

        SelectCoins(db, 450'678'910, false);
    }
    {
        auto db = createSqliteWalletDB();

        vector<Coin> coins;
        coins.reserve(count);

        for (uint32_t i = 1; i <= count; ++i)
        {
            Amount amount = 1000000 + rand() % 100000;
            coins.push_back(Coin(amount, Coin::Available, 10, Key::Type::Regular));
        }

        db->store(coins);

        SelectCoins(db, 450'678'910, false);
    }
}

void TestTransferredByTx()
{
    cout << "\nWallet database test: calculate sums of spent & received in transactions\n";
    auto walletDB = createSqliteWalletDB();

    Amount amount = 10;
    Amount kAmountStep = 5;
    uint64_t kMaxCount = 1000;
    Amount totalAmount = 0;

    TxDescription txTemplate;
    txTemplate.m_peerId.m_Pk = unsigned(23);
    txTemplate.m_peerId.m_Channel = 0U;
    txTemplate.m_myId.m_Pk = unsigned(42);
    txTemplate.m_myId.m_Channel = 0U;
    txTemplate.m_createTime = 123456;
    txTemplate.m_minHeight = 134;
    txTemplate.m_status = TxStatus::Completed;
    txTemplate.m_change = 10;

    cout << "Create & Save " << kMaxCount << " transactions to database" << "\n";

    for (uint64_t i = 0; i < kMaxCount; ++i, amount += kAmountStep)
    {
        // i -> txID
        std::copy((uint8_t*)&i, ((uint8_t*)&i) + sizeof(uint64_t), txTemplate.m_txId.begin());

        txTemplate.m_amount = amount;
        txTemplate.m_sender = (i > kMaxCount / 2);

        WALLET_CHECK_NO_THROW(walletDB->saveTx(txTemplate));

        totalAmount += amount;

        if (i == kMaxCount / 2) 
        {
            amount = kAmountStep;
        }
    }

    // calculate sum
    helpers::StopWatch sw;

    cout << "Calculate sums using GetTxHistory" << "\n";
    sw.start();
    auto txHistory = walletDB->getTxHistory();
    Amount historySumSent = 0;
    Amount historySumReceived = 0;

    for (const auto& item : txHistory)
    {
        switch (item.m_status)
        {
        case TxStatus::Completed:
            (item.m_sender ? historySumSent : historySumReceived) += item.m_amount;
            break;
        default: break;
        }
    }
    sw.stop();
    cout << "Elapsed time: " << sw.milliseconds() << " ms\n";

    cout << "Calculate sums using getTransferredByTx" << "\n";
    sw.start();
    auto sumSent = wallet::getSpentByTx(walletDB, TxStatus::Completed);
    auto sumReceived = wallet::getReceivedByTx(walletDB, TxStatus::Completed);

    sw.stop();
    cout << "Elapsed time: " << sw.milliseconds() << " ms\n";

    WALLET_CHECK(historySumReceived == sumReceived);
    WALLET_CHECK(historySumSent == sumSent);
}

int main() 
{
    int logLevel = LOG_LEVEL_DEBUG;
#if LOG_VERBOSE_ENABLED
    logLevel = LOG_LEVEL_VERBOSE;
#endif
    auto logger = beam::Logger::create(logLevel, logLevel);
    ECC::InitializeContext();

    TestWalletDataBase();
    TestStoreCoins();
    TestStoreTxRecord();
    TestTxRollback();
    TestRollback();
    TestSelect();
    TestSelect2();
    TestSelect3();
    TestSelect4();
    TestSelect5();
    TestSelect6();
    TestAddresses();

    TestTxParameters();

    /*TestTransferredByTx();*/


    return WALLET_CHECK_RESULT;
}
