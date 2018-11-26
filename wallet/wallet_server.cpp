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

#include "wallet_server.h"

#include "utility/helpers.h"

#include "utility/io/tcpserver.h"

#define LOG_VERBOSE_ENABLED 1
#include "utility/logger.h"

#include <map>
#include "nlohmann/json.hpp"
#include "p2p/json_serializer.h"

using json = nlohmann::json;

namespace
{
    int parse_json(const void* buf, size_t bufSize, json& o) 
    {
        if (bufSize == 0) return -30000;

        const char* bufc = (const char*)buf;

        try 
        {
            o = json::parse(bufc, bufc + bufSize);
        }
        catch (const std::exception& e) 
        {
            LOG_ERROR() << "json parse: " << e.what() << "\n" << std::string(bufc, bufc + (bufSize > 1024 ? 1024 : bufSize));
            return -30000;
        }
        return 0; // OK
    }
}

namespace beam
{
    struct ConnectionToServer 
    {
        virtual ~ConnectionToServer() = default;

        virtual void on_bad_peer(uint64_t from) = 0;
    };

    class WalletServer : public ConnectionToServer
    {
    public:
        WalletServer(io::Reactor& reactor, io::Address listenTo)
            : _reactor(reactor)
            , _bindAddress(listenTo)
        {

        }

        void start()
        {
            LOG_INFO() << "Start server on " << _bindAddress;

            try
            {
                _server = io::TcpServer::create(
                    _reactor,
                    _bindAddress,
                    BIND_THIS_MEMFN(on_stream_accepted)
                );
            }
            catch (const std::exception& e)
            {
                LOG_ERROR() << "cannot start server: " << e.what();
            }
        }

        void stop()
        {

        }

    protected:

        void on_bad_peer(uint64_t from) override
        {
            _connections.erase(from);
        }

    private:

        void on_stream_accepted(io::TcpStream::Ptr&& newStream, io::ErrorCode errorCode)
        {
            if (errorCode == 0) 
            {
                auto peer = newStream->peer_address();
                LOG_DEBUG() << "+peer " << peer;

                _connections[peer.u64()] = std::make_unique<Connection>(*this, peer.u64(), std::move(newStream));
            }

            LOG_DEBUG() << "on_stream_accepted";
        }

    private:
        class Connection
        {
        public:
            Connection(ConnectionToServer& owner, uint64_t id, io::TcpStream::Ptr&& newStream)
                : _owner(owner)
                , _id(id)
                , _stream(std::move(newStream))
            {
                _stream->enable_keepalive(2);
                _stream->enable_read(BIND_THIS_MEMFN(on_stream_data));
            }

            bool on_stream_data(io::ErrorCode errorCode, void* data, size_t size)
            {
                if (errorCode != 0) 
                {
                    LOG_INFO() << "peer disconnected, code=" << io::error_str(errorCode);
                    _owner.on_bad_peer(_id);
                    return false;
                }

                {
                    json o;
                    auto result = parse_json(data, size, o);

                    if (result != 0)
                    {
                        return false;
                    }

                    LOG_INFO() << "new data from client: " << "jsonrpc = " << o["jsonrpc"];
                }

                {
                    io::SerializedMsg currentMsg;
                    io::FragmentWriter fw(4096, 0, [&](io::SharedBuffer&& buf) { currentMsg.push_back(buf); });

                    json msg{ {"jsonrpc", "2.0"}, {"method" , "bye"} };
                    //msg["jsonrpc"] = "2.0";
                    serialize_json_msg(fw, msg);

                    _stream->write(currentMsg);
                }

                return true;
            }
        private:
            ConnectionToServer& _owner;
            uint64_t _id;
            io::TcpStream::Ptr _stream;
        };

        io::Reactor& _reactor;
        io::TcpServer::Ptr _server;
        io::Address _bindAddress;
        std::map<uint64_t, std::unique_ptr<Connection>> _connections;
    };
}

using namespace beam;

int main()
{
    auto logger = Logger::create(LOG_LEVEL_VERBOSE, LOG_LEVEL_VERBOSE);

    try
    {
        io::Address listenTo = io::Address::localhost().port(10000);
        io::Reactor::Ptr reactor = io::Reactor::create();
        io::Reactor::Scope scope(*reactor);
        io::Reactor::GracefulIntHandler gih(*reactor);

        WalletServer server(*reactor, listenTo);
        server.start();

        reactor->run();

        server.stop();

        LOG_INFO() << "Done";
    }
    catch (const std::exception& e)
    {
        LOG_ERROR() << "EXCEPTION: " << e.what();
    }
    catch (...)
    {
        LOG_ERROR() << "NON_STD EXCEPTION";
    }


    return 0;
}
