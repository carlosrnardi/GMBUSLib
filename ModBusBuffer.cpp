#include "ModBusBuffer.hpp"

std::pair<uint8_t, uint8_t> GMBUSLib::Buffer::BufferSizeCalc(GMBUSLib::Types::ReadFC ReadFC, uint8_t Size)
{
    if (Size <= 0) return std::pair<uint8_t, uint8_t>(0, 0);

    uint8_t _Size = 0;
    uint8_t _Align = 0;

    switch (ReadFC)
    {
    case GMBUSLib::Types::ReadFC::NONE:
        break;
    case GMBUSLib::Types::ReadFC::FC1_READ_COILS:
        _Size = static_cast<uint8_t>(std::ceil(static_cast<double>(Size / 8.0)));
        _Align = static_cast<uint8_t>((_Size * 8) - Size);
        break;
    case GMBUSLib::Types::ReadFC::FC2_READ_INPUT_BITS:
        _Size = static_cast<uint8_t>(std::ceil(static_cast<double>(Size / 8.0)));
        _Align = static_cast<uint8_t>((_Size * 8) - Size);
        break;
    case GMBUSLib::Types::ReadFC::FC3_READ_REGS:
        _Size = static_cast<uint8_t>(Size * 2);
        _Align = 0;
        break;
    case GMBUSLib::Types::ReadFC::FC4_READ_INPUT_REGS:
        _Size = static_cast<uint8_t>(Size * 2);
        _Align = 0;
        break;
    default:
        break;
    }
    return std::pair<uint8_t, uint8_t>(_Size, _Align);
}

std::pair<uint8_t, uint8_t> GMBUSLib::Buffer::BufferSizeCalc(GMBUSLib::Types::WriteFC WriteFC, uint8_t Size)
{
    if (Size <= 0) return std::pair<uint8_t, uint8_t>(0, 0);

    uint8_t _Size = 0;
    uint8_t _Align = 0;

    switch (WriteFC)
    {
    case GMBUSLib::Types::WriteFC::NONE:
        break;
    case GMBUSLib::Types::WriteFC::FC5_WRITE_SINGLE_COIL:
        _Size = uint8_t(2);
        _Align = 0;
        break;
    case GMBUSLib::Types::WriteFC::FC6_WRITE_SINGLE_REGISTER:
        _Size = uint8_t(2);
        _Align = 0;
        break;
    case GMBUSLib::Types::WriteFC::FC15_WRITE_MULTIPLE_COILS:
        _Size = static_cast<uint8_t>(std::ceil(static_cast<double>(Size / 8.0)));
        _Align = static_cast<uint8_t>((_Size * 8) - Size);
        break;
    case GMBUSLib::Types::WriteFC::FC16_WRITE_MULTIPLE_REGISTERS:
        _Size = static_cast<uint8_t>(Size * 2);
        _Align = 0;
        break;
    default:
        break;
    }
    return std::pair<uint8_t, uint8_t>(_Size, _Align);
}

GMBUSLib::Buffer::Buffer()
{
}

//GMBUSLib::ModBusBuffer::GMBUSLib::ModBusBuffer(const GMBUSLib::ModBusBuffer& p1)
//{
//    std::cout << "GMBUSLib::ModBusBuffer Copy contructor called. ...\n";
//    //BufferRead.resize(p1.BufferRead.size());
//    //BufferAlign = p1.BufferAlign;
//    //for (uint8_t counter = 0; counter < p1.BufferRead.size(); counter++)
//    //{
//    //    if (counter == p1.BufferRead.size())
//    //    {
//    //        uint8_t mask = uint8_t((2 ^ ((8 - BufferAlign) + 1)) - 1);
//    //        BufferRead[counter] = p1.BufferRead[counter] & mask;
//    //    }
//    //    else
//    //    {
//    //        BufferRead[counter] = p1.BufferRead[counter];
//    //    }
//    //    
//    //}
//}

GMBUSLib::Buffer& GMBUSLib::Buffer::operator=(GMBUSLib::Buffer other)
{
    //std::cout << "GMBUSLib::ModBusBuffer Assignment contructor called. ...\n";
    rawBuffer.resize(other.rawBuffer.size());
    BufferAlign = other.BufferAlign;
    for (uint8_t counter = 0; counter < other.rawBuffer.size(); counter++)
    {
        if (counter == other.rawBuffer.size()-1)
        {
            uint8_t mask = uint8_t(0xffu >> other.BufferAlign);
            rawBuffer[counter] = other.rawBuffer[counter] & mask;
        }
        else
        {
            rawBuffer[counter] = other.rawBuffer[counter];
        }
    }
    return *this;
}

