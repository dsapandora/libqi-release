/*
** Author(s):
**  - Herve Cuche <hcuche@aldebaran-robotics.com>
**
** Copyright (C) 2012 Aldebaran Robotics
*/

#include <iostream>
#include <cstring>
#include <map>
#include <stdint.h>

#include <event2/util.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "src/transport_socket_p.hpp"
#include "src/transport_socket_libevent_p.hpp"
#include "src/network_thread.hpp"
#include "src/message_p.hpp"
#include "src/buffer_p.hpp"
#include "src/session_p.hpp"

#include <qi/log.hpp>
#include <qimessaging/session.hpp>
#include <qimessaging/message.hpp>
#include <qimessaging/datastream.hpp>
#include <qimessaging/buffer.hpp>

#define MAX_LINE 16384

namespace qi
{
  static void readcb(struct bufferevent *bev,
                     void *context)
  {
    TransportSocketLibEvent *tc = static_cast<TransportSocketLibEvent*>(context);
    tc->readcb(bev, context);
  }

  static void writecb(struct bufferevent* bev,
                      void* context)
  {
    TransportSocketLibEvent *tc = static_cast<TransportSocketLibEvent*>(context);
    tc->writecb(bev, context);
  }


  static void eventcb(struct bufferevent *bev,
                      short error,
                      void *context)
  {
    TransportSocketLibEvent *tc = static_cast<TransportSocketLibEvent*>(context);
    tc->eventcb(bev, error, context);
  }


  void TransportSocketLibEvent::readcb(struct bufferevent *bev,
                                       void               *QI_UNUSED(context))
  {
    struct evbuffer *input = bufferevent_get_input(bev);
    int              msgId = 0;
    unsigned int magicValue = qi::MessagePrivate::magic;

    while (true)
    {
      if (msg == NULL)
      {
        msg = new qi::Message();
        readHdr = true;
      }

      if (readHdr)
      {
        if (evbuffer_get_length(input) < sizeof(MessagePrivate::MessageHeader))
          return;

        struct evbuffer_ptr p;
        evbuffer_ptr_set(input, &p, 0, EVBUFFER_PTR_SET);
        // search a messageMagic number
        p = evbuffer_search(input, (const char *)&magicValue, sizeof(uint32_t), &p);
        if (p.pos < 0)
        {
          qiLogWarning("qimessaging.TransportSocketLibevent") << "No magic found in the message. Waiting for more data.";
          return;
        }
        // Drain to the magic
        evbuffer_drain(input, p.pos);
        // Get the message header
        // there is a copy here.
        evbuffer_copyout(input, msg->_p->getHeader(), sizeof(MessagePrivate::MessageHeader));
        // check if the msg is valid
        if (!msg->isValid())
        {
          qiLogError("qimessaging.TransportSocketLibevent") << "Message received is invalid! Try to find a new one.";
          // only drop the magic and restart scanning
          evbuffer_drain(input, sizeof(uint32_t));
          return;
        }
        // header is valid, next step get all the buffer
        // remove the header from the evbuffer
        evbuffer_drain(input, sizeof(MessagePrivate::MessageHeader));

        readHdr = false;
      }

      if (evbuffer_get_length(input) <
          static_cast<MessagePrivate::MessageHeader*>(msg->_p->getHeader())->size)
        return;

      /* we have to let the Buffer know we are going to push some data inside */
      qi::Buffer buf;
      buf.reserve(static_cast<MessagePrivate::MessageHeader*>(msg->_p->getHeader())->size);
      msg->setBuffer(buf);

      evbuffer_remove(input,
                      buf.data(),
                      buf.size());

      {
        boost::mutex::scoped_lock l(mtx);
        msgSend[msg->id()] = msg;
        msgId = msg->id();
        msg = NULL;
        cond.notify_all();
      }
      if (tcd)
        tcd->onSocketReadyRead(self, msgId);

    }
  }


  void TransportSocketLibEvent::writecb(struct bufferevent *QI_UNUSED(bev),
                                        void               *QI_UNUSED(context))
  {
    if (tcd)
      tcd->onSocketWriteDone(self);
  }

  void TransportSocketLibEvent::eventcb(struct bufferevent *bev,
                                        short events,
                                        void *QI_UNUSED(context))
  {
    if (events & BEV_EVENT_CONNECTED)
    {
      connected = true;
      if (tcd)
        tcd->onSocketConnected(self);
    }
    else if ((events & BEV_EVENT_EOF) || (events & BEV_EVENT_ERROR))
    {
      bufferevent_free(bev);
      bev = 0;
      bool wasco = connected;
      connected = false;
      //for waitForId
      cond.notify_all();

      if (tcd)
        tcd->onSocketConnectionError(self);
      if (wasco && tcd)
        tcd->onSocketDisconnected(self);
      status = errno;
      // check errno to see what error occurred
      qiLogVerbose("qimessaging.TransportSocketLibevent")  << "socket terminate (" << errno << "): " << strerror(errno) << std::endl;
    }
    else if (events & BEV_EVENT_TIMEOUT)
    {
      // must be a timeout event handle, handle it
      qiLogError("qimessaging.TransportSocketLibevent")  << "must be a timeout event handle, handle it" << std::endl;
    }
  }

