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

namespace beam
{
    class WalletServer
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

    private:

        void on_stream_accepted(io::TcpStream::Ptr&& newStream, io::ErrorCode errorCode)
        {
            LOG_DEBUG() << "on_stream_accepted";
        }

    private:
        io::Reactor& _reactor;
        io::TcpServer::Ptr _server;
        io::Address _bindAddress;
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
