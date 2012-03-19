/*
** Author(s):
**  - Cedric GESTES <gestes@aldebaran-robotics.com>
**
** Copyright (C) 2010, 2012 Aldebaran Robotics
*/


#include <vector>
#include <iostream>
#include <sstream>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include <qimessaging/transport_server.hpp>
#include <qimessaging/transport_socket.hpp>

#include <qimessaging/session.hpp>
#include "dataperftimer.hpp"

#include <qimessaging/service_directory.hpp>
#include <qimessaging/gateway.hpp>

#include <qimessaging/transport_server.hpp>
#include <qimessaging/url.hpp>
#include "../src/network_thread.hpp"
#include "../src/session_p.hpp"

static int gLoopCount = 10000;
static const int gThreadCount = 1;

#include <iostream>


class ServerEventPrivate : public qi::TransportServerInterface, public qi::TransportSocketInterface {
public:
  ServerEventPrivate()
  {
    _dp = new qi::perf::DataPerfTimer("Transport synchronous call");
    _msgRecv = 0;
    _numBytes = 0;
  }

  virtual void newConnection()
  {
    qi::TransportSocket *socket = _ts.nextPendingConnection();
    if (!socket)
      return;
    socket->setCallbacks(this);
  }

  virtual void onSocketReadyRead(qi::TransportSocket *client, int id)
  {
    qi::Message msg;
    client->read(id, &msg);
    _msgRecv++;
    int s = msg.buffer().size();
    if (s != _numBytes)
    {
      _dp->stop(1);
      if (_msgRecv != gLoopCount)
        std::cout << "Drop " << gLoopCount - _msgRecv << " messages!" << std::endl;

      _numBytes = s;
       _msgRecv = 0;
      _dp->start(gLoopCount, _numBytes);
    }

  }

  virtual void onSocketWriteDone(qi::TransportSocket *client)
  {
  }

  virtual void onSocketConnected(qi::TransportSocket *client)
  {
  }

  virtual void onSocketDisconnected(qi::TransportSocket *client)
  {
  }

public:
  std::map<unsigned int, qi::Object*> _services;
  qi::TransportServer                 _ts;
  std::vector<std::string>            _endpoints;
  qi::Session                        *_session;

  qi::perf::DataPerfTimer            *_dp;
  int                                 _msgRecv;
  unsigned int                        _numBytes;
};

class ServerEvent
{
public:
  ServerEvent()
    : _p(new ServerEventPrivate())
  {
  }

  ~ServerEvent()
  {
    delete _p;
  }

  void listen(qi::Session *session, const std::vector<std::string> &endpoints)
  {
    _p->_endpoints = endpoints;
    _p->_session = session;

    qi::Url urlo(_p->_endpoints[0]);

    _p->_ts.setCallbacks(_p);
    _p->_ts.start(session, urlo);
  }


  unsigned int registerService(const std::string &name, qi::Object *obj)
  {
    qi::Message     msg;
    qi::ServiceInfo si;
    msg.setType(qi::Message::Type_Event);
    msg.setService(qi::Message::Service_ServiceDirectory);
    msg.setPath(qi::Message::Path_Main);
    msg.setFunction(qi::Message::ServiceDirectoryFunction_RegisterService);

    qi::DataStream d(msg.buffer());
    si.setName(name);
    si.setProcessId(qi::os::getpid());
    si.setMachineId("TODO");
    si.setEndpoints(_p->_endpoints);
    d << si;

    _p->_session->_p->_serviceSocket->send(msg);
    _p->_session->_p->_serviceSocket->waitForId(msg.id());
    qi::Message ans;
    _p->_session->_p->_serviceSocket->read(msg.id(), &ans);
    qi::DataStream dout(ans.buffer());
    unsigned int idx = 0;
    dout >> idx;
    _p->_services[idx] = obj;
    return idx;
  }


  void stop() {
  }

private:
  ServerEventPrivate *_p;
};


int main_client(std::string src)
{
  unsigned int serviceId;
  qi::Session  session;
  session.connect("tcp://127.0.0.1:5555");
  session.waitForConnected();

  qi::TransportSocket *sock = session.serviceSocket("serviceTest", &serviceId);

  for (int i = 0; i < 12; ++i)
  {
    char character = 'c';
    unsigned int numBytes = (unsigned int)pow(2.0f, (int)i);
    std::string requeststr = std::string(numBytes, character);

    for (int j = 0; j < gLoopCount; ++j)
    {
      static int id = 1;
      id++;
      char c = 1;
      c++;
      if (c == 0)
        c++;
      requeststr[2] = c;

      qi::Message msg;
      qi::DataStream d(msg.buffer());
      d << requeststr;

      msg.setType(qi::Message::Type_Event);
      msg.setService(serviceId);
      msg.setPath(qi::Message::Path_Main);
      msg.setFunction(1);
      sock->send(msg);
    }
  }
  return 0;
}

void start_client(int count)
{
  boost::thread thd[100];

  assert(count < 100);

  for (int i = 0; i < count; ++i)
  {
    std::stringstream ss;
    ss << "remote" << i;
    std::cout << "starting thread: " << ss.str() << std::endl;
    thd[i] = boost::thread(boost::bind(&main_client, ss.str()));
  }

  for (int i = 0; i < count; ++i)
    thd[i].join();
}


#include <qi/os.hpp>


int main_gateway()
{
  qi::Gateway       gate;

  gate.listen("tcp://127.0.0.1:12345", "tcp://127.0.0.1:5555");
  std::cout << "ready." << std::endl;
  gate.join();

  return 0;
}

#include <qimessaging/server.hpp>

std::string reply(const std::string &msg)
{
  return msg;
}

int main_server()
{
  qi::ServiceDirectory sd;
  sd.listen("tcp://127.0.0.1:5555");
  std::cout << "Service Directory ready." << std::endl;

  qi::Session session;
  qi::Object  obj;
  ServerEvent srv;
  obj.advertiseMethod("reply", &reply);

  session.connect("tcp://127.0.0.1:5555");
  session.waitForConnected();

  std::vector<std::string> endpoints;
  endpoints.push_back("tcp://127.0.0.1:9559");
  srv.listen(&session, endpoints);
  srv.registerService("serviceTest", &obj);
  std::cout << "serviceTest ready." << std::endl;

  session.join();

  srv.stop();
  session.disconnect();

  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "--client"))
  {
    int threadc = 1;
    if (argc > 2)
      threadc = atoi(argv[2]);
    start_client(threadc);
  }
  else if (argc > 1 && !strcmp(argv[1], "--server"))
  {
    return main_server();
  }
  else
  {
    //start the server
    boost::thread threadServer1(boost::bind(&main_server));
    qi::os::sleep(1);
    start_client(gThreadCount);
  }
  return 0;
}