  TransportSocketLibEvent::TransportSocketLibEvent(TransportSocket *socket)
    : TransportSocketPrivate(socket)
    , bev(NULL)
    , fd(-1)
  {
  }

  TransportSocketLibEvent::TransportSocketLibEvent(TransportSocket *socket, int fileDesc, void *data)
    : TransportSocketPrivate(socket)
    , bev(NULL)
    , fd(fileDesc)
  {
    struct event_base *base = static_cast<event_base *>(data);
    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    bufferevent_setcb(bev, ::qi::readcb, ::qi::writecb, ::qi::eventcb, this);
    bufferevent_setwatermark(bev, EV_WRITE, 0, MAX_LINE);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    connected = true;
  }

  TransportSocketLibEvent::~TransportSocketLibEvent()
  {
    if (isConnected())
      disconnect();
  }

  void TransportSocketLibEvent::onBufferSent(const void *QI_UNUSED(data),
                                             size_t QI_UNUSED(datalen),
                                             void *buffer)
  {
    Buffer *b = static_cast<Buffer *>(buffer);
    delete b;
  }

  void TransportSocketLibEvent::onMessageSent(const void *QI_UNUSED(data),
                                              size_t QI_UNUSED(datalen),
                                              void *msg)
  {
    Message *m = static_cast<Message *>(msg);
    delete m;
  }

  bool TransportSocketLibEvent::connect(qi::Session *session,
                                        const qi::Url &url)
  {
    const std::string &address = url.host();
    struct evutil_addrinfo *ai = NULL;
    int                     err;
    struct evutil_addrinfo  hint;
    char                    portbuf[10];

    qiLogVerbose("qimessaging.transportsocket.connect") << "Trying to connect to " << url.host() << ":" << url.port();
    if (!isConnected())
    {
      bev = bufferevent_socket_new(session->_p->_networkThread->getEventBase(), -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
      bufferevent_setcb(bev, ::qi::readcb, ::qi::writecb, ::qi::eventcb, this);
      bufferevent_setwatermark(bev, EV_WRITE, 0, MAX_LINE);
      bufferevent_enable(bev, EV_READ|EV_WRITE);

      evutil_snprintf(portbuf, sizeof(portbuf), "%d", url.port());
      memset(&hint, 0, sizeof(hint));
      hint.ai_family = AF_UNSPEC;
      hint.ai_protocol = IPPROTO_TCP;
      hint.ai_socktype = SOCK_STREAM;
      err = evutil_getaddrinfo(address.c_str(), portbuf, &hint, &ai);
      if (err != 0)
      {
        qiLogError("qimessaging.TransportSocketLibEvent") << "Cannot resolve dns (" << address << ")";
        return (false);
      }

      int result = bufferevent_socket_connect(bev, ai->ai_addr, ai->ai_addrlen);

      if (result == 0)
        return true;

      return false;
    }
    else
    {
      qiLogError("qimessaging.TransportSocketLibevent") << "socket is already connected.";
    }

    return false;
  }

  void TransportSocketLibEvent::disconnect()
  {
    if (isConnected())
    {
      bufferevent_free(bev);
      bev = NULL;
      connected = false;
    }
    else
    {
      qiLogError("qimessaging.TransportSocketLibevent") << "socket is not connected.";
    }
  }

  bool TransportSocketLibEvent::send(const qi::Message &msg)
  {
    if (!isConnected())
    {
      qiLogError("qimessaging.TransportSocketLibevent") << "socket is not connected.";
      return false;
    }

    qi::Message *m = new qi::Message();
    *m = msg;
    m->_p->complete();
    assert(m->isValid());

    struct evbuffer *evb = bufferevent_get_output(bev);

    // m might be deleted.
    qi::Buffer *b = new qi::Buffer(m->buffer());

    if (evbuffer_add_reference(evb,
                               m->_p->getHeader(),
                               sizeof(qi::MessagePrivate::MessageHeader),
                               qi::TransportSocketLibEvent::onMessageSent,
                               static_cast<void *>(m)) != 0)
    {
      qiLogError("qimessaging.TransportSocketLibevent") << "Add reference fail in header";
      delete m;
      delete b;
      return false;
    }
    size_t sz = b->size();
    if (sz) {
      if (evbuffer_add_reference(evb,
                                 b->data(),
                                 sz,
                                 qi::TransportSocketLibEvent::onBufferSent,
                                 static_cast<void *>(b)) != 0)
      {
        qiLogError("qimessaging.TransportSocketLibevent") << "Add reference fail for block of size " << sz;
        delete b;
        return false;
      }
    }
    else {
      delete b;
    }

    if (bufferevent_write_buffer(bev, evb) != 0)
    {
      qiLogError("qimessaging.TransportSocketLibevent") << "Can't add buffer to the send queue";
      evbuffer_drain(evb, sizeof(qi::MessagePrivate::MessageHeader) + sz);
      return false;
    }

    return true;
  }
}