void GMBUSLib::Buffer::SetType(GMBUSLib::Types::ReadFC ReadFC, uint8_t Size)
{

    std::pair<uint8_t, uint8_t> _BuffSize = BufferSizeCalc(ReadFC, Size);
    rawBuffer.resize(_BuffSize.first);
    BufferAlign = _BuffSize.second;
}

void GMBUSLib::Buffer::SetType(GMBUSLib::Types::WriteFC WriteFC, uint8_t Size)
{
    std::pair<uint8_t, uint8_t> _BuffSize = BufferSizeCalc(WriteFC, Size);
    rawBuffer.resize(_BuffSize.first);
    BufferAlign = _BuffSize.second;
}

GMBUSLib::Buffer::Buffer(GMBUSLib::Types::ReadFC ReadFC, uint8_t Size)
{
    SetType(ReadFC, Size);
}

GMBUSLib::Buffer::Buffer(GMBUSLib::Types::WriteFC WriteFC, uint8_t Size)
{
    SetType(WriteFC, Size);
}

void GMBUSLib::Buffer::SkipBits(uint16_t Size)
{
    if (Size == 0) return;
    if (Size > GetTotalBitsAvailable()) throw std::invalid_argument("Buffer available < amount of bits requested");
    BufferByteConsumed = static_cast<uint8_t>(BufferByteConsumed + static_cast<uint8_t>(std::floor((static_cast<double>(BufferBitConsumed + Size) / 8.0))));
    BufferBitConsumed = static_cast<uint8_t>((BufferBitConsumed + Size) % 8);
}

[[nodiscard]] bool GMBUSLib::Buffer::GetBit()
{
    if (rawBuffer.size() == 0 || GetTotalBitsAvailable() == 0) {
        throw std::invalid_argument("Buffer Empty");
        return false;
    }

    bool _returnValue = static_cast<bool>(((rawBuffer[BufferByteConsumed]) & static_cast<uint8_t>(pow(2, BufferBitConsumed))) >> BufferBitConsumed);
    BufferByteConsumed = static_cast<uint8_t>(BufferByteConsumed + static_cast<uint8_t>(std::floor((static_cast<double>(BufferBitConsumed + 1) / 8.0))));
    BufferBitConsumed = static_cast<uint8_t>((BufferBitConsumed + 1) % 8);
    return _returnValue;
}

[[nodiscard]] uint16_t GMBUSLib::Buffer::GetBits(uint8_t Size)
{
    if (Size == 0) return 0;
    if (Size > 16) throw std::invalid_argument("DataType size < amount of bits requested");
    if (Size > GetTotalBitsAvailable()) throw std::invalid_argument("Buffer available < amount of data requested");
    uint16_t _returnValue = 0;
    if (BufferBitConsumed == 0)
    {
        _returnValue = ((rawBuffer[BufferByteConsumed]) & static_cast<uint8_t>(pow(2, Size) - 1));
    }
    else
    {
        uint8_t _returnBytePosition = 0;
        uint8_t _TempSize = Size;
        uint8_t _TempBufferBitConsumed = BufferBitConsumed;
        uint8_t _ByteShift = 0;
        do {
            uint8_t _BitsToConsume = 0;
            if (_TempSize > (8 - _TempBufferBitConsumed))
            {
                _BitsToConsume = static_cast<uint8_t>(8 - _TempBufferBitConsumed);
            }
            else
            {
                _BitsToConsume = _TempSize;
            }
            _TempSize = static_cast<uint8_t>(_TempSize - _BitsToConsume);

            _returnValue = uint16_t(_returnValue + ((((rawBuffer[BufferByteConsumed + _ByteShift]) & uint16_t((static_cast<uint8_t>((pow(2, _BitsToConsume) - 1)) << _TempBufferBitConsumed))) >> _TempBufferBitConsumed) << _returnBytePosition));
            _TempBufferBitConsumed = 0;
            _returnBytePosition = int8_t(_returnBytePosition + _BitsToConsume);
            _ByteShift = uint8_t(_ByteShift+1);
        } while (_TempSize > 0);
    }
    BufferByteConsumed = static_cast<uint8_t>(BufferByteConsumed + static_cast<uint8_t>(std::floor((static_cast<double>(BufferBitConsumed + Size) / 8.0))));
    BufferBitConsumed = static_cast<uint8_t>((BufferBitConsumed + Size) % 8);
    return _returnValue;
}

