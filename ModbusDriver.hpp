#pragma once
#include "ModbusDriverTypes.hpp"
#include "ModBusBuffer.hpp"
#include <cstddef>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <iterator>
#include <mutex>
#include <future>
#include <cmath>

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#ifdef _WIN32

// WINDOWS socket
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
using X_SOCKET = SOCKET;
using ssize_t = int;

#define X_ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define X_CLOSE_SOCKET(s) closesocket(s)
#define X_ISCONNECTSUCCEED(s) ((s) != SOCKET_ERROR)
#else
// Berkeley socket
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// #include <fcntl.h> /* Added for the nonblocking socket */
using X_SOCKET = int;

#define X_ISVALIDSOCKET(s) ((s) >= 0)
#define X_CLOSE_SOCKET(s) close(s)
#define X_ISCONNECTSUCCEED(s) ((s) >= 0)
#endif

using SOCKADDR = struct sockaddr;
using SOCKADDR_IN = struct sockaddr_in;

// int recvtimeout(int s, char* buf, int len, int timeout);
namespace GMBUSLib {
    class MBDriver
    {
    public:
        struct MessageContainer
        {
            uint8_t Id = 0;
            GMBUSLib::Types::Priority Priority = GMBUSLib::Types::Priority::High;
            GMBUSLib::Types::ReadFC ReadFC = GMBUSLib::Types::ReadFC::NONE;
            GMBUSLib::Types::WriteFC WriteFC = GMBUSLib::Types::WriteFC::NONE;
            uint16_t StartRegister = 0;
            uint8_t Size = 0;
            uint16_t LastModbusMessageId = 0;
            std::chrono::milliseconds Interval = std::chrono::milliseconds(0);
            std::chrono::milliseconds Timeout = std::chrono::milliseconds(50);
            std::chrono::time_point<std::chrono::steady_clock> LastEvent = {};
            GMBUSLib::Types::PollingState State = GMBUSLib::Types::PollingState::None;
            GMBUSLib::Buffer MBBuffer;
            // GMBUSLib::Types::WordBuffer WordBuffer;
            // GMBUSLib::Types::ByteBuffer ByteBuffer;
            void (*CallBackFunction)(GMBUSLib::Buffer) = NULL;
        };

        struct CommStatistics
        {
            bool Connected;
            uint64_t BytesReceived;
            uint64_t BytesSent;
            uint32_t SuccessReadCounter;
            uint32_t SucessWriteCounter;
            uint32_t FailReadCounter;
            uint32_t FailWriteCounter;
            uint32_t TimeoutCounter;
            uint32_t DropPacketCounter;
            uint8_t LastErrorCode;
            std::chrono::time_point<std::chrono::steady_clock> DisconnectedTime;
        };

    private:
        struct QueueMng
        {
            bool AutoPolling = false;
            int NextHighPriorityPosition = 0;
            int NextNormalPriorityPosition = 0;
            int NextLowPriorityPosition = 0;
            int NumberOfConcurrentPollingMessages = 2;
            uint16_t LastModbusMessageId = 0;
            std::vector<uint8_t> HighPriorityList;
            std::vector<uint8_t> NormalPriorityList;
            std::vector<uint8_t> LowPriorityList;
            std::chrono::milliseconds DefaultInterval = std::chrono::milliseconds(0);
            std::chrono::time_point<std::chrono::steady_clock> LastEvent;
            // MessageContainer TempMsg;
        };

        std::string HostName;
        uint16_t TcpPort = 502;
        uint8_t SlaveID = 1;

        X_SOCKET _socket{};
        SOCKADDR_IN _server{};

        bool _finish_all_threads = false;

        std::vector<MessageContainer> MsgContainer;
        std::vector<uint8_t> QueueHashTable;
        std::vector<uint8_t> PollingHashTable;

        GMBUSLib::Types::MapType MapType;
        QueueMng QueueMng;

