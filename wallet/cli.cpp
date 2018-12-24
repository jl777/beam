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

#include "wallet/wallet_network.h"
#include "core/common.h"

#include "wallet/wallet.h"
#include "wallet/wallet_db.h"
#include "wallet/wallet_network.h"
#include "wallet/secstring.h"
#include "core/ecc_native.h"
#include "core/serialization_adapters.h"
#include "core/treasury.h"
#include "unittests/util.h"
#include "mnemonic/mnemonic.h"
#include "utility/string_helpers.h"

#ifndef LOG_VERBOSE_ENABLED
    #define LOG_VERBOSE_ENABLED 0
#endif

#include "utility/logger.h"
#include "utility/options.h"
#include "utility/helpers.h"
#include <iomanip>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <iterator>
#include <future>
#include "version.h"

using namespace std;
using namespace beam;
using namespace ECC;

namespace beam
{
    std::ostream& operator<<(std::ostream& os, Coin::Status s)
    {
        stringstream ss;
        ss << "[";
        switch (s)
        {
        case Coin::Available: ss << "Available"; break;
        case Coin::Unavailable: ss << "Unavailable"; break;
        case Coin::Spent: ss << "Spent"; break;
        case Coin::Maturing: ss << "Maturing"; break;
        case Coin::Outgoing: ss << "In progress(outgoing)"; break;
        case Coin::Incoming: ss << "In progress(incoming)"; break;
        case Coin::Change: ss << "In progress(change)"; break;
        default:
            assert(false && "Unknown coin status");
        }
        ss << "]";
        string str = ss.str();
        os << str;
        assert(str.length() <= 24);
        size_t c = 24 - str.length();
        for (size_t i = 0; i < c; ++i) os << ' ';
        return os;
    }

    const char* getTxStatus(const TxDescription& tx)
    {
        static const char* Pending = "Pending";
        static const char* Sending = "Sending";
        static const char* Receiving = "Receiving";
        static const char* Cancelled = "Cancelled";
        static const char* Sent = "Sent";
        static const char* Received = "Received";
        static const char* Failed = "Failed";

        switch (tx.m_status)
        {
        case TxStatus::Pending: return Pending;
        case TxStatus::InProgress: return tx.m_sender ? Sending : Receiving;
        case TxStatus::Cancelled: return Cancelled;
        case TxStatus::Completed: return tx.m_sender ? Sent : Received;
        case TxStatus::Failed: return Failed;
        default:
            assert(false && "Unknown key type");
        }

        return "";
    }
}
namespace
{
    void printHelp(const po::options_description& options)
    {
        cout << options << std::endl;
    }

    void newAddress(
        const IWalletDB::Ptr& walletDB,
        const std::string& label,
        const SecString& pass)
    {
        WalletAddress address = wallet::createAddress(walletDB);

        address.m_label = label;
        walletDB->saveAddress(address);

        LOG_INFO() << "New address generated:\n\n" << std::to_string(address.m_walletID) << "\n";
        if (!label.empty()) {
            LOG_INFO() << "label = " << label;
        }
    }

    WordList GeneratePhrase()
    {
        auto phrase = createMnemonic(getEntropy(), language::en);
        assert(phrase.size() == 12);
        cout << "======\nGenerated wallet phrase: \n\n\t";
        for (const auto& word : phrase)
        {
            cout << word << ';';
        }
        cout << "\n\n\tIMPORTANT\n\n\tYour recovery phrase is the access key to all the cryptocurrencies in your wallet.\n\tPrint or write down the phrase to keep it in a safe or in a locked vault.\n\tWithout the phrase you will not be able to recover your money.\n======" << endl;
        return phrase;
    }

