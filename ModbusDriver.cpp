#include "ModbusDriverTypes.hpp"
#include "ModbusDriver.hpp"
#include <cstring>

/*

  __  __         _ _              _____ ___ ___   ___           _              _
 |  \/  |___  __| | |__ _  _ ___ |_   _/ __| _ \ | _ \__ _ _  _| |___  __ _ __| |
 | |\/| / _ \/ _` | '_ \ || (_-<   | || (__|  _/ |  _/ _` | || | / _ \/ _` / _` |
 |_|  |_\___/\__,_|_.__/\_,_/__/   |_| \___|_|   |_| \__,_|\_, |_\___/\__,_\__,_|
                                                           |__/


                                                      ←---------------------[Modbus RTU]---------------------→
                                                      ╔════════╗╔════════╗╔════════════════╗╔════════════════╗
                                                      ║Slave ID║║   FC   ║║      DATA      ║║       CRC      ║
                                                      ╚════════╝╚════════╝╚════════════════╝╚════════════════╝
╔════════════════╗╔════════════════╗╔════════════════╗╔════════╗╔════════╗╔════════════════╗←----[2 Bytes]---→
║ Transaction ID ║║  Protocol ID   ║║     Length     ║║Unit ID ║║   FC   ║║      DATA      ║
╚════════════════╝╚════════════════╝╚════════════════╝╚════════╝╚════════╝╚════════════════╝
←----[2 Bytes]---→←----[2 Bytes]---→←----[2 Bytes]---→←[1 Byte]→←[1 Byte]→←----[X Bytes]---→
←------------------------[MBAP Header]-------------------------→←----------[PDU]-----------→
←---------------------------------------[Modbus TCP]---------------------------------------→

 MBAP - Modbus Application Header
 PDU  - Protocol Data Unit
*/

void GMBUSLib::MBDriver::Init(GMBUSLib::Types::MapType MapType, const std::string_view Host)
{
    this->MapType = MapType;
    this->HostName = Host;
    this->TcpPort = 502;

    this->CommStat.Connected = false;
    this->CommStat.BytesReceived = 0;
    this->CommStat.BytesSent = 0;
    this->CommStat.DropPacketCounter = 0;
    this->CommStat.FailReadCounter = 0;
    this->CommStat.FailWriteCounter = 0;
    this->CommStat.LastErrorCode = 0;
    this->CommStat.SuccessReadCounter = 0;
    this->CommStat.SucessWriteCounter = 0;
    this->CommStat.TimeoutCounter = 0;
    // this->QueueMng.LastModbusMessageId = 65000;

    MessageContainer tempmap;
    tempmap.Id = 0;
    tempmap.Priority = GMBUSLib::Types::Priority::High;
    tempmap.ReadFC = GMBUSLib::Types::ReadFC::NONE;
    tempmap.WriteFC = GMBUSLib::Types::WriteFC::NONE;
    tempmap.State = GMBUSLib::Types::PollingState::None;
    tempmap.StartRegister = 0;
    tempmap.Size = 0;
    tempmap.Interval = std::chrono::milliseconds(0);
    tempmap.Timeout = std::chrono::milliseconds(50);
    tempmap.LastEvent = {};
    tempmap.LastModbusMessageId = 0;
    tempmap.CallBackFunction = NULL;

    MsgContainer.push_back(tempmap);

    thrSend = std::thread(&GMBUSLib::MBDriver::Send, this);
    thrReceive = std::thread(&GMBUSLib::MBDriver::Receive, this);
    thrCommManager = std::thread(&GMBUSLib::MBDriver::CommunicationManager, this);
}

// GMBUSLib::MBDriver::ModbusDriver(const ModbusDriver& p1)
//{
//     std::cout << "Copy contructor called. ...";
// }

GMBUSLib::MBDriver::MBDriver(GMBUSLib::Types::MapType MapType, std::string Host)
{
    Init(MapType, Host);
}

GMBUSLib::MBDriver::MBDriver(GMBUSLib::Types::MapType MapType, std::string Host, uint16_t TcpPort)
{
    Init(MapType, Host);
    this->TcpPort = TcpPort;
};

GMBUSLib::MBDriver::~MBDriver()
{
    _finish_all_threads = true;
    thrReceive.join();
    thrSend.join();
    thrCommManager.join();
    if (CommStat.Connected)
        Disconnect();
    ;
};

void GMBUSLib::MBDriver::SetSlaveID(uint8_t SlaveID)
{
    std::lock_guard<std::mutex> lock(GeneralSettingsMutex);
    this->SlaveID = SlaveID;
};

uint8_t GMBUSLib::MBDriver::GetSlaveID()
{
    return SlaveID;
};

int GMBUSLib::MBDriver::GetNumberOfPollingMessages()
{
    return static_cast<int>(MsgContainer.size() - 1);
}

GMBUSLib::Buffer GMBUSLib::MBDriver::GetBuffer(uint8_t MessageID)
{
    return GMBUSLib::Buffer(MsgContainer[MessageID].MBBuffer);
}

GMBUSLib::MBDriver::CommStatistics GMBUSLib::MBDriver::GetCommStatistics()
{
    return CommStat;
};

GMBUSLib::Types::MapType GMBUSLib::MBDriver::GetMapType()
{
    std::lock_guard<std::mutex> lock(GeneralSettingsMutex);
    return MapType;
};

