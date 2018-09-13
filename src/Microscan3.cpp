// this is for emacs file handling -*- mode: c++; indent-tabs-mode: nil -*-

// -- BEGIN LICENSE BLOCK ----------------------------------------------

// -- END LICENSE BLOCK ------------------------------------------------

//----------------------------------------------------------------------
/*!\file Microscan3.cpp
*
* \author  Lennart Puck <puck@fzi.de>
* \date    2018-08-21
*
*/
//----------------------------------------------------------------------

#include "sick_microscan3_ros_driver/Microscan3.h"

namespace sick {

Microscan3::Microscan3(PaketReceivedCallbackFunction newPaketReceivedCallbackFunction, sick::datastructure::CommSettings settings)
  : m_newPaketReceivedCallbackFunction(newPaketReceivedCallbackFunction)
{
  std::cout << "starting microscan" << std::endl;
  m_io_service_ptr = boost::make_shared<boost::asio::io_service>();
  m_async_udp_client_ptr = boost::make_shared<sick::communication::AsyncUDPClient>(boost::bind(&Microscan3::processUDPPaket, this, _1),
                                                                               boost::ref(*m_io_service_ptr), settings.getHostUdpPort());
  m_paket_merger_ptr = boost::make_shared<sick::data_processing::UDPPaketMerger>();
  std::cout << "started Microscan" << std::endl;
}

Microscan3::~Microscan3()
{
  m_udp_client_thread_ptr.reset();
}

bool Microscan3::run()
{
  m_udp_client_thread_ptr.reset(new boost::thread(boost::bind(&Microscan3::UDPClientThread, this)));

  m_async_udp_client_ptr->run_service();
}

bool Microscan3::UDPClientThread()
{
   std::cout << "enter thread" << std::endl;
   m_io_work_ptr = boost::make_shared<boost::asio::io_service::work>(boost::ref(*m_io_service_ptr));
   m_io_service_ptr->run();
   std::cout << "exit thread" << std::endl;
}


void Microscan3::processTCPPaket(const sick::datastructure::PacketBuffer& buffer)
{
  //TODO?? Do I expect a return to process? Maybe for different Methdos
  std::cout << "process tcp in microscan" << std::endl;
}

//TODO rename
void Microscan3::serviceTCP(sick::datastructure::CommSettings settings){

   startTCPConnection(settings);

   changeCommSettingsinColaSession(settings);

   stopTCPConnection();
}

void Microscan3::startTCPConnection(sick::datastructure::CommSettings settings)
{
  //TODO parameterize

  boost::shared_ptr<sick::communication::AsyncTCPClient> async_tcp_client
   = boost::make_shared<sick::communication::AsyncTCPClient>(boost::bind(&Microscan3::processTCPPaket, this, _1),
                                                                               boost::ref(*m_io_service_ptr), settings.getSensorIp(),
                                                                               settings.getSensorTcpPort());
   async_tcp_client->do_connect();
   m_session_ptr = boost::make_shared<sick::cola2::Cola2Session>(async_tcp_client);
}

void Microscan3::changeCommSettingsinColaSession(sick::datastructure::CommSettings settings)
{
  m_session_ptr->open();
  sick::cola2::Cola2Session::CommandPtr command_ptr =
          boost::make_shared<sick::cola2::ChangeCommSettingsCommand>(boost::ref(*m_session_ptr), settings);
  m_session_ptr->executeCommand(command_ptr);
  std::cout << "SessionID: " << m_session_ptr->getSessionID() << std::endl;
  m_session_ptr->close();

}

void Microscan3::stopTCPConnection()
{
  m_session_ptr.reset();
}



void Microscan3::processUDPPaket(const sick::datastructure::PacketBuffer& buffer)
{
  if(m_paket_merger_ptr->addUDPPaket(buffer)) {

    sick::datastructure::PacketBuffer deployedBuffer =  m_paket_merger_ptr->getDeployedPacketBuffer();
    std::cout << "buffer to parse: " << deployedBuffer.getLength() << std::endl;

    sick::datastructure::Data data;
    sick::data_processing::ParseData data_parser;
    data_parser.parseUDPSequence(deployedBuffer,data);

    m_newPaketReceivedCallbackFunction(data);
  }
}

} /* namespace */