    bool ReadWalletSeed(NoLeak<uintBig>& walletSeed, const po::variables_map& vm, bool generateNew)
    {
        SecString seed;
        WordList phrase;
        if (generateNew)
        {
            LOG_INFO() << "Generating wallet phrase...";
            phrase = GeneratePhrase();
        }
        else if (vm.count(cli::WALLET_PHRASE))
        {
            auto tempPhrase = vm[cli::WALLET_PHRASE].as<string>();
            phrase = string_helpers::split(tempPhrase, ';');
            assert(phrase.size() == 12);
            if (phrase.size() != 12)
            {
                LOG_ERROR() << "Invalid recovery phrases provided: " << tempPhrase;
                return false;
            }
        }
        else
        {
            LOG_ERROR() << "Wallet phrase has not been provided.";
            return false;
        }

        auto buf = decodeMnemonic(phrase);
        seed.assign(buf.data(), buf.size());

        walletSeed.V = seed.hash().V;
        return true;
    }
}

void ResolveWID(PeerID& res, const std::string& s)
{
	bool bValid = true;
	ByteBuffer bb = from_hex(s, &bValid);

	if ((bb.size() != res.nBytes) || !bValid)
		throw std::runtime_error("invalid WID");

	memcpy(res.m_pData, &bb.front(), res.nBytes);
}

template <typename T>
bool FLoad(T& x, const std::string& sPath, bool bStrict = true)
{
	std::FStream f;
	if (!f.Open(sPath.c_str(), true, bStrict))
		return false;

	yas::binary_iarchive<std::FStream, SERIALIZE_OPTIONS> arc(f);
	arc & x;
	return true;
}

template <typename T>
void FSave(const T& x, const std::string& sPath)
{
	std::FStream f;
	f.Open(sPath.c_str(), false, true);

	yas::binary_oarchive<std::FStream, SERIALIZE_OPTIONS> arc(f);
	arc & x;
}

int HandleTreasury(const po::variables_map& vm, Key::IKdf& kdf)
{
	PeerID wid;
	Scalar::Native sk;
	Treasury::get_ID(kdf, wid, sk);

	char szID[PeerID::nTxtLen + 1];
	wid.Print(szID);

	static const char* szPlans = "treasury_plans.bin";
	static const char* szRequest = "-plan.bin";
	static const char* szResponse = "-response.bin";
	static const char* szData = "treasury_data.bin";

	Treasury tres;
	FLoad(tres, szPlans, false);


	auto nCode = vm[cli::TR_OPCODE].as<uint32_t>();
	switch (nCode)
	{
	default:
		cout << "ID: " << szID << std::endl;
		break;

	case 1:
		{
			// generate plan
			std::string sID = vm[cli::TR_WID].as<std::string>();
			ResolveWID(wid, sID);

			auto perc = vm[cli::TR_PERC].as<double>();
			perc *= 0.01;

			Amount val = static_cast<Amount>(Rules::get().EmissionValue0 * perc); // rounded down

			Treasury::Parameters pars; // default
			Treasury::Entry* pE = tres.CreatePlan(wid, val, pars);

			FSave(pE->m_Request, sID + szRequest);
			FSave(tres, szPlans);
		}
		break;

	case 2:
		{
			// generate response
			Treasury::Request treq;
			FLoad(treq, std::string(szID) + szRequest);

			Treasury::Response tresp;
			uint64_t nIndex = 1;
			tresp.Create(treq, kdf, nIndex);

			FSave(tresp, std::string(szID) + szResponse);
		}
		break;

	case 3:
		{
			// verify & import reponse
			std::string sID = vm[cli::TR_WID].as<std::string>();
			ResolveWID(wid, sID);

			Treasury::EntryMap::iterator it = tres.m_Entries.find(wid);
			if (tres.m_Entries.end() == it)
				throw std::runtime_error("plan not found");

			Treasury::Entry& e = it->second;
			e.m_pResponse.reset(new Treasury::Response);
			FLoad(*e.m_pResponse, sID + szResponse);

			if (!e.m_pResponse->IsValid(e.m_Request))
				throw std::runtime_error("invalid response");

			FSave(tres, szPlans);
		}
		break;

	case 4:
		{
			// Finally generate treasury
			Treasury::Data data;
			data.m_sCustomMsg = vm[cli::TR_COMMENT].as<std::string>();
			tres.Build(data);

			FSave(data, szData);

			Serializer ser;
			ser & data;

			ByteBuffer bb;
			ser.swap_buf(bb);

			Hash::Value hv;
			Hash::Processor() << Blob(bb) >> hv;

			char szHash[Hash::Value::nTxtLen + 1];
			hv.Print(szHash);

			cout << "Treasury data hash: " << szHash << std::endl;

		}
		break;

	case 5:
		{
			// recover and print
			Treasury::Data data;
			FLoad(data, szData);

			std::vector<Treasury::Data::Coin> vCoins;
			data.Recover(kdf, vCoins);

			cout << "Recovered coins: " << vCoins.size() << std::endl;

			for (size_t i = 0; i < vCoins.size(); i++)
			{
				const Treasury::Data::Coin& coin = vCoins[i];
				cout << "\t" << coin.m_Kidv << ", Height=" << coin.m_Incubation << std::endl;

			}
		}
		break;

	}

	return 0;
}