template<typename T>
T GMBUSLib::Buffer::GetData()
{
    uint8_t Size = sizeof(T);
    if (GetTotalBytesAvailable() < Size) throw std::invalid_argument("Buffer available < amount of data requested");
    Size = uint8_t(Size * 8);
    T _returnValue = 0;
    if (BufferBitConsumed == 0)
    {

        for (auto counter = 0; counter < (Size / 8); counter++)
        {
            
            T _ByteShift = static_cast<T>(counter * 8);
            T _ValuetoAdd = static_cast<T>(static_cast<T>(rawBuffer[BufferByteConsumed + counter]) << _ByteShift);
            _returnValue = static_cast<T>(_returnValue + _ValuetoAdd);
        }
        
    }
    else
    {
        uint8_t _returnBytePosition = 0;
        uint8_t _TempSize = Size;
        uint8_t _TempBufferBitConsumed = BufferBitConsumed;
        uint8_t _ByteShift = 0;
        do {
            uint8_t _BitsToConsume = 0;
            if (_TempSize > (8 - _TempBufferBitConsumed))
            {
                _BitsToConsume = static_cast<uint8_t>(8 - _TempBufferBitConsumed);
            }
            else
            {
                _BitsToConsume = _TempSize;
            }
            _TempSize = static_cast<uint8_t>(_TempSize - _BitsToConsume);
            _returnValue = static_cast<T>(_returnValue + ((((rawBuffer[BufferByteConsumed + _ByteShift]) & uint8_t((static_cast<uint8_t>((pow(2, _BitsToConsume) - 1)) << _TempBufferBitConsumed))) >> _TempBufferBitConsumed) << _returnBytePosition));
            _TempBufferBitConsumed = 0;
            _returnBytePosition = int8_t(_returnBytePosition + _BitsToConsume);
            _ByteShift = uint8_t(_ByteShift + 1);
        } while (_TempSize > 0);
    }
    BufferByteConsumed = static_cast<uint8_t>(BufferByteConsumed + static_cast<uint8_t>(std::floor((static_cast<double>(BufferBitConsumed + Size) / 8.0))));
    BufferBitConsumed = static_cast<uint8_t>((BufferBitConsumed + Size) % 8);


    return T(_returnValue);
}

uint8_t GMBUSLib::Buffer::GetByte()
{
    return GetData<uint8_t>();
}

uint16_t GMBUSLib::Buffer::GetWord()
{
    return GetData<uint16_t>();
}

uint16_t GMBUSLib::Buffer::GetWord(GMBUSLib::Types::ByteOrder ByteOrder)
{
    uint16_t _resultValue = GetData<uint16_t>();

    
    if (ByteOrder == GMBUSLib::Types::ByteOrder::BYTE_SWAP)
    {
        _resultValue = uint16_t(((_resultValue << 8) & 0xFF00) | ((_resultValue >> 8) & 0x00FF));
    }
    
    return _resultValue;
}

uint32_t GMBUSLib::Buffer::GetDoubleWord()
{
    return GetData<uint32_t>();
}

uint32_t GMBUSLib::Buffer::GetDoubleWord(GMBUSLib::Types::ByteOrder ByteOrder)
{
    uint32_t _resultValue = GetData<uint32_t>();

    if (ByteOrder == GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_WITH_BYTE_SWAP || ByteOrder == GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP || ByteOrder == GMBUSLib::Types::ByteOrder::BYTE_SWAP)
    {
        _resultValue = uint32_t(((_resultValue << 8) & 0xFF00FF00) | ((_resultValue >> 8) & 0x00FF00FF));
    }

    if (ByteOrder == GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP || ByteOrder == GMBUSLib::Types::ByteOrder::BIG_ENDIAN_WITH_BYTE_SWAP)
    {
        _resultValue = uint32_t(((_resultValue << 16) & 0xFFFF0000) | ((_resultValue >> 16) & 0x0000FFFF));
    }

    return _resultValue;
}

uint64_t GMBUSLib::Buffer::GetQuadWord()
{
    return GetData<uint64_t>();
}

uint64_t GMBUSLib::Buffer::GetQuadWord(GMBUSLib::Types::ByteOrder ByteOrder)
{
    uint64_t _resultValue = GetData<uint64_t>(); //double words value
    //uint64_t _resultValue2 = _resultValue;  //received value
    //uint64_t _resultValue3 = _resultValue;  //bytes swaped
    //uint64_t _resultValue4 = _resultValue;  //word swaped

    if (ByteOrder == GMBUSLib::Types::ByteOrder::LITTLE_ENDIAN_WITH_BYTE_SWAP || ByteOrder == GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP || ByteOrder == GMBUSLib::Types::ByteOrder::BYTE_SWAP)
    {
        _resultValue = uint64_t(((_resultValue << 8) & 0xFF00FF00FF00FF00) | ((_resultValue >> 8) & 0x00FF00FF00FF00FF));
        //_resultValue3 = _resultValue;
    }

    if (ByteOrder == GMBUSLib::Types::ByteOrder::BIG_ENDIAN_NO_BYTE_SWAP || ByteOrder == GMBUSLib::Types::ByteOrder::BIG_ENDIAN_WITH_BYTE_SWAP)
    {
        _resultValue = uint64_t(((_resultValue << 16) & 0xFFFF0000FFFF0000ull) | ((_resultValue >> 16) & 0x0000FFFF0000FFFFull));
        //_resultValue4 = _resultValue;
        _resultValue = uint64_t(((_resultValue << 32) & 0xFFFFFFFF00000000ull) | ((_resultValue >> 32) & 0x00000000FFFFFFFFull));
    }

    return _resultValue;
}

