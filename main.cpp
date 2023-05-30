#define EN_MB1 1

// #define EN_MB2 1
#include <cstdio>
#include <iostream>
// #include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <future>
#include "ModbusDriver.hpp"
// #include "ModBusBuffer.hpp"

class IntervalTimer
{
    std::chrono::system_clock::time_point start, end;

public:
    IntervalTimer()
    {
        start = std::chrono::system_clock::now();
    }
    ~IntervalTimer()
    {
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        std::time_t end_time = std::chrono::system_clock::to_time_t(end);
        std::cout << "finished command at " << std::ctime(&end_time)
                  << "elapsed time: " << elapsed_seconds.count() << "s\n"
                  << std::flush;
    }
};
void UnpackDataMessage01(GMBUSLib::Buffer MBBuffer)
{
    std::cout << "UnpackDataMessage01 - Message ID 1a " << static_cast<long int>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_NO_BYTE_SWAP)) << std::endl;
    std::cout << "UnpackDataMessage01 - Message ID 1b " << static_cast<long int>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_WITH_BYTE_SWAP)) << std::endl;
    std::cout << "UnpackDataMessage01 - Message ID 1c " << static_cast<long int>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP)) << std::endl;
    std::cout << "UnpackDataMessage01 - Message ID 1d " << static_cast<long int>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_WITH_BYTE_SWAP)) << std::endl
              << std::flush;
}

void UnpackDataMessage02(GMBUSLib::Buffer MBBuffer)
{
    std::cout << "UnpackDataMessage02 - Message ID 2 " << static_cast<int>(MBBuffer.GetWord(GMBUSLib::Types::ByteOrder::BYTE_SWAP)) << std::endl
              << std::flush;
}

void PrintCommState(GMBUSLib::MBDriver::CommStatistics CommStat)
{
    std::cout << "Device " << (CommStat.Connected ? " Connected" : " Not Connected") << std::endl;
    std::cout << "Bytes sent [" << CommStat.BytesSent << "]" << std::endl;
    std::cout << "Bytes received [" << CommStat.BytesReceived << "]" << std::endl;
}