io::Reactor::Ptr reactor;

static const unsigned LOG_ROTATION_PERIOD = 3*60*60*1000; // 3 hours

int main_impl(int argc, char* argv[])
{
	beam::Crash::InstallHandler(NULL);

    try
    {
        auto [options, visibleOptions] = createOptionsDescription(GENERAL_OPTIONS | WALLET_OPTIONS);

        po::variables_map vm;
        try
        {
            vm = getOptions(argc, argv, "beam-wallet.cfg", options, true);
        }
        catch (const po::error& e)
        {
            cout << e.what() << std::endl;
            printHelp(visibleOptions);

            return 0;
        }

        if (vm.count(cli::HELP))
        {
            printHelp(visibleOptions);

            return 0;
        }

        if (vm.count(cli::VERSION))
        {
            cout << PROJECT_VERSION << endl;
            return 0;
        }

        if (vm.count(cli::GIT_COMMIT_HASH))
        {
            cout << GIT_COMMIT_HASH << endl;
            return 0;
        }

        int logLevel = getLogLevel(cli::LOG_LEVEL, vm, LOG_LEVEL_DEBUG);
        int fileLogLevel = getLogLevel(cli::FILE_LOG_LEVEL, vm, LOG_LEVEL_DEBUG);

        const auto path = boost::filesystem::system_complete("./logs");
        auto logger = beam::Logger::create(logLevel, logLevel, fileLogLevel, "wallet_", path.string());

        try
        {
            po::notify(vm);

            Rules::get().UpdateChecksum();

           {
                reactor = io::Reactor::create();
                io::Reactor::Scope scope(*reactor);

                io::Reactor::GracefulIntHandler gih(*reactor);

                io::Timer::Ptr logRotateTimer = io::Timer::create(*reactor);
                logRotateTimer->start(
                    LOG_ROTATION_PERIOD, true,
                    []() {
                        Logger::get()->rotate();
                    }
                );

                {
                    if (vm.count(cli::COMMAND))
                    {
                        auto command = vm[cli::COMMAND].as<string>();
                        if (command != cli::INIT
                            && command != cli::RESTORE
                            && command != cli::SEND
                            && command != cli::RECEIVE
                            && command != cli::LISTEN
                            && command != cli::TREASURY
                            && command != cli::INFO
							&& command != cli::EXPORT_MINER_KEY
                            && command != cli::EXPORT_OWNER_KEY
                            && command != cli::NEW_ADDRESS
                            && command != cli::CANCEL_TX
                            && command != cli::GENERATE_PHRASE)
                        {
                            LOG_ERROR() << "unknown command: \'" << command << "\'";
                            return -1;
                        }

                        if (command == cli::GENERATE_PHRASE)
                        {
                            GeneratePhrase();
                            return 0;
                        }

                        LOG_INFO() << "Rules signature: " << Rules::get().Checksum;

                        assert(vm.count(cli::WALLET_STORAGE) > 0);
                        auto walletPath = vm[cli::WALLET_STORAGE].as<string>();

                        if (!WalletDB::isInitialized(walletPath) && (command != cli::INIT && command != cli::RESTORE))
                        {
                            LOG_ERROR() << "Please initialize your wallet first... \nExample: beam-wallet --command=init";
                            return -1;
                        }
                        else if (WalletDB::isInitialized(walletPath) && (command == cli::INIT || command == cli::RESTORE))
                        {
                            LOG_ERROR() << "Your wallet is already initialized.";
                            return -1;
                        }

                        LOG_INFO() << "starting a wallet...";

                        SecString pass;
                        if (!beam::read_wallet_pass(pass, vm))
                        {
                            LOG_ERROR() << "Please, provide password for the wallet.";
                            return -1;
                        }

                        if (command == cli::INIT || command == cli::RESTORE)
                        {
                            NoLeak<uintBig> walletSeed;
                            walletSeed.V = Zero;
                            if (!ReadWalletSeed(walletSeed, vm, command == cli::INIT))
                            {
                                LOG_ERROR() << "Please, provide seed phrase for the wallet.";
                                return -1;
                            }
                            auto walletDB = WalletDB::init(walletPath, pass, walletSeed);
                            if (walletDB)
                            {
                                LOG_INFO() << "wallet successfully created...";

                                // generate default address
                                newAddress(walletDB, "default", pass);

                                return 0;
                            }
                            else
                            {
                                LOG_ERROR() << "something went wrong, wallet not created...";
                                return -1;
                            }
                        }

                        auto walletDB = WalletDB::open(walletPath, pass);
                        if (!walletDB)
                        {
                            LOG_ERROR() << "Wallet data unreadable, restore wallet.db from latest backup or delete it and reinitialize the wallet";
                            return -1;
                        }

						if (command == cli::EXPORT_MINER_KEY)
						{
							uint32_t subKey = vm[cli::KEY_SUBKEY].as<uint32_t>();
                            if (subKey < 1)
                            {
                                cout << "Please, specify Subkey number --subkey=N (N > 0)" << endl;
                                return -1;
                            }
							Key::IKdf::Ptr pKey = walletDB->get_ChildKdf(subKey);
							const ECC::HKdf& kdf = static_cast<ECC::HKdf&>(*pKey);

							KeyString ks;
							ks.SetPassword(Blob(pass.data(), static_cast<uint32_t>(pass.size())));
							ks.m_sMeta = std::to_string(subKey);

							ks.Export(kdf);
							cout << "Secret Subkey " << subKey <<  ": " << ks.m_sRes << std::endl;

							return 0;
						}

                        if (command == cli::EXPORT_OWNER_KEY)
                        {
                            Key::IKdf::Ptr pKey = walletDB->get_ChildKdf(0);
                            const ECC::HKdf& kdf = static_cast<ECC::HKdf&>(*pKey);

                            KeyString ks;
                            ks.SetPassword(Blob(pass.data(), static_cast<uint32_t>(pass.size())));
                            ks.m_sMeta = std::to_string(0);

                            ECC::HKdfPub pkdf;
                            pkdf.GenerateFrom(kdf);

                            ks.Export(pkdf);
                            cout << "Owner Viewer key: " << ks.m_sRes << std::endl;
                            
                            return 0;
                        }

                        if (command == cli::NEW_ADDRESS)
                        {
                            auto label = vm[cli::NEW_ADDRESS_LABEL].as<string>();
                            newAddress(walletDB, label, pass);

                            if (!vm.count(cli::LISTEN)) 
                            {
                                return 0;
                            }
                        }

                        LOG_INFO() << "wallet sucessfully opened...";

						if (command == cli::TREASURY)
							return HandleTreasury(vm, *walletDB->get_MasterKdf());

                        if (command == cli::INFO)
                        {
                            Block::SystemState::ID stateID = {};
                            walletDB->getSystemStateID(stateID);
                            auto totalInProgress = walletDB->getTotal(Coin::Incoming) +
                                walletDB->getTotal(Coin::Outgoing) + walletDB->getTotal(Coin::Change);
                            auto totalCoinbase = walletDB->getTotalByType(Coin::Available, Key::Type::Coinbase) +
                                walletDB->getTotalByType(Coin::Maturing, Key::Type::Coinbase);
                            auto totalFee = walletDB->getTotalByType(Coin::Available, Key::Type::Comission) +
                                walletDB->getTotalByType(Coin::Maturing, Key::Type::Comission);
                            auto totalUnspent = walletDB->getTotal(Coin::Available) + walletDB->getTotal(Coin::Maturing);

                            cout << "____Wallet summary____\n\n"
                                << "Current height............" << stateID.m_Height << '\n'
                                << "Current state ID.........." << stateID.m_Hash << "\n\n"
                                << "Available................." << PrintableAmount(walletDB->getAvailable()) << '\n'
                                << "Maturing.................." << PrintableAmount(walletDB->getTotal(Coin::Maturing)) << '\n'
                                << "In progress..............." << PrintableAmount(totalInProgress) << '\n'
                                << "Unavailable..............." << PrintableAmount(walletDB->getTotal(Coin::Unavailable)) << '\n'
                                << "Available coinbase ......." << PrintableAmount(walletDB->getAvailableByType(Key::Type::Coinbase)) << '\n'
                                << "Total coinbase............" << PrintableAmount(totalCoinbase) << '\n'
                                << "Avaliable fee............." << PrintableAmount(walletDB->getAvailableByType(Key::Type::Comission)) << '\n'
                                << "Total fee................." << PrintableAmount(totalFee) << '\n'
                                << "Total unspent............." << PrintableAmount(totalUnspent) << "\n\n";
                            if (vm.count(cli::TX_HISTORY))
                            {
                                auto txHistory = walletDB->getTxHistory();
                                if (txHistory.empty())
                                {
                                    cout << "No transactions\n";
                                    return 0;
                                }

                                cout << "TRANSACTIONS\n\n"
                                    << "| datetime            | amount, BEAM        | status    | ID\t|\n";
                                for (auto& tx : txHistory)
                                {
                                    cout << "  " << format_timestamp("%Y.%m.%d %H:%M:%S", tx.m_createTime * 1000, false)
                                        << setw(17) << PrintableAmount(tx.m_amount, true)
                                        << "  " << getTxStatus(tx) << "  " << tx.m_txId << '\n';
                                }
                                return 0;
                            }

                            cout << setw(20) << "id" << " |"
                                << setw(14) << "Beam" << " |"
                                << setw(14) << "Groth" << " |"
                                << setw(14) << "height" << " |"
                                << setw(18) << "maturity" << " |"
                                << setw(24) << "status" << " |"
                                << setw(8) << "type" << endl;
                            walletDB->visit([](const Coin& c)->bool
                            {
                                cout << setw(20) << c.m_ID.m_Idx
                                    << setw(16) << c.m_ID.m_Value / Rules::Coin
                                    << setw(16) << c.m_ID.m_Value % Rules::Coin
                                    << setw(16) << static_cast<int64_t>(c.m_createHeight)
                                    << setw(20) << (static_cast<int64_t>(c.m_maturity) < 0 ? "-" : std::to_string(static_cast<int64_t>(c.m_maturity)))
                                    << "   " << c.m_status
                                    << setw(8) << c.m_ID.m_Type << endl;
                                return true;
                            });
                            return 0;
                        }

                        if (vm.count(cli::NODE_ADDR) == 0)
                        {
                            LOG_ERROR() << "node address should be specified";
                            return -1;
                        }

                        string nodeURI = vm[cli::NODE_ADDR].as<string>();
                        io::Address node_addr;
                        if (!node_addr.resolve(nodeURI.c_str()))
                        {
                            LOG_ERROR() << "unable to resolve node address: " << nodeURI;
                            return -1;
                        }

                        io::Address receiverAddr;
                        Amount amount = 0;
                        Amount fee = 0;
                        WalletID receiverWalletID(Zero);
                        bool isTxInitiator = command == cli::SEND || command == cli::RECEIVE;
                        if (isTxInitiator)
                        {
                            if (vm.count(cli::RECEIVER_ADDR) == 0)
                            {
                                LOG_ERROR() << "receiver's address is missing";
                                return -1;
                            }
                            if (vm.count(cli::AMOUNT) == 0)
                            {
                                LOG_ERROR() << "amount is missing";
                                return -1;
                            }

							receiverWalletID.FromHex(vm[cli::RECEIVER_ADDR].as<string>());

                            auto signedAmount = vm[cli::AMOUNT].as<double>();
                            if (signedAmount < 0)
                            {
                                LOG_ERROR() << "Unable to send negative amount of coins";
                                return -1;
                            }

                            signedAmount *= Rules::Coin; // convert beams to coins

                            amount = static_cast<ECC::Amount>(signedAmount);
                            if (amount == 0)
                            {
                                LOG_ERROR() << "Unable to send zero coins";
                                return -1;
                            }

                            auto signedFee = vm[cli::FEE].as<double>();
                            if (signedFee < 0)
                            {
                                LOG_ERROR() << "Unable to take negative fee";
                                return -1;
                            }

                            signedFee *= Rules::Coin; // convert beams to coins

                            fee = static_cast<ECC::Amount>(signedFee);
                        }

                        bool is_server = (command == cli::LISTEN || vm.count(cli::LISTEN));

                        Wallet wallet{ walletDB, is_server ? Wallet::TxCompletedAction() : [](auto) { io::Reactor::get_Current().stop(); } };

						proto::FlyClient::NetworkStd nnet(wallet);
						nnet.m_Cfg.m_vNodes.push_back(node_addr);
						nnet.Connect();

						WalletNetworkViaBbs wnet(wallet, nnet, walletDB);
						
						wallet.set_Network(nnet, wnet);

                        if (isTxInitiator)
                        {
                            // TODO: make db request by 'default' label
                            auto addresses = walletDB->getAddresses(true);
                            if (addresses.empty()) {
                                newAddress(walletDB, "default", pass);
                                addresses = walletDB->getAddresses(true);
                                assert(!addresses.empty());
                            }
                            wallet.transfer_money(addresses[0].m_walletID, receiverWalletID, move(amount), move(fee), command == cli::SEND);
                        }

                        if (command == cli::CANCEL_TX) 
                        {
                            auto txIdVec = from_hex(vm[cli::TX_ID].as<string>());
                            TxID txId;
                            std::copy_n(txIdVec.begin(), 16, txId.begin());
                            wallet.cancel_tx(txId);
                        }

						io::Reactor::get_Current().run();

                    }
                    else
                    {
                        LOG_ERROR() << "command parameter not specified.";
                        printHelp(visibleOptions);
                    }
                }
            }
        }
        catch (const po::error& e)
        {
            LOG_ERROR() << e.what();
            printHelp(visibleOptions);
        }
        catch (const std::runtime_error& e)
        {
            LOG_ERROR() << e.what();
        }
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }

    return 0;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    return main_impl(argc, argv);
#else
    block_sigpipe();
    auto f = std::async(
        std::launch::async,
        [argc, argv]() -> int {
            // TODO: this hungs app on OSX
            //lock_signals_in_this_thread();
            int ret = main_impl(argc, argv);
            kill(0, SIGINT);
            return ret;
        }
    );

    wait_for_termination(0);

    if (reactor) reactor->stop();

    return f.get();
#endif
}