        // MessageContainerMutex
        std::mutex GeneralSettingsMutex;
        std::mutex MsgContainerMutex;
        std::mutex QueueHashTableMutex;
        int PollingHashSendSize = 0;
        std::mutex PollingHashTableMutex;

        std::thread thrCommManager;
        std::thread thrSend;
        std::thread thrReceive;
        CommStatistics CommStat;

        int recvtimeout(int s, char *buf, int len, std::chrono::milliseconds Timeout);

        void BuildHeader(uint8_t *to_send, uint8_t Id);
        void Send();
        void Receive();
        uint8_t ModbusWriteCommand(GMBUSLib::Types::WriteFC WriteFC, const uint16_t StartRegister, const GMBUSLib::Buffer & MBBuffer, const std::chrono::milliseconds Timeout);
        uint8_t ModbusReadCommand(GMBUSLib::Types::ReadFC ReadFC, const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer & MBBuffer, const std::chrono::milliseconds Timeout);

        void Init(GMBUSLib::Types::MapType MapType, const std::string_view Host);

    public:
        bool DebugPrint = false;
        bool AutoReconnect = true;
        std::chrono::milliseconds ReconnectTimeout = std::chrono::milliseconds(5000);

        MBDriver() = delete;
        // ModbusDriver(const ModbusDriver& p1);
        MBDriver(GMBUSLib::Types::MapType MapType, std::string Host);
        MBDriver(GMBUSLib::Types::MapType MapType, std::string Host, uint16_t TcpPort);

        ~MBDriver();

        // Getting and Setters
        void SetSlaveID(uint8_t SlaveID);
        uint8_t GetSlaveID();
        GMBUSLib::Types::MapType GetMapType();
        CommStatistics GetCommStatistics();

        bool Connect();
        void Disconnect();

        void CommunicationManager();

        void StartAutoPolling();

        void StopAutoPolling();

        uint8_t AddPollingMessageToQueue(GMBUSLib::Types::ReadFC ReadFC, uint16_t StartRegister, uint8_t Size, std::chrono::milliseconds Timeout, void (*CallBackFunction)(GMBUSLib::Buffer));
        uint8_t AddPollingMessageToQueue(GMBUSLib::Types::Priority Priority, GMBUSLib::Types::ReadFC ReadFC, uint16_t StartRegister, uint8_t Size, std::chrono::milliseconds Timeout, void (*CallBackFunction)(GMBUSLib::Buffer));
        uint8_t AddPollingMessageToQueue(std::chrono::milliseconds Interval, GMBUSLib::Types::ReadFC ReadFC, uint16_t StartRegister, uint8_t Size, std::chrono::milliseconds Timeout, void (*CallBackFunction)(GMBUSLib::Buffer));
        uint8_t WriteSingleCoil(const uint16_t StartRegister, const bool value, const std::chrono::milliseconds Timeout);
        uint8_t WriteSingleRegister(const uint16_t StartRegister, const uint16_t value, const std::chrono::milliseconds Timeout);
        uint8_t WriteMultipleCoils(const uint16_t StartRegister, const GMBUSLib::Buffer &MBBuffer, const std::chrono::milliseconds Timeout);
        uint8_t WriteMultipleRegisters(const uint16_t StartRegister, const GMBUSLib::Buffer &MBBuffer, const std::chrono::milliseconds Timeout);

        uint8_t ReadCoils(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer &MBBuffer, std::chrono::milliseconds Timeout);
        uint8_t ReadDiscreteInputs(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer &MBBuffer, std::chrono::milliseconds Timeout);
        uint8_t ReadHoldingRegisters(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer&MBBuffer, std::chrono::milliseconds Timeout);
        uint8_t ReadInputRegisters(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer&MBBuffer, std::chrono::milliseconds Timeout);

        int GetNumberOfPollingMessages();

        GMBUSLib::Buffer GetBuffer(uint8_t MessageID);
    };
}