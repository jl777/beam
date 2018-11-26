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

#include "wallet_client.h"

#include "utility/helpers.h"

#include "utility/io/tcpserver.h"

#define LOG_VERBOSE_ENABLED 1
#include "utility/logger.h"

#include "utility/io/timer.h"
#include "nlohmann/json.hpp"
#include "p2p/json_serializer.h"
#include "p2p/line_protocol.h"

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
    static const unsigned RECONNECT_TIMEOUT = 1000;

    class WalletClient
    {
    public:
        WalletClient(io::Reactor& reactor, io::Address serverAddress)
            : _reactor(reactor)
            , _serverAddress(serverAddress)
            , _timer(io::Timer::create(_reactor))
            , _lineProtocol(BIND_THIS_MEMFN(on_raw_message), BIND_THIS_MEMFN(on_write))
        {
            _timer->start(0, false, BIND_THIS_MEMFN(on_reconnect));
        }

        bool on_raw_message(void* data, size_t size)
        {
            json o;
            auto result = parse_json(data, size, o);

            if (result == 0)
            {
                LOG_INFO() << "new data from server: " << "method = " << o["method"];

                if (o["method"] == "hello")
                {
                    {
                        json msg
                        {
                            {"jsonrpc", "2.0"},
                            {"method", "poll"}
                        };

                        serialize_json_msg(_lineProtocol, msg);
                    }

                    _lineProtocol.finalize();

                    LOG_INFO() << "call POLL method";
                }
                else if (o["method"] == "bye")
                {
                    LOG_INFO() << "closing connection and exit";
                    _reactor.stop();
                }
                else
                {
                    LOG_ERROR() << "Unknown method, closing connection...";
                    // close connection here
                    return false;
                }
            }
            else
            {
                LOG_ERROR() << "stream corrupted.";
                return false;
            }

            return true;
        }

        void on_write(io::SharedBuffer&& msg)
        {
            auto result = _stream->write(msg);

            if (!result)
            {
                on_disconnected(result.error());
            }
        }

        void on_reconnect() 
        {
            LOG_INFO() << "connecting to " << _serverAddress;
            if (!_reactor.tcp_connect(_serverAddress, 1, BIND_THIS_MEMFN(on_connected), 10000)) 
            {
                LOG_ERROR() << "connect attempt failed, rescheduling";
                _timer->start(RECONNECT_TIMEOUT, false, BIND_THIS_MEMFN(on_reconnect));
            }
        }

        void on_disconnected(io::ErrorCode error) 
        {
            LOG_INFO() << "disconnected, error=" << io::error_str(error) << ", rescheduling";
            _stream.reset();
            _timer->start(RECONNECT_TIMEOUT, false, BIND_THIS_MEMFN(on_reconnect));
        }

        void on_connected(uint64_t, io::TcpStream::Ptr&& newStream, io::ErrorCode errorCode)
        {
            if (errorCode != 0) 
            {
                on_disconnected(errorCode);
                return;
            }

            LOG_INFO() << "connected to " << _serverAddress;
            _stream = std::move(newStream);
            _stream->enable_keepalive(2);
            _stream->enable_read(BIND_THIS_MEMFN(on_stream_data));

            {
                json msg
                { 
                    {"jsonrpc", "2.0"}, 
                    {"method", "hello"}
                };

                serialize_json_msg(_lineProtocol, msg);
            }

            _lineProtocol.finalize();
        }

        bool on_stream_data(io::ErrorCode errorCode, void* data, size_t size) 
        {
            if (errorCode != 0) 
            {
                on_disconnected(errorCode);
                return false;
            }

            _lineProtocol.new_data_from_stream(data, size);

            return true;
        }

    private:
        io::Reactor& _reactor;
        io::Address _serverAddress;
        io::Timer::Ptr _timer;
        io::TcpStream::Ptr _stream;
        LineProtocol _lineProtocol;
    };
}

using namespace beam;

#ifdef WIN32
struct WSAInit {
    WSAInit() {
        WSADATA wsaData = { };
        int errorno = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (errorno != 0) {
            throw std::runtime_error("Failed to init WSA");
        }
    }
    ~WSAInit() {
        WSACleanup();
    }
};
#endif

int main()
{
#ifdef WIN32
    WSAInit init;
#endif // !WIN32

    auto logger = Logger::create(LOG_LEVEL_INFO, LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG, "wallet_client_");

    try
    {
        std::string serverAddr = "127.0.0.1:10000";
        io::Address connectTo;

        if (!connectTo.resolve(serverAddr.c_str())) 
        {
            throw std::runtime_error(std::string("cannot resolve server address ") + serverAddr);
        }

        io::Reactor::Ptr reactor = io::Reactor::create();
        io::Reactor::Scope scope(*reactor);
        io::Reactor::GracefulIntHandler gih(*reactor);

        WalletClient client(*reactor, connectTo);

        reactor->run();

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