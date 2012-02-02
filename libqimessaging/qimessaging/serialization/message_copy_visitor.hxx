/*
*  Author(s):
*  - Cedric Gestes <gestes@aldebaran-robotics.com>
*  - Chris  Kilner <ckilner@aldebaran-robotics.com>
*
*  Copyright (C) 2010 Aldebaran Robotics
*/

namespace qi {
  namespace serialization {



    template <typename MessageSrc, typename MessageDest>
    MessageCopyVisitor<MessageSrc, MessageDest>::MessageCopyVisitor(MessageSrc &msgSrc, MessageDest &msgDest, const char *signature)
      : _signature(signature),
        _msgSrc(msgSrc),
        _msgDest(msgDest)
    {

    }

    template <typename MessageSrc, typename MessageDest>
    void MessageCopyVisitor<MessageSrc, MessageDest>::visit()
    {
      std::cout << "visit" << std::endl;

      qi::Signature::iterator it;

      for(it = _signature.begin(); it != _signature.end(); ++it)
      {
        visitElement(it);
      }
    }

    template <typename MessageSrc, typename MessageDest>
    void MessageCopyVisitor<MessageSrc, MessageDest>::visitElement(qi::Signature::iterator &elt)
    {
      switch (elt.type()) {
      case qi::Signature::List:
        onList(elt.raw_child_1);
        break;
      case qi::Signature::Map:
        onMap(elt.raw_child_1, elt.raw_child_2);
        break;
      case qi::Signature::Protobuf:
        onProtobuf(elt.raw_signature);
        break;
      case 0:
        break;
      default:
        onSimple(elt.raw_signature);
        break;
      }
    }

    template <typename MessageSrc, typename MessageDest>
    void MessageCopyVisitor<MessageSrc, MessageDest>::onSimple(const char *simpleType)
    {
      switch(*simpleType) {
      case qi::Signature::Bool:
        bool b;
        _msgSrc >> b;
        _msgDest << b;
        break;
      case qi::Signature::Char:
        char c;
        _msgSrc >> c;
        _msgDest << c;
        break;
      case qi::Signature::Int:
        int i;
        _msgSrc >> i;
        _msgDest << i;
        break;
      case qi::Signature::Float:
        float f;
        _msgSrc >> f;
        _msgDest << f;
        break;
      case qi::Signature::Double:
        double d;
        _msgSrc >> d;
        _msgDest << d;
        break;
      case qi::Signature::String:
        std::string s;
        _msgSrc >> s;
        _msgDest << s;
        break;

      }
    }

    template <typename MessageSrc, typename MessageDest>
    void MessageCopyVisitor<MessageSrc, MessageDest>::onList(const char *elementType)
    {
      int count, i;
      _msgSrc >> count;
      _msgDest << count;
      qi::Signature           sig(elementType);
      qi::Signature::iterator it;
      it = sig.begin();

      for (i = 0; i < count; ++i)
      {
        visitElement(it);
      }
    }

    template <typename MessageSrc, typename MessageDest>
    void MessageCopyVisitor<MessageSrc, MessageDest>::onMap(const char *keyType, const char *valueType)
    {
      int count, i;
      _msgSrc >> count;
      _msgDest << count;
      qi::Signature           sigk(keyType);
      qi::Signature::iterator itk = sigk.begin();
      qi::Signature           sigv(valueType);
      qi::Signature::iterator itv = sigv.begin();

      for (i = 0; i < count; ++i)
      {
        visitElement(itk);
        visitElement(itv);
      }
    }

    template <typename MessageSrc, typename MessageDest>
    void MessageCopyVisitor<MessageSrc, MessageDest>::onProtobuf(const char *signature)
    {
      std::cout << "protobuf" << std::endl;
    }

  }
}