float GMBUSLib::Buffer::GetFloat()
{
    uint32_t _resultValue = GetDoubleWord();

    return reinterpret_cast<float&>(_resultValue);
}

float GMBUSLib::Buffer::GetFloat(GMBUSLib::Types::ByteOrder ByteOrder)
{
    uint32_t _resultValue = GetDoubleWord(ByteOrder);
  
    return reinterpret_cast<float&>(_resultValue);
}

double GMBUSLib::Buffer::GetDouble()
{
    uint64_t _resultValue = GetQuadWord();

    return reinterpret_cast<double&>(_resultValue);
}

double GMBUSLib::Buffer::GetDouble(GMBUSLib::Types::ByteOrder ByteOrder)
{
    uint64_t _resultValue = GetQuadWord(ByteOrder);

    return reinterpret_cast<double&>(_resultValue);
}

void GMBUSLib::Buffer::SetBit(const bool value)
{
    if (((static_cast<uint8_t>(rawBuffer.size()) * 8) - BufferAlign) > ((BufferByteConsumed * 8) + BufferBitConsumed))
    {
        if (value)
        {
            uint8_t BitMask = static_cast<uint8_t>(pow(2, BufferBitConsumed));
            rawBuffer[BufferByteConsumed] = rawBuffer[BufferByteConsumed] | BitMask;
        }
        else
        {
            uint8_t BitMask =  static_cast<uint8_t>(255.0 - pow(2, BufferBitConsumed));
            rawBuffer[BufferByteConsumed] = rawBuffer[BufferByteConsumed] & BitMask;
        }
        
    }
    else
    {
        throw std::invalid_argument("Buffer available < amount of data requested");
    }
    BufferByteConsumed = static_cast<uint8_t>(BufferByteConsumed + static_cast<uint8_t>(std::floor((static_cast<double>(BufferBitConsumed + 1) / 8.0))));
    BufferBitConsumed = static_cast<uint8_t>((BufferBitConsumed + 1) % 8);
}

uint16_t GMBUSLib::Buffer::GetTotalBitsAvailable()
{

    return uint16_t(((rawBuffer.size() - BufferByteConsumed) * 8) - BufferBitConsumed - BufferAlign);
}

uint16_t GMBUSLib::Buffer::GetTotalBytesAvailable()
{
    return uint16_t((rawBuffer.size() - BufferByteConsumed) - uint16_t(std::ceil((BufferBitConsumed + BufferAlign) / 8.0)));
}

uint16_t GMBUSLib::Buffer::GetTotalWordsAvailable()
{
    return uint16_t(std::floor(GetTotalBytesAvailable() / 2.0));
}

uint16_t GMBUSLib::Buffer::GetTotalDoubleWordsAvailable()
{
    return uint16_t(std::floor(GetTotalBytesAvailable() / 4.0));
}

uint16_t GMBUSLib::Buffer::GetSize()
{
    return GetSize(GMBUSLib::Types::SizeTypes::BYTES);
}

uint16_t GMBUSLib::Buffer::GetSize(GMBUSLib::Types::SizeTypes Type)
{
    uint16_t _Size = static_cast<uint16_t>((rawBuffer.size() * 8) - BufferAlign);

    if (Type == GMBUSLib::Types::SizeTypes::BITS)
    {
        return _Size;
    }

    _Size = static_cast<uint16_t>(std::floor(static_cast<double>(_Size / 8.0)));

    if (Type == GMBUSLib::Types::SizeTypes::BYTES)
    {
        return _Size;
    }

    _Size = static_cast<uint16_t>(std::floor(static_cast<double>(_Size / 2.0)));

    if (Type == GMBUSLib::Types::SizeTypes::WORDS)
    {
        return _Size;
    }

    _Size = static_cast<uint16_t>(std::floor(static_cast<double>(_Size / 2.0)));

    return _Size;
}

uint16_t GMBUSLib::Buffer::GetBufferSize()
{
    return uint16_t(rawBuffer.size());
}
