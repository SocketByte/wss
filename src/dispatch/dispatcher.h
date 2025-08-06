#ifndef DISPATCH_H
#define DISPATCH_H
#include "CLI/CLI11.hpp"
#include "zmq_req.h"

namespace WSS {
class Dispatcher {
  private:
    ZMQReq m_ZMQReq;

  public:
    Dispatcher() = default;
    ~Dispatcher() = default;

    void InitCommands(CLI::App& app);
};
} // namespace WSS

#endif // DISPATCH_H