int main()
{
    GMBUSLib::Buffer TestBuffer;
    TestBuffer.SetType(GMBUSLib::Types::WriteFC::FC15_WRITE_MULTIPLE_COILS, 12);
    TestBuffer.SetBit(true);
    TestBuffer.SetBit(false);
    TestBuffer.SkipBits(3);
    TestBuffer.SetBit(true);
    TestBuffer.SetBit(false);
    TestBuffer.SetBit(true);
    TestBuffer.SkipBits(2);
    TestBuffer.SetBit(true);

#ifdef EN_MB1
    GMBUSLib::MBDriver MB1(GMBUSLib::Types::MapType::PollingInterval, "127.0.0.1", 502);
    //MB1.DebugPrint = true;
    GMBUSLib::Buffer Msg01;
    MB1.AddPollingMessageToQueue(std::chrono::milliseconds(5000), GMBUSLib::Types::ReadFC::FC1_READ_COILS, 0, 16, std::chrono::seconds(10), [](GMBUSLib::Buffer MBBuffer)
                                 {
            
            std::cout << "UnpackDataMessage01" << std::endl;
            for (auto counter = 0; counter < 4; counter++)
            {
                std::cout << "Bit[" << counter << "] " << MBBuffer.GetBit() << std::endl;
            }

            std::cout << "Byte " << static_cast<int>(MBBuffer.GetByte()) << std::endl;
            std::cout << "Bits[12] Skip" << std::endl; MBBuffer.SkipBits(1);
            std::cout << "Bits[13-15] " << MBBuffer.GetBits(3) << std::endl << std::flush; });

    MB1.AddPollingMessageToQueue(std::chrono::milliseconds(5000), GMBUSLib::Types::ReadFC::FC3_READ_REGS, 2, 50, std::chrono::seconds(10), [](GMBUSLib::Buffer MBBuffer)
                                 {
            std::cout << "UnpackDataMessage02" << std::endl;
            
            MBBuffer.SkipBits(8);
            std::cout << "Byte " << static_cast<uint16_t>(MBBuffer.GetByte()) << std::endl;
            
            std::cout << "Double Word - Little Endian " << static_cast<int>(MBBuffer.GetDoubleWord(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_NO_BYTE_SWAP)) << std::endl;
            std::cout << "Double Word - Little Endian Bytes Swap " << static_cast<int>(MBBuffer.GetDoubleWord(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_WITH_BYTE_SWAP)) << std::endl;
            std::cout << "Double Word - Big Endian " << static_cast<int>(MBBuffer.GetDoubleWord(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP)) << std::endl;
            std::cout << "Double Word - Big Endian Bytes Swap " << static_cast<int>(MBBuffer.GetDoubleWord(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_WITH_BYTE_SWAP)) << std::endl << std::flush;

            std::cout << "Float 32 bits - Little Endian " << MBBuffer.GetFloat(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_NO_BYTE_SWAP) << std::endl;
            std::cout << "Float 32 bits - Little Endian Bytes Swap " << MBBuffer.GetFloat(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_WITH_BYTE_SWAP) << std::endl;
            std::cout << "Float 32 bits - Big Endian " << MBBuffer.GetFloat(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP) << std::endl;
            std::cout << "Float 32 bits - Big Endian Bytes Swap " << MBBuffer.GetFloat(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_WITH_BYTE_SWAP) << std::endl << std::flush;

            std::cout << "Quad Word - Little Endian " << static_cast<unsigned long>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_NO_BYTE_SWAP)) << std::endl;
            std::cout << "Quad Word - Little Endian Bytes Swap " << static_cast<unsigned long>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_WITH_BYTE_SWAP)) << std::endl;
            std::cout << "Quad Word - Big Endian " << static_cast<unsigned long>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP)) << std::endl;
            std::cout << "Quad Word - Big Endian Bytes Swap " << static_cast<unsigned long>(MBBuffer.GetQuadWord(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_WITH_BYTE_SWAP)) << std::endl << std::flush;

            std::cout << "Double 64 bits - Little Endian " << MBBuffer.GetDouble(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_NO_BYTE_SWAP) << std::endl;
            std::cout << "Double 64 bits - Little Endian Bytes Swap " << MBBuffer.GetDouble(GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_WITH_BYTE_SWAP) << std::endl;
            std::cout << "Double 64 bits - Big Endian " << MBBuffer.GetDouble(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP) << std::endl;
            std::cout << "Double 64 bits - Big Endian Bytes Swap " << MBBuffer.GetDouble(GMBUSLib::Types::ByteOrder::BIG_ENDIAN_WITH_BYTE_SWAP) << std::endl;

            std::cout << "Word - Little Endian " << static_cast<int>(MBBuffer.GetWord(GMBUSLib::Types::ByteOrder::BYTE_SWAP)) << std::endl << std::flush; });

    std::cout << "MB1 MapType = " << MB1.GetMapType() << std::endl;
    std::cout << "MB1 Slave ID = " << static_cast<int>(MB1.GetSlaveID()) << std::endl;
    MB1.SetSlaveID(1);
    std::cout << "MB1 Slave ID = " << static_cast<int>(MB1.GetSlaveID()) << std::endl;
    MB1.Connect();
    std::cout << "MB1 Number of polling messages = " << MB1.GetNumberOfPollingMessages() << std::endl
              << std::flush;
#endif

#ifdef EN_MB2
    GMBUSLib::MBDriver MB2(GMBUSLib::Types::MapType::Priority, "127.0.0.1", 502);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::High, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 0, 1, std::chrono::seconds(30), NULL);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::High, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 2, 1, std::chrono::seconds(30), NULL);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::High, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 3, 1, std::chrono::seconds(30), NULL);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::Normal, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 4, 1, std::chrono::seconds(30), NULL);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::Normal, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 5, 1, std::chrono::seconds(30), NULL);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::Low, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 6, 1, std::chrono::seconds(30), NULL);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::Low, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 7, 1, std::chrono::seconds(30), NULL);
    MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::Low, GMBUSLib::Types::ReadFC::FC1_READ_COILS, 8, 1, std::chrono::seconds(30), NULL);
    MB2.Connect();

