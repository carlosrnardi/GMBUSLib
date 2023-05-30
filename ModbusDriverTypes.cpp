#include "ModbusDriverTypes.hpp"
namespace GMBUSLib::Types
{
    std::ostream& operator << (std::ostream& os, const MapType& obj)
    {
        switch (obj)
        {
        case MapType::Priority:
            os << "Priority";
            break;
        case MapType::PollingInterval:
            os << "PollingInterval";
            break;
        }
        return os;
    };
}