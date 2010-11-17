/*
** Author(s):
**  - Chris Kilner <ckilner@aldebaran-robotics.com>
**
** Copyright (C) 2010 Aldebaran Robotics
*/

#include <qi/nodes/detail/master_node_imp.hpp>
#include <qi/functors/makefunctor.hpp>
#include <qi/log.hpp>

namespace qi {
  namespace detail {

    MasterNodeImp::~MasterNodeImp() {}

    MasterNodeImp::MasterNodeImp(const std::string& masterAddress) :
        fName("master"),
        fAddress(masterAddress),
        fServerNode(fName, masterAddress, masterAddress) {
      xInit();
    }

    void MasterNodeImp::xInit() {
      fServerNode.addService("master.registerService", this, &MasterNodeImp::registerService);
      registerService(fAddress, "master.registerService::v:ss");
      fServerNode.addService("master.locateService", this, &MasterNodeImp::locateService);
      registerService(fAddress, "master.locateService::s:s");
      fServerNode.addService("master.listServices", this, &MasterNodeImp::listServices);
      registerService(fAddress, "master.listServices::{ss}:");

      fServerNode.addService("master.registerServerNode", this, &MasterNodeImp::registerServerNode);
      registerService(fAddress, "master.registerServerNode::v:ss");
      fServerNode.addService("master.unregisterServerNode", this, &MasterNodeImp::unregisterServerNode);
      registerService(fAddress, "master.unregisterServerNode::v:ss");

      fServerNode.addService("master.registerClientNode", this, &MasterNodeImp::registerClientNode);
      registerService(fAddress, "master.registerClientNode::v:ss");
      fServerNode.addService("master.unregisterClientNode", this, &MasterNodeImp::unregisterClientNode);
      registerService(fAddress, "master.unregisterClientNode::v:ss");
    }

    void MasterNodeImp::registerService(
      const std::string& nodeAddress, const std::string& methodSignature) {
      qisInfo << "Master::registerService " << nodeAddress << " " << methodSignature << std::endl;
      fServiceCache.insert(methodSignature, nodeAddress);
    }

    void MasterNodeImp::registerServerNode(const std::string& nodeName, const std::string& nodeAddress) {
      qisInfo << "Master::registerServerNode " << nodeName << " " << nodeAddress << std::endl;
      fServerNodeCache.insert(nodeName, nodeAddress);
    }

    void MasterNodeImp::unregisterServerNode(const std::string& nodeName, const std::string& nodeAddress) {
      qisInfo << "Master::unregisterServerNode " << nodeName << " " << nodeAddress << std::endl;
      // todo remove associated services
      fServerNodeCache.remove(nodeName);
    }

    void MasterNodeImp::registerClientNode(const std::string& nodeName, const std::string& nodeAddress) {
      qisInfo << "Master::registerClientNode " << nodeName << " " << nodeAddress << std::endl;
      fClientNodeCache.insert(nodeName, nodeAddress);
    }

    void MasterNodeImp::unregisterClientNode(const std::string& nodeName, const std::string& nodeAddress) {
      qisInfo << "Master::unregisterClientNode " << nodeName << " " << nodeAddress << std::endl;
      fClientNodeCache.remove(nodeName);
    }

    //TODO: add a ref to the return value
    const std::string MasterNodeImp::locateService(const std::string& methodSignature) {
      // If not found, an empty string is returned
      // TODO friendly error describing closest service?
      return fServiceCache.get(methodSignature);
    }

    const std::map<std::string, std::string>& MasterNodeImp::listServices() {
      return fServiceCache.getMap();
    }

  }
}
