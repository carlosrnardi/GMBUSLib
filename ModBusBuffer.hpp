#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cmath>

#include "ModbusDriverTypes.hpp"
namespace GMBUSLib
{

	class Buffer // ModBusBuffer
	{
		std::pair<uint8_t,uint8_t> BufferSizeCalc(GMBUSLib::Types::ReadFC ReadFC, uint8_t Size);
		std::pair<uint8_t, uint8_t> BufferSizeCalc(GMBUSLib::Types::WriteFC WriteFC, uint8_t Size);
		template<typename T>
		T GetData();
	public:
		std::vector<uint8_t> rawBuffer;
		uint8_t BufferByteConsumed = 0;
		uint8_t BufferBitConsumed = 0;
		uint8_t BufferAlign = 0;
		Buffer();
		//ModBusBuffer(const ModBusBuffer& p1);
		Buffer& operator=(Buffer other);
		void SetType(GMBUSLib::Types::ReadFC ReadFC, uint8_t Size);
		void SetType(GMBUSLib::Types::WriteFC WriteFC, uint8_t Size);
		Buffer(GMBUSLib::Types::ReadFC ReadFC, uint8_t Size);
		Buffer(GMBUSLib::Types::WriteFC WriteFC, uint8_t Size);
	

		void SkipBits(uint16_t Size);
		//Read Buffer Functions
		[[nodiscard]] bool GetBit();
		[[nodiscard]] uint16_t GetBits(uint8_t Size);
		[[nodiscard]] uint8_t GetByte();
		[[nodiscard]] uint16_t GetWord();
		[[nodiscard]] uint16_t GetWord(GMBUSLib::Types::ByteOrder ByteOrder);
		[[nodiscard]] uint32_t GetDoubleWord();
		[[nodiscard]] uint32_t GetDoubleWord(GMBUSLib::Types::ByteOrder  ByteOrder);
		[[nodiscard]] uint64_t GetQuadWord();
		[[nodiscard]] uint64_t GetQuadWord(GMBUSLib::Types::ByteOrder  ByteOrder);
		[[nodiscard]] float GetFloat();
		[[nodiscard]] float GetFloat(GMBUSLib::Types::ByteOrder  ByteOrder);
		[[nodiscard]] double GetDouble();
		[[nodiscard]] double GetDouble(GMBUSLib::Types::ByteOrder  ByteOrder);
		//Write Buffer Functions
		void SetBit(const bool value);
		uint16_t GetTotalBitsAvailable();
		uint16_t GetTotalBytesAvailable();
		uint16_t GetTotalWordsAvailable();
		uint16_t GetTotalDoubleWordsAvailable();
		uint16_t GetSize();
		uint16_t GetSize(GMBUSLib::Types::SizeTypes Type);
		uint16_t GetBufferSize();
	};
}