bool GMBUSLib::MBDriver::Connect()
{
    std::lock_guard<std::mutex> lock(GeneralSettingsMutex);
    if (HostName.empty() || TcpPort == 0 || CommStat.Connected)
    {
        return false;
    }

#ifdef _WIN32
    WSADATA wsadata;
    if (WSAStartup(0x0202, &wsadata))
    {
        return false;
    }
#endif

    _socket = socket(AF_INET, SOCK_STREAM, 0);
    // int flags = fcntl(_socket, F_GETFL, 0);
    // fcntl(_socket, F_SETFL, flags | O_NONBLOCK);
    if (!X_ISVALIDSOCKET(_socket))
    {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

#ifdef WIN32
    const DWORD timeout = 20;
#else
    struct timeval timeout
    {
    };
    timeout.tv_sec = 20; // after 20 seconds connect() will timeout
    timeout.tv_usec = 0;
#endif

    setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    _server.sin_family = AF_INET;
    _server.sin_addr.s_addr = inet_addr(HostName.c_str());
    _server.sin_port = htons(TcpPort);

    if (!X_ISCONNECTSUCCEED(connect(_socket, (SOCKADDR *)&_server, sizeof(_server))))
    {
#ifdef _WIN32
        WSACleanup();
#endif
        std::cout << "Failed to connect\n";
        CommStat.DisconnectedTime = std::chrono::steady_clock::now();
        CommStat.Connected = false;
        return false;
    }
    CommStat.Connected = true;
    std::cout << "Connect!\n";
    return true;
}

void GMBUSLib::MBDriver::Disconnect()
{
    X_CLOSE_SOCKET(_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    GeneralSettingsMutex.lock();
    CommStat.Connected = false;
    CommStat.DisconnectedTime = std::chrono::steady_clock::now();
    GeneralSettingsMutex.unlock();
}

void GMBUSLib::MBDriver::CommunicationManager()
{
    while (!_finish_all_threads)
    {
        if (!_finish_all_threads && CommStat.Connected)
        {
            if (QueueMng.AutoPolling)
            {
                // check for message on sleep state
                /*
                  _______
                 /  12   \
                |    |    |    ___                      _____ _              ___ _           _
                |9   |   3|   / _ \ _  _ ___ _  _ ___  |_   _(_)_ __  ___   / __| |_  ___ __| |__
                |     \   |  | (_) | || / -_) || / -_)   | | | | '  \/ -_) | (__| ' \/ -_) _| / /
                |         |   \__\_\\_,_\___|\_,_\___|   |_| |_|_|_|_\___|  \___|_||_\___\__|_\_\
                 \___6___/
                */
                std::lock_guard<std::mutex> lock(MsgContainerMutex);
                std::lock_guard<std::mutex> lock2(QueueHashTableMutex);
                if (this->MapType == GMBUSLib::Types::MapType::PollingInterval)
                {
                    for (auto it = MsgContainer.begin() + 1; it != MsgContainer.end(); it++)
                    {
                        if (it->State == GMBUSLib::Types::PollingState::Sleep || it->State == GMBUSLib::Types::PollingState::Error || it->State == GMBUSLib::Types::PollingState::Timeout)
                        {
                            std::chrono::time_point<std::chrono::steady_clock> Timeout;
                            Timeout = it->LastEvent + it->Interval;
                            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Timeout).count() > 0)
                            {
                                QueueHashTable.push_back(it->Id);
                                it->State = GMBUSLib::Types::PollingState::Queue;
                                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - (Timeout + it->Interval)).count() > 0)
                                {
                                    it->LastEvent = std::chrono::steady_clock::now();
                                }
                                else
                                {
                                    it->LastEvent = Timeout;
                                    // std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(it->LastEvent.time_since_epoch()).count() << std::endl;
                                }
                            }
                        }
                    }
                }
                else
                {
                    bool NextPriorityLevel = false;
                    if (QueueMng.HighPriorityList.size() > 0)
                    {
                        for (auto counter = QueueMng.NextHighPriorityPosition; counter < static_cast<int>(QueueMng.HighPriorityList.size()); counter++)
                        {
                            if (MsgContainer[QueueMng.HighPriorityList[counter]].State == GMBUSLib::Types::PollingState::Sleep || MsgContainer[QueueMng.HighPriorityList[counter]].State == GMBUSLib::Types::PollingState::Error || MsgContainer[QueueMng.HighPriorityList[counter]].State == GMBUSLib::Types::PollingState::Timeout)
                            {
                                QueueHashTable.push_back(MsgContainer[QueueMng.HighPriorityList[counter]].Id);
                                MsgContainer[QueueMng.HighPriorityList[counter]].State = GMBUSLib::Types::PollingState::Queue;
                                MsgContainer[QueueMng.HighPriorityList[counter]].LastEvent = std::chrono::steady_clock::now();
                                if (counter + 1 == static_cast<int>(QueueMng.HighPriorityList.size()))
                                {
                                    NextPriorityLevel = true;
                                    QueueMng.NextHighPriorityPosition = 0;
                                }
                                else
                                {
                                    QueueMng.NextHighPriorityPosition++;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        NextPriorityLevel = true;
                    }
                    if (QueueMng.NormalPriorityList.size() > 0 && NextPriorityLevel)
                    {
                        for (auto counter = QueueMng.NextNormalPriorityPosition; counter < static_cast<int>(QueueMng.NormalPriorityList.size()); counter++)
                        {
                            if (MsgContainer[QueueMng.NormalPriorityList[counter]].State == GMBUSLib::Types::PollingState::Sleep || MsgContainer[QueueMng.NormalPriorityList[counter]].State == GMBUSLib::Types::PollingState::Error || MsgContainer[QueueMng.NormalPriorityList[counter]].State == GMBUSLib::Types::PollingState::Timeout)
                            {
                                QueueHashTable.push_back(MsgContainer[QueueMng.NormalPriorityList[counter]].Id);
                                MsgContainer[QueueMng.NormalPriorityList[counter]].State = GMBUSLib::Types::PollingState::Queue;
                                MsgContainer[QueueMng.NormalPriorityList[counter]].LastEvent = std::chrono::steady_clock::now();
                                if (counter + 1 == static_cast<int>(QueueMng.NormalPriorityList.size()))
                                {
                                    NextPriorityLevel = true;
                                    QueueMng.NextNormalPriorityPosition = 0;
                                }
                                else
                                {
                                    NextPriorityLevel = false;
                                    QueueMng.NextNormalPriorityPosition++;
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }

                    if (QueueMng.LowPriorityList.size() > 0 && NextPriorityLevel)
                    {
                        for (auto counter = QueueMng.NextLowPriorityPosition; counter < static_cast<int>(QueueMng.LowPriorityList.size()); counter++)
                        {
                            if (MsgContainer[QueueMng.LowPriorityList[counter]].State == GMBUSLib::Types::PollingState::Sleep || MsgContainer[QueueMng.LowPriorityList[counter]].State == GMBUSLib::Types::PollingState::Error || MsgContainer[QueueMng.LowPriorityList[counter]].State == GMBUSLib::Types::PollingState::Timeout)
                            {
                                QueueHashTable.push_back(MsgContainer[QueueMng.LowPriorityList[counter]].Id);
                                MsgContainer[QueueMng.LowPriorityList[counter]].State = GMBUSLib::Types::PollingState::Queue;
                                MsgContainer[QueueMng.LowPriorityList[counter]].LastEvent = std::chrono::steady_clock::now();
                                if (counter + 1 == static_cast<int>(QueueMng.LowPriorityList.size()))
                                {
                                    QueueMng.NextLowPriorityPosition = 0;
                                }
                                else
                                {
                                    QueueMng.NextLowPriorityPosition++;
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
            }

            {
                std::chrono::time_point<std::chrono::steady_clock> Timeout;
                Timeout = QueueMng.LastEvent + QueueMng.DefaultInterval;

                std::lock_guard<std::mutex> lock(MsgContainerMutex);
                std::lock_guard<std::mutex> lock1(QueueHashTableMutex);
                std::lock_guard<std::mutex> lock2(PollingHashTableMutex);

                bool PriorityMapTriggerCondition = (this->MapType == GMBUSLib::Types::MapType::Priority && (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Timeout).count() > 0));

                while ((QueueHashTable.size() > 0 && PollingHashTable.size() < static_cast<long unsigned int>(QueueMng.NumberOfConcurrentPollingMessages)) &&
                       (this->MapType == GMBUSLib::Types::MapType::PollingInterval || PriorityMapTriggerCondition))
                {
                    MsgContainer[QueueHashTable[0]].State = GMBUSLib::Types::PollingState::ToSend;
                    PollingHashTable.push_back(QueueHashTable[0]);
                    QueueHashTable.erase(QueueHashTable.begin());
                }

                if (PollingHashTable.size() > 0 && !_finish_all_threads)
                {
                    PollingHashSendSize = static_cast<int>(PollingHashTable.size());
                }

                if (PriorityMapTriggerCondition)
                {
                    QueueMng.LastEvent = std::chrono::steady_clock::now();
                }
            }

            {
                /*
                    +====+  _____ _                    _
                    |(::)| |_   _(_)_ __  ___ ___ _  _| |_
                    | )( |   | | | | '  \/ -_) _ \ || |  _|
                    |(..)|   |_| |_|_|_|_\___\___/\_,_|\__|
                    +====+
                 */

                std::lock_guard<std::mutex> lock(MsgContainerMutex);
                std::lock_guard<std::mutex> lock1(QueueHashTableMutex);
                std::lock_guard<std::mutex> lock2(PollingHashTableMutex);

                for (auto it = MsgContainer.begin(); it != MsgContainer.end(); it++)
                {
                    std::chrono::time_point<std::chrono::steady_clock> Timeout;
                    Timeout = it->LastEvent + it->Timeout;
                    if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Timeout).count() > 0) && (it->State == GMBUSLib::Types::PollingState::Queue || it->State == GMBUSLib::Types::PollingState::ToSend || it->State == GMBUSLib::Types::PollingState::Sent))
                    {
                        // if (DebugPrint == true) std::cout << "Id " << static_cast<int>(it->Id) << "state =" << static_cast<int>(it->State) << " step 3 - timeout management" << std::endl << std::flush;
                        // std::cout << "[" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() << "]" << " step 3 - Timeout of msgId " << static_cast<int>(it->Id) << "state = " << static_cast<int>(it->State) << " ModbusMessageID" << static_cast<int>(it->LastModbusMessageId) << std::endl << std::flush;
                        //  Timeout

                        if (it->State == GMBUSLib::Types::PollingState::Queue)
                        {
                            std::vector<uint8_t>::iterator itQueueTable = std::find(QueueHashTable.begin(), QueueHashTable.end(), it->Id);
                            if (itQueueTable != QueueHashTable.end())
                                itQueueTable = QueueHashTable.erase(itQueueTable);
                            std::cout << "Id " << static_cast<int>(it->Id) << " Timeout - removing from queue table [" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() << "]"
                                      << "Message ID =" << int(it->LastModbusMessageId) << std::endl
                                      << std::flush;
                        }

                        if (it->State == GMBUSLib::Types::PollingState::ToSend || it->State == GMBUSLib::Types::PollingState::Sent)
                        {
                            if (DebugPrint == true)
                                std::cout << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                                          << std::flush;
                            std::vector<uint8_t>::iterator itHashTable = std::find(PollingHashTable.begin(), PollingHashTable.end(), it->Id);
                            if (itHashTable != PollingHashTable.end())
                                itHashTable = PollingHashTable.erase(itHashTable);
                            PollingHashSendSize--;
                            std::cout << "Id " << static_cast<int>(it->Id) << " Timeout - removing from hash table [" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() << "]"
                                      << "Message ID =" << int(it->LastModbusMessageId) << std::endl
                                      << std::flush;
                        }

                        it->State = GMBUSLib::Types::PollingState::Timeout;
                        it->LastEvent += it->Timeout;
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - it->LastEvent).count() > 0)
                        {
                            it->LastEvent = std::chrono::steady_clock::now();
                        }
                    }
                }
            }
        }
        else
        {
            /*
             ___/-\___
            |---------|
             |   |   |   ___            _           ___
             | | | | |  | __|_ __  _ __| |_ _  _   / _ \ _  _ ___ _  _ ___
             | | | | |  | _|| '  \| '_ \  _| || | | (_) | || / -_) || / -_)
             | | | | |  |___|_|_|_| .__/\__|\_, |  \__\_\\_,_\___|\_,_\___|
             |_______|            |_|       |__/
            */
            std::lock_guard<std::mutex> lock(MsgContainerMutex);
            std::lock_guard<std::mutex> lock1(QueueHashTableMutex);
            std::lock_guard<std::mutex> lock2(PollingHashTableMutex);

            MsgContainer[0].State = GMBUSLib::Types::PollingState::None;
            MsgContainer[0].LastModbusMessageId = 0;

            for (auto it = MsgContainer.begin() + 1; it != MsgContainer.end(); it++)
            {

                it->State = GMBUSLib::Types::PollingState::Sleep;
                it->LastModbusMessageId = 0;
            }

            PollingHashSendSize = 0;
            QueueHashTable.clear();
            PollingHashTable.clear();

            QueueMng.NextHighPriorityPosition = 0;
            QueueMng.NextNormalPriorityPosition = 0;
            QueueMng.NextLowPriorityPosition = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (AutoReconnect && !CommStat.Connected)
        {
            GeneralSettingsMutex.lock();
            std::chrono::time_point<std::chrono::steady_clock> Timeout = CommStat.DisconnectedTime + ReconnectTimeout;
            GeneralSettingsMutex.unlock();

            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Timeout).count() > 0)
            {
                Connect();
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    if (DebugPrint == true)
        std::cout << "Finished CommManage Thread" << std::endl
                  << std::flush;
}

void GMBUSLib::MBDriver::StartAutoPolling()
{
    if (!QueueMng.AutoPolling)
    {
        if (MapType == GMBUSLib::Types::MapType::Priority)
        {
            for (auto it = MsgContainer.begin() + 1; it != MsgContainer.end(); it++)
            {
                it->LastEvent = std::chrono::steady_clock::now();
            }
            QueueMng.LastEvent = std::chrono::steady_clock::now() - QueueMng.DefaultInterval;
        }
        QueueMng.AutoPolling = true;
        if (DebugPrint == true)
            std::cout << "StartCommManager" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                      << std::flush;
        /*thrCommManage = std::thread(&GMBUSLib::MBDriver::QueueManager, this);*/
        // thrCommManage.detach();
        return;
    }
}

void GMBUSLib::MBDriver::StopAutoPolling()
{
    if (QueueMng.AutoPolling)
    {
        QueueMng.AutoPolling = false;
        return;
    }
}

uint8_t GMBUSLib::MBDriver::AddPollingMessageToQueue(GMBUSLib::Types::ReadFC ReadFC, uint16_t StartRegister, uint8_t Size, std::chrono::milliseconds Timeout, void (*CallBackFunction)(GMBUSLib::Buffer))
{
    GMBUSLib::MBDriver::MessageContainer tempmap;
    std::lock_guard<std::mutex> lock(MsgContainerMutex);
    tempmap.Id = static_cast<uint8_t>(MsgContainer.size());
    // std::cout << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl << std::flush;
    tempmap.MBBuffer.SetType(ReadFC, Size);

    tempmap.ReadFC = ReadFC;
    tempmap.WriteFC = GMBUSLib::Types::WriteFC::NONE;
    tempmap.StartRegister = StartRegister;
    tempmap.Size = Size;
    tempmap.Interval = std::chrono::milliseconds(1000);
    tempmap.Priority = GMBUSLib::Types::Priority::Normal;
    tempmap.Timeout = Timeout;
    tempmap.LastEvent = {};
    tempmap.State = GMBUSLib::Types::PollingState::Sleep;
    tempmap.CallBackFunction = CallBackFunction;
    tempmap.LastModbusMessageId = 0;

    MsgContainer.push_back(tempmap);

    return static_cast<uint8_t>(MsgContainer.size() - 1);
}
// Method to add Polling messages based on priority
uint8_t GMBUSLib::MBDriver::AddPollingMessageToQueue(GMBUSLib::Types::Priority Priority, GMBUSLib::Types::ReadFC ReadFC, uint16_t StartRegister, uint8_t Size, std::chrono::milliseconds Timeout, void (*CallBackFunction)(GMBUSLib::Buffer))
{
    if (ReadFC == GMBUSLib::Types::ReadFC::NONE || Size <= 0)
        return -1;

    uint8_t TempID = AddPollingMessageToQueue(ReadFC, StartRegister, Size, Timeout, CallBackFunction);

    std::lock_guard<std::mutex> lock(MsgContainerMutex);

    MsgContainer[TempID].Priority = Priority;

    std::lock_guard<std::mutex> lock2(GeneralSettingsMutex);
    switch (Priority)
    {
    case GMBUSLib::Types::Priority::High:
        if (QueueMng.HighPriorityList.size() == 0)
        {
            QueueMng.NextHighPriorityPosition = 0;
        }
        QueueMng.HighPriorityList.push_back(TempID);
        break;
    case GMBUSLib::Types::Priority::Normal:
        if (QueueMng.NormalPriorityList.size() == 0)
        {
            QueueMng.NextNormalPriorityPosition = 0;
        }
        QueueMng.NormalPriorityList.push_back(TempID);
        break;
    case GMBUSLib::Types::Priority::Low:
        if (QueueMng.LowPriorityList.size() == 0)
        {
            QueueMng.NextLowPriorityPosition = 0;
        }
        QueueMng.LowPriorityList.push_back(TempID);
        break;
    }

    return TempID;
}

// Method to add Polling messages based on Polling Interval
uint8_t GMBUSLib::MBDriver::AddPollingMessageToQueue(std::chrono::milliseconds Interval, GMBUSLib::Types::ReadFC ReadFC, uint16_t StartRegister, uint8_t Size, std::chrono::milliseconds Timeout, void (*CallBackFunction)(GMBUSLib::Buffer))
{
    if (ReadFC == GMBUSLib::Types::ReadFC::NONE || Size <= 0)
        return -1;

    uint8_t TempID = AddPollingMessageToQueue(ReadFC, StartRegister, Size, Timeout, CallBackFunction);

    std::lock_guard<std::mutex> lock(MsgContainerMutex);

    MsgContainer[TempID].Interval = Interval;

    std::lock_guard<std::mutex> lock2(GeneralSettingsMutex);
    if (QueueMng.NormalPriorityList.size() == 0)
    {
        QueueMng.NextNormalPriorityPosition = 0;
    }

    QueueMng.NormalPriorityList.push_back(TempID);

    return TempID;
}

uint8_t GMBUSLib::MBDriver::ModbusWriteCommand(const GMBUSLib::Types::WriteFC WriteFC, const uint16_t StartRegister, const GMBUSLib::Buffer &Buffer, const std::chrono::milliseconds Timeout)
{
    MsgContainerMutex.lock();
    QueueHashTableMutex.lock();
    QueueHashTable.insert(QueueHashTable.begin(), 0);
    MsgContainer[0].State = GMBUSLib::Types::PollingState::Queue;
    MsgContainer[0].ReadFC = GMBUSLib::Types::ReadFC::NONE;
    MsgContainer[0].WriteFC = WriteFC;
    MsgContainer[0].StartRegister = StartRegister;
    MsgContainer[0].MBBuffer = Buffer;
    MsgContainer[0].LastEvent = std::chrono::steady_clock::now() + Timeout;
    QueueHashTableMutex.unlock();
    MsgContainerMutex.unlock();
    while (!_finish_all_threads)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        std::lock_guard<std::mutex> lock(MsgContainerMutex);
        if (MsgContainer[0].State == GMBUSLib::Types::PollingState::None ||
            MsgContainer[0].State == GMBUSLib::Types::PollingState::Error ||
            MsgContainer[0].State == GMBUSLib::Types::PollingState::Timeout ||
            MsgContainer[0].State == GMBUSLib::Types::PollingState::Sleep)
        {
            break;
        }
    }
    std::lock_guard<std::mutex> lock(MsgContainerMutex);
    std::lock_guard<std::mutex> lock2(GeneralSettingsMutex);

    switch (MsgContainer[0].State)
    {
    case GMBUSLib::Types::PollingState::Error:
        return CommStat.LastErrorCode;
    case GMBUSLib::Types::PollingState::Timeout:
        return uint8_t(255);
    default:
        return uint8_t(0);
    }
    return uint8_t(0);
}

uint8_t GMBUSLib::MBDriver::ModbusReadCommand(GMBUSLib::Types::ReadFC ReadFC, const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer &MBBuffer, const std::chrono::milliseconds Timeout)
{
    MsgContainerMutex.lock();
    QueueHashTableMutex.lock();
    QueueHashTable.insert(QueueHashTable.begin(), 0);
    MsgContainer[0].State = GMBUSLib::Types::PollingState::Queue;
    MsgContainer[0].ReadFC = ReadFC;
    MsgContainer[0].WriteFC = GMBUSLib::Types::WriteFC::NONE;
    MsgContainer[0].StartRegister = StartRegister;
    MsgContainer[0].Size = Size;
    MsgContainer[0].LastEvent = std::chrono::steady_clock::now() + Timeout;
    MsgContainer[0].MBBuffer.SetType(ReadFC, Size);
    QueueHashTableMutex.unlock();
    MsgContainerMutex.unlock();
    while (!_finish_all_threads)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        std::lock_guard<std::mutex> lock(MsgContainerMutex);
        if (MsgContainer[0].State == GMBUSLib::Types::PollingState::None ||
            MsgContainer[0].State == GMBUSLib::Types::PollingState::Error ||
            MsgContainer[0].State == GMBUSLib::Types::PollingState::Timeout ||
            MsgContainer[0].State == GMBUSLib::Types::PollingState::Sleep)
        {
            break;
        }
    }
    std::lock_guard<std::mutex> lock(MsgContainerMutex);
    std::lock_guard<std::mutex> lock2(GeneralSettingsMutex);
    MBBuffer = MsgContainer[0].MBBuffer;
    switch (MsgContainer[0].State)
    {
    case GMBUSLib::Types::PollingState::Error:
        MBBuffer.BufferAlign = 0;
        MBBuffer.rawBuffer.clear();
        return CommStat.LastErrorCode;
    case GMBUSLib::Types::PollingState::Timeout:
        MBBuffer.BufferAlign = 0;
        MBBuffer.rawBuffer.clear();
        return uint8_t(255);
    default:
        return uint8_t(0);
    }
    return uint8_t(0);
}

uint8_t GMBUSLib::MBDriver::WriteSingleCoil(const uint16_t StartRegister, const bool value, const std::chrono::milliseconds Timeout)
{
    GMBUSLib::Buffer MBBuffer;
    MBBuffer.SetType(GMBUSLib::Types::WriteFC::FC5_WRITE_SINGLE_COIL, 1);
    MBBuffer.rawBuffer[0] = (value) ? uint8_t(255) : uint8_t(0);
    MBBuffer.rawBuffer[1] = uint8_t(0);

    return ModbusWriteCommand(GMBUSLib::Types::WriteFC::FC5_WRITE_SINGLE_COIL, StartRegister, MBBuffer, Timeout);
}

uint8_t GMBUSLib::MBDriver::WriteSingleRegister(const uint16_t StartRegister, const uint16_t value, const std::chrono::milliseconds Timeout)
{
    GMBUSLib::Buffer MBBuffer;
    MBBuffer.SetType(GMBUSLib::Types::WriteFC::FC6_WRITE_SINGLE_REGISTER, 1);
    MBBuffer.rawBuffer[0] = uint8_t((value & 0xFF00u) >> 8);
    MBBuffer.rawBuffer[1] = uint8_t(value & 0xFFu);

    return ModbusWriteCommand(GMBUSLib::Types::WriteFC::FC6_WRITE_SINGLE_REGISTER, StartRegister, MBBuffer, Timeout);
}

uint8_t GMBUSLib::MBDriver::WriteMultipleCoils(const uint16_t StartRegister, const GMBUSLib::Buffer &MBBuffer, const std::chrono::milliseconds Timeout)
{
    if (GMBUSLib::Buffer(MBBuffer).GetSize(GMBUSLib::Types::SizeTypes::BITS) > 0)
    {
        return ModbusWriteCommand(GMBUSLib::Types::WriteFC::FC15_WRITE_MULTIPLE_COILS, StartRegister, MBBuffer, Timeout);
    }
    return uint8_t(0);
}

uint8_t GMBUSLib::MBDriver::WriteMultipleRegisters(const uint16_t StartRegister, const GMBUSLib::Buffer& MBBuffer, const std::chrono::milliseconds Timeout)
{
    if ((GMBUSLib::Buffer(MBBuffer).GetSize(GMBUSLib::Types::SizeTypes::WORDS) > 0) && (GMBUSLib::Buffer(MBBuffer).BufferAlign == 0))
    {
        return ModbusWriteCommand(GMBUSLib::Types::WriteFC::FC16_WRITE_MULTIPLE_REGISTERS, StartRegister, MBBuffer, Timeout);
    }
    return uint8_t(0);
}

uint8_t GMBUSLib::MBDriver::ReadCoils(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer &MBBuffer, std::chrono::milliseconds Timeout)
{
    if (Size > 0)
    {
        return ModbusReadCommand(GMBUSLib::Types::ReadFC::FC1_READ_COILS, StartRegister, Size, MBBuffer, Timeout);
    }
    return uint8_t(0);
}

uint8_t GMBUSLib::MBDriver::ReadDiscreteInputs(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer& MBBuffer, std::chrono::milliseconds Timeout)
{
    if (Size > 0)
    {
        return ModbusReadCommand(GMBUSLib::Types::ReadFC::FC2_READ_INPUT_BITS, StartRegister, Size, MBBuffer, Timeout);
    }
    return uint8_t(0);
}

uint8_t GMBUSLib::MBDriver::ReadHoldingRegisters(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer& MBBuffer, std::chrono::milliseconds Timeout)
{
    if (Size > 0)
    {
        return ModbusReadCommand(GMBUSLib::Types::ReadFC::FC3_READ_REGS, StartRegister, Size, MBBuffer, Timeout);
    }
    return uint8_t(0);
}

uint8_t GMBUSLib::MBDriver::ReadInputRegisters(const uint16_t StartRegister, const uint8_t Size, GMBUSLib::Buffer& MBBuffer, std::chrono::milliseconds Timeout)
{
    if (Size > 0)
    {
        return ModbusReadCommand(GMBUSLib::Types::ReadFC::FC4_READ_INPUT_REGS, StartRegister, Size, MBBuffer, Timeout);
    }
    return uint8_t(0);
}

void GMBUSLib::MBDriver::BuildHeader(uint8_t *to_send, uint8_t Id)
{
    /*
      __  __         _ _              __  __                            ___      _ _    _
     |  \/  |___  __| | |__ _  _ ___ |  \/  |___ ______ __ _ __ _ ___  | _ )_  _(_) |__| |___ _ _
     | |\/| / _ \/ _` | '_ \ || (_-< | |\/| / -_|_-<_-</ _` / _` / -_) | _ \ || | | / _` / -_) '_|
     |_|  |_\___/\__,_|_.__/\_,_/__/ |_|  |_\___/__/__/\__,_\__, \___| |___/\_,_|_|_\__,_\___|_|
                                                            |___/
    */
    GMBUSLib::MBDriver::MessageContainer Message;

    Message = MsgContainer[Id];
    if (QueueMng.LastModbusMessageId == 65535)
    {
        QueueMng.LastModbusMessageId = 1;
    }
    else
    {
        QueueMng.LastModbusMessageId++;
    }

    bool readMessage = (Message.WriteFC == GMBUSLib::Types::WriteFC::NONE);

    to_send[0] = static_cast<uint8_t>(QueueMng.LastModbusMessageId >> 8u); // Message ID
    to_send[1] = static_cast<uint8_t>(QueueMng.LastModbusMessageId & 0x00FFu);
    ;               // Message ID
    to_send[2] = 0; // Protocol ID
    to_send[3] = 0; // Protocol ID

    std::size_t MessageSize = 6; // Length for all read FC(1,2,3,4)

    if (!readMessage)
    {
        if (Message.WriteFC == GMBUSLib::Types::WriteFC::FC15_WRITE_MULTIPLE_COILS || Message.WriteFC == GMBUSLib::Types::WriteFC::FC16_WRITE_MULTIPLE_REGISTERS)
        {
            MessageSize = 7 + Message.MBBuffer.rawBuffer.size();
        }
        else
        {
            MessageSize = 4 + Message.MBBuffer.GetSize();
        }
    }
    to_send[4] = (MessageSize & 0xFF00u) >> 8u; // Length
    to_send[5] = MessageSize & 0xFFu;           // Length - Holding Register
    to_send[6] = this->SlaveID;
    to_send[7] = readMessage ? static_cast<uint8_t>(Message.ReadFC) : static_cast<uint8_t>(Message.WriteFC);
    to_send[8] = static_cast<uint8_t>(MsgContainer[Id].StartRegister >> 8u);
    to_send[9] = static_cast<uint8_t>(MsgContainer[Id].StartRegister & 0x00FFu);

    if (readMessage)
    {
        to_send[10] = 0;
        to_send[11] = MsgContainer[Id].Size;
        return;
    }

    uint16_t size{};
    if (Message.WriteFC == GMBUSLib::Types::WriteFC::FC15_WRITE_MULTIPLE_COILS)
    {
        size = MsgContainer[Id].MBBuffer.GetSize(GMBUSLib::Types::SizeTypes::BITS);
    }
    else if (Message.WriteFC == GMBUSLib::Types::WriteFC::FC16_WRITE_MULTIPLE_REGISTERS)
    {
        size = MsgContainer[Id].MBBuffer.GetSize(GMBUSLib::Types::SizeTypes::WORDS);
    }

    to_send[10] = uint8_t((size & 0xFF00u) >> 8);
    to_send[11] = (size & 0xFFu);
    to_send[12] = uint8_t(MsgContainer[Id].MBBuffer.GetBufferSize());
}

void GMBUSLib::MBDriver::Send()
{
    uint8_t SendBuffer[260] = {};
    auto it = PollingHashTable.end();
    std::cout << "GMBUSLib::MBDriver::Send()\n";
    while (!_finish_all_threads)
    {
        while (!_finish_all_threads)
        {
            if (DebugPrint == true)
                std::cout << "PollingHashTableMutex.lock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                          << std::flush;
            MsgContainerMutex.lock();
            PollingHashTableMutex.lock();
            if (PollingHashSendSize > 0)
            {
                // neither it variable declaration of for loop needed, reevaluate.
                it = PollingHashTable.end();
                for (it = PollingHashTable.begin(); it != PollingHashTable.end(); it++)
                {
                    if (MsgContainer[*it].State == GMBUSLib::Types::PollingState::ToSend)
                        break;
                }

                // check if any new message need to be sended
                if (DebugPrint == true)
                    std::cout << "PollingHashTableMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                              << std::flush;
                if (it != PollingHashTable.end())
                {
                    PollingHashTableMutex.unlock();
                    MsgContainerMutex.unlock();
                    break;
                }
            }
            if (DebugPrint == true)
                std::cout << "PollingHashTableMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                          << std::flush;
            PollingHashTableMutex.unlock();
            MsgContainerMutex.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        // std::cout << "Send() - After Wait\n" << std::flush;
        if (DebugPrint == true)
            std::cout << "MsgContainerMutex.lock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                      << std::flush;
        MsgContainerMutex.lock();
        // if (DebugPrint == true) std::cout << "QueueHashTableMutex.lock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl << std::flush;
        // QueueHashTableMutex.lock();
        if (DebugPrint == true)
            std::cout << "PollingHashTableMutex.lock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                      << std::flush;
        PollingHashTableMutex.lock();
        if (DebugPrint == true)
            std::cout << "GeneralSettingsMutex.lock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                      << std::flush;
        GeneralSettingsMutex.lock();

        if (CommStat.Connected && !_finish_all_threads)
        {
            if (DebugPrint == true)
                std::cout << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                          << std::flush;
            // auto it = PollingHashTable.begin();
            BuildHeader(SendBuffer, *it);

            if (MsgContainer[*it].WriteFC == GMBUSLib::Types::WriteFC::FC5_WRITE_SINGLE_COIL ||
                MsgContainer[*it].WriteFC == GMBUSLib::Types::WriteFC::FC6_WRITE_SINGLE_REGISTER)
            {
                for (uint8_t counter = 0; counter < MsgContainer[*it].MBBuffer.rawBuffer.size(); counter++)
                {
                    SendBuffer[10 + counter] = MsgContainer[*it].MBBuffer.rawBuffer[counter];
                }
            }
            else if (MsgContainer[*it].WriteFC == GMBUSLib::Types::WriteFC::FC15_WRITE_MULTIPLE_COILS ||
                     MsgContainer[*it].WriteFC == GMBUSLib::Types::WriteFC::FC16_WRITE_MULTIPLE_REGISTERS)
            {
                for (uint8_t counter = 0; counter < MsgContainer[*it].MBBuffer.rawBuffer.size(); counter++)
                {
                    SendBuffer[13 + counter] = MsgContainer[*it].MBBuffer.rawBuffer[counter];
                }
            }

            CommStat.BytesSent += uint64_t(6 + SendBuffer[5]);
            send(_socket, (const char *)SendBuffer, static_cast<unsigned char>(6 + SendBuffer[5]), 0);
            // std::cout << "Send() - ID sent <<" << static_cast<int>(*it) << " LastModbusMessageId" << QueueMng.LastModbusMessageId << std::endl << std::flush;
            MsgContainer[*it].State = GMBUSLib::Types::PollingState::Sent;
            MsgContainer[*it].LastModbusMessageId = QueueMng.LastModbusMessageId;
            /*it = PollingHashTable.erase(it);
            PollingHashSendSize--;*/
        }
        GeneralSettingsMutex.unlock();
        if (DebugPrint == true)
            std::cout << "GeneralSettingsMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                      << std::flush;
        PollingHashTableMutex.unlock();
        if (DebugPrint == true)
            std::cout << "PollingHashTableMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                      << std::flush;
        // QueueHashTableMutex.unlock();
        // if (DebugPrint == true) std::cout << "QueueHashTableMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl << std::flush;
        MsgContainerMutex.unlock();
        if (DebugPrint == true)
            std::cout << "MsgContainerMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                      << std::flush;
    };
    if (DebugPrint == true)
        std::cout << "Finished Send Thread\n"
                  << std::flush;
}

void GMBUSLib::MBDriver::Receive()
{
    uint8_t ReceiveBuffer[260] = {};
    std::cout << "GMBUSLib::MBDriver::Receive()\n";

    while (!_finish_all_threads)
    {
        // int BytesReceived = -2;

        

        if (!_finish_all_threads && CommStat.Connected)
        {
            int BytesReceived = recvtimeout(_socket, (char*)ReceiveBuffer, 260, std::chrono::milliseconds(1)); //,MSG_DONTWAIT);

            if (BytesReceived == 0 || BytesReceived == -1)
            {
                std::lock_guard<std::mutex> lock2(GeneralSettingsMutex);
                CommStat.Connected = false;
                CommStat.DisconnectedTime = std::chrono::steady_clock::now();
            }
            else if (BytesReceived == -2)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            else if (!_finish_all_threads)
            {
                std::lock_guard<std::mutex> lock(MsgContainerMutex);
                std::lock_guard<std::mutex> lock2(PollingHashTableMutex);
                std::lock_guard<std::mutex> lock3(GeneralSettingsMutex);
                CommStat.BytesReceived += BytesReceived;                
                uint16_t TempId = static_cast<uint16_t>((ReceiveBuffer[0] << 8u) + ReceiveBuffer[1]);
                bool DropMessage = true;
                int TempMessageID = -1;
                for (auto it = MsgContainer.begin(); it != MsgContainer.end(); it++)
                {
                    if (it->LastModbusMessageId == TempId)
                    {
                        TempMessageID = static_cast<int>(it->Id);
                        DropMessage = false;
                        break;
                    }
                }

                if (DropMessage)
                {
                    CommStat.DropPacketCounter++;
                    // std::cout << "Drop Message ID = " << static_cast<int>(TempId) << " " << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl;
                    // std::cout << "Bytes received = " << static_cast<int>(BytesReceived) << std::endl << std::flush;
                }
                else
                {

                    std::vector<uint8_t>::iterator it = std::find(PollingHashTable.begin(), PollingHashTable.end(), TempMessageID);
                    if (it != PollingHashTable.end())
                    {
                        it = PollingHashTable.erase(it);
                        PollingHashSendSize--;
                    }

                    // Check for Errors
                    if (ReceiveBuffer[5] == 3u) // If Payload size is equal 3, it´s an error message.
                    {
                        /*
                          __  __         _ _              ___                   ___           _              _
                         |  \/  |___  __| | |__ _  _ ___ | __|_ _ _ _ ___ _ _  | _ \__ _ _  _| |___  __ _ __| |
                         | |\/| / _ \/ _` | '_ \ || (_-< | _|| '_| '_/ _ \ '_| |  _/ _` | || | / _ \/ _` / _` |
                         |_|  |_\___/\__,_|_.__/\_,_/__/ |___|_| |_| \___/_|   |_| \__,_|\_, |_\___/\__,_\__,_|
                                                                                         |__/
                        ╔════════════════╗╔════════════════╗╔════════════════╗╔════════╗╔════════╗╔════════╗
                        ║ Transaction ID ║║  Protocol ID   ║║     Length     ║║Unit ID ║║   FC   ║║   EC   ║
                        ╚════════════════╝╚════════════════╝╚════════════════╝╚════════╝╚════════╝╚════════╝
                        ←----[2 Bytes]---→←----[2 Bytes]---→←----[2 Bytes]---→←[1 Byte]→←[1 Byte]→←[1 Byte]→
                        ←------------------------[MBAP Header]-------------------------→←------[PDU]-------→
                        ←---------------------------------------[Modbus TCP]---------------------------------------→

                         MBAP - Modbus Application Header
                         PDU  - Protocol Data Unit
                        */
                        MsgContainer[TempMessageID].State = GMBUSLib::Types::PollingState::Error;
                        CommStat.LastErrorCode = ReceiveBuffer[8];
                    }
                    else
                    {
                        MsgContainer[TempMessageID].State = (TempMessageID == 0) ? GMBUSLib::Types::PollingState::None : GMBUSLib::Types::PollingState::Sleep;
                        /*
                        * Position start from 0
                        * FC01 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                ByteCount           - [8]
                                Data                - [9]
                        * FC02 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                ByteCount           - [8]
                                Data                - [9]
                        * FC03 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                ByteCount           - [8]
                                Data                - [9]
                        * FC04 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                ByteCount           - [8]
                                Data                - [9]
                        * FC05 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                Ref. Number Address - [8-9]
                                Data                - [10]
                        * FC06 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                Ref. Number Address - [8-9]
                                Data                - [10]
                        * FC15 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                Ref. Number Address - [8-9]
                                BitCount            - [10]
                                *No Data
                        * FC16 -
                                Modbus/TCP Header   - [0-6]
                                FC                  - [7]
                                Ref. Number Address - [8-9]
                                WordCount            - [10]
                                *No Data

                        */

                        // memcpy(MsgContainer[TempMessageID].Buffer.BufferRead.data(), &ReceiveBuffer[9], BytesReceived-10);

                        CommStat.LastErrorCode = 0;
                        bool DataChange = false;
                        if ((ReceiveBuffer[7] > 0) && (ReceiveBuffer[7] < 5)) // Check if read message FC01, FC02, FC03, FC04
                        {
                            // MsgContainer[TempMessageID].MBBuffer.rawBuffer.clear();
                            CommStat.LastErrorCode = 0;

                            for (int counter = 9; counter < BytesReceived; counter++)
                            {
                                // MsgContainer[TempMessageID].MBBuffer.rawBuffer.push_back(ReceiveBuffer[counter]);
                                if (MsgContainer[TempMessageID].MBBuffer.rawBuffer[counter - 9] != (ReceiveBuffer[counter]))
                                {
                                    DataChange = true;
                                    MsgContainer[TempMessageID].MBBuffer.rawBuffer[counter - 9] = (ReceiveBuffer[counter]);
                                }
                            }
                        }

                        if (MsgContainer[TempMessageID].CallBackFunction != NULL && DataChange)
                        {
                            if (DebugPrint == true)
                                std::cout << "Adding Function Call on async thread\n"
                                          << std::flush;
                            std::async(std::launch::async, MsgContainer[TempMessageID].CallBackFunction, MsgContainer[TempMessageID].MBBuffer);
                        }
                    }
                }
                if (DebugPrint == true)
                    std::cout << "MsgContainerMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                              << std::flush;
                if (DebugPrint == true)
                    std::cout << "GeneralSettingsMutex.unlock()" << __PRETTY_FUNCTION__ << " " << __LINE__ << std::endl
                              << std::flush;
            }
        }
    }

    if (DebugPrint == true)
        std::cout << "Finished Receive Thread\n"
                  << std::flush;
}

int GMBUSLib::MBDriver::recvtimeout(int s, char *buf, int len, std::chrono::milliseconds Timeout)
{
    fd_set fds{};
    int n;
    struct timeval tv {};

    // set up the file descriptor set
    FD_ZERO(&fds);
    FD_SET(s, &fds);

    // set up the struct timeval for the timeout
    tv.tv_sec = 0;
    tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(Timeout).count();

    // wait until timeout or data received
    n = select(s + 1, &fds, NULL, NULL, &tv);
    if (n == 0)
        return -2; // timeout!
    if (n == -1)
        return -1; // error

    // data must be here, so do a normal recv()
    return static_cast<int>(recv(s, buf, len, 0));
}