#endif // MB2

    // MB2.AddPollingMessageToQueue(GMBUSLib::Types::Priority::High, GMBUSLib::Types::ReadFC::FC3_READ_REGS, 0, 1, std::chrono::milliseconds(1000), NULL);
    // MB2.Connect();
    // MB2.AddPollingMessageToQueue(std::chrono::milliseconds(40000), GMBUSLib::Types::ReadFC::FC2_READ_INPUT_BITS, 8, 1, std::chrono::milliseconds(2000), [](ModBusBuffer Buffer) {});
    // MB2.AddPollingMessageToQueue(std::chrono::milliseconds(50000), GMBUSLib::Types::ReadFC::FC3_READ_REGS, 1, 1, std::chrono::milliseconds(2000), [](ModBusBuffer Buffer) {});
    // MB2.AddPollingMessageToQueue(std::chrono::milliseconds(60000), GMBUSLib::Types::ReadFC::FC4_READ_INPUT_REGS, 2, 1, std::chrono::milliseconds(2000), [](ModBusBuffer Buffer) {});
    // MB2.Connect();
    std::cout << "While Loop" << std::endl;
    while (true)
    {
        std::cout << "Inside While Loop" << std::endl;
        int option = 10;
        std::cin >> option;
        std::cout << "option value " << option << std::endl
                  << std::flush;
        if (option == 0)
        {
#ifdef EN_MB1
            MB1.StartAutoPolling();
#endif
        }
        else if (option == 1)
        {
#ifdef EN_MB2
            MB2.StartAutoPolling();
#endif
        }
        else if (option == 2)
        {
#ifdef EN_MB1
            MB1.StopAutoPolling();
#endif
        }
        else if (option == 3)
        {
#ifdef EN_MB2
            MB2.StopAutoPolling();
#endif
        }
        else if (option == 4)
        {
#ifdef EN_MB1
            // MB1.DebugPrint = !MB1.DebugPrint;
            std::cout << "MB1 Status\n";
            PrintCommState(MB1.GetCommStatistics());
#endif
        }
        else if (option == 5)
        {
#ifdef EN_MB2
            // MB2.DebugPrint = !MB2.DebugPrint;
            std::cout << "MB2 Status\n";
            PrintCommState(MB2.GetCommStatistics());
#endif
        }
        else if (option == 6)
        {
#ifdef EN_MB1
            MB1.Connect();
#endif
        }
        else if (option == 7)
        {
#ifdef EN_MB2
            MB2.Connect();
#endif
        }
        else if (option == 8)
        {
#ifdef EN_MB1
            {
                // IntervalTimer Timer1;
                MB1.WriteSingleCoil(10, true, std::chrono::milliseconds(1000));
            }

            {
                // IntervalTimer Timer1;
                MB1.WriteSingleRegister(10, 12345, std::chrono::milliseconds(1000));
            }

            {
                // IntervalTimer Timer1;
                GMBUSLib::Buffer MBBuffer(GMBUSLib::Types::WriteFC::FC15_WRITE_MULTIPLE_COILS, 15);
                MBBuffer.rawBuffer[0] = 255;
                MBBuffer.rawBuffer[1] = 255;
                MB1.WriteMultipleCoils(0, MBBuffer, std::chrono::milliseconds(1000));
            }

            {
                // IntervalTimer Timer1;
                GMBUSLib::Buffer MBBuffer(GMBUSLib::Types::WriteFC::FC16_WRITE_MULTIPLE_REGISTERS, 4);
                MBBuffer.rawBuffer[0] = 255;
                MBBuffer.rawBuffer[1] = 255;
                MBBuffer.rawBuffer[2] = 0;
                MBBuffer.rawBuffer[3] = 255;
                MBBuffer.rawBuffer[4] = 255;
                MBBuffer.rawBuffer[5] = 0;
                MBBuffer.rawBuffer[6] = 127;
                MBBuffer.rawBuffer[7] = 128;
                std::cout << "Error code = " << static_cast<int16_t>(MB1.WriteMultipleRegisters(0, MBBuffer, std::chrono::milliseconds(1000))) << std::endl
                          << std::flush;
            }

            {
                // IntervalTimer Timer1;
                GMBUSLib::Buffer MBBuffer;
                MBBuffer.SetType(GMBUSLib::Types::ReadFC::FC1_READ_COILS, 5);
                MB1.ReadCoils(0, 4, MBBuffer, std::chrono::milliseconds(100000));
                std::cout << "ReadCoils - Size(bits)[" << static_cast<uint16_t>(MBBuffer.GetSize(GMBUSLib::Types::SizeTypes::BITS)) << "]" << std::endl
                          << std::flush;
                while (MBBuffer.GetTotalBitsAvailable() > 0)
                {
                    std::cout << "          - bit = " << static_cast<uint16_t>(MBBuffer.GetBit()) << "]" << std::endl;
                }
                std::cout << std::flush;
            }

            {
                // IntervalTimer Timer1;
                GMBUSLib::Buffer MBBuffer;
                MB1.ReadDiscreteInputs(0, 17, MBBuffer, std::chrono::milliseconds(1000));
                std::cout << "ReadDiscreteInputs - Size(bits)[" << static_cast<uint16_t>(MBBuffer.GetSize(GMBUSLib::Types::SizeTypes::BITS)) << "]" << std::endl
                          << std::flush;
                while (MBBuffer.GetTotalBitsAvailable() > 0)
                {
                    std::cout << "                   - bit = " << static_cast<uint16_t>(MBBuffer.GetBit()) << "]" << std::endl;
                }
                std::cout << std::flush;
            }

            {
                // IntervalTimer Timer1;
                GMBUSLib::Buffer MBBuffer;
                MB1.ReadHoldingRegisters(2, 10, MBBuffer, std::chrono::milliseconds(1000));
                std::cout << "ReadHoldingRegisters - Size(words)[" << static_cast<uint16_t>(MBBuffer.GetSize(GMBUSLib::Types::SizeTypes::WORDS)) << "]" << std::endl
                          << std::flush;
                while (MBBuffer.GetTotalWordsAvailable() > 0)
                {
                    std::cout << "                   - word = " << static_cast<uint16_t>(MBBuffer.GetWord(GMBUSLib::Types::ByteOrder::BYTE_SWAP)) << "]" << std::endl;
                }
                std::cout << std::flush;
            }

            {
                // IntervalTimer Timer1;
                GMBUSLib::Buffer MBBuffer;
                MB1.ReadInputRegisters(0, 4, MBBuffer, std::chrono::milliseconds(1000));
                std::cout << "ReadInputRegisters - Size(words)[" << static_cast<uint16_t>(MBBuffer.GetSize(GMBUSLib::Types::SizeTypes::WORDS)) << "]" << std::endl
                          << std::flush;
                while (MBBuffer.GetTotalWordsAvailable() > 0)
                {
                    std::cout << "                   - word = " << static_cast<uint16_t>(MBBuffer.GetWord(GMBUSLib::Types::ByteOrder::BYTE_SWAP)) << "]" << std::endl;
                }
                std::cout << std::flush;
            }
#endif
        }
        else if (option == 9)
        {
            std::cout << "Exit - Option 9\n";
            break;
        };
    }

    /*
    // create a modbus object
    modbus mb = modbus("127.0.0.1", 502);

    // set slave id
    mb.modbus_set_slave_id(1);

    // connect with the server
    mb.modbus_connect();

    uint16_t read_holding_regs[1];
    while(true)
    {
        // read holding registers           function 0x03
        mb.modbus_read_holding_registers(0, 1, read_holding_regs);
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (mb.err)
        {
            std::cout << "Error " << mb.err << "  number " << mb.err_no << " " << mb.error_msg << std::endl << std::flush;
            return -1;
        }
    }

    std::cout << read_holding_regs[0] << std::endl;

    // write multiple regs              function 0x10
    uint16_t write_regs[4] = { 123, 123, 123 };
    mb.modbus_write_registers(0, 4, write_regs);

    // close connection and free the memory
    mb.modbus_close();
    */
    return 0;
}
