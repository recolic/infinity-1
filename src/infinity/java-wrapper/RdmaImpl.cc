#include "RdmaImpl.hpp"

core::Context *context = nullptr;
queues::QueuePairFactory *qpFactory = nullptr;

#if HUST
namespace std {
std::string to_string(magic_t &m) {return std::to_string((long long unsigned int)m);}
std::string to_string(int &m) {return std::to_string((long long int)m);}
}
#endif
