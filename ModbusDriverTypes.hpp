#pragma once
#include <iostream>
#include <vector>
/*
#include <thread>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <future>
#include <cmath>
*/

namespace GMBUSLib::Types // ModbusDriverTypes
{
    using WordBuffer = std::vector<uint16_t>;
    using ByteBuffer = std::vector<uint8_t>;

    enum class MapType {
        Priority = 0,
        PollingInterval = 1
    };

    std::ostream& operator << (std::ostream& os, const MapType& obj);

    enum class SizeTypes {

        BITS = 0, BYTES = 1, WORDS = 2, DOUBLE_WORDS = 3
    };

    enum class Priority {
        High, Normal, Low
    };

    enum class ReadFC {
        NONE = 0, FC1_READ_COILS = 1, FC2_READ_INPUT_BITS = 2, FC3_READ_REGS = 3, FC4_READ_INPUT_REGS = 4
    };
    enum class WriteFC {
        NONE = 0, FC5_WRITE_SINGLE_COIL = 5, FC6_WRITE_SINGLE_REGISTER = 6, FC15_WRITE_MULTIPLE_COILS = 15, FC16_WRITE_MULTIPLE_REGISTERS = 16
    };

    enum class PollingState {
        None = 0, Sleep = 1, Queue = 2, ToSend = 3, Sent = 4, Error = 5, Timeout = 6
    };

    enum class ByteOrder {
        NO_BYTE_SWAP = 0, BYTE_SWAP = 1, LITTLE_ENDIAN_NO_BYTE_SWAP = 2, LITTLE_ENDIAN_WITH_BYTE_SWAP = 3, BIG_ENDIAN_NO_BYTE_SWAP = 4, BIG_ENDIAN_WITH_BYTE_SWAP = 5
    };
}
