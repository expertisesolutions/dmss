
#include <boost/asio.hpp>
#include <iostream>

// Packet
// 32 bits - cmd
// 32 bits - length?
// 32 bits - string?
// 5x32 bits
// 32 bytes de header
// body - length is same as length above

// enum {
//   first     = 0x580000b4
//   , second  = 0x000000a4
//   , third   = 0x000000a3
//   , fourth  = 0x00000083
//   , fifth   = 0x000000f4
//   , sixth   = 0x000000f6 // json
//   , seventh = 0x580000f6 // config answer
//   , eighth  = 0x01000011
//   , nineth  = 0x000000bc // stream DHAV
//   , login   = 0x600005a0 //
//   , realm   = 0x580000b0
// }

template <typename S>
void discard_until_nop(S& socket)
{
  namespace asio = boost::asio;
  while(true)
  {
    std::vector<char> buffer(32);
    int size = socket.receive(asio::mutable_buffers_1(&buffer[0], buffer.size()));
    assert(size == 32);

    uint8_t command = (uint8_t)buffer[0];
    int body_size = *(uint32_t*)&buffer[4];
    buffer.resize(body_size);

    size = socket.receive(asio::mutable_buffers_1(&buffer[0], body_size));
    assert(body_size == size);
    std::cout << "received reply with cmd " << std::hex << (unsigned int)command << std::dec << " received body size " << size << std::endl;
    if(command == 0xb1 /* nop */)
      break;
  }
}

template <typename S>
void get_reply(S& socket)
{
  namespace asio = boost::asio;
  std::vector<char> buffer(32);
  std::size_t size = socket.receive(asio::mutable_buffers_1(&buffer[0], 32));
  assert(size == 32);
  uint32_t body_size = *(uint32_t*)&buffer[4];
  int command = (uint8_t)buffer[0];

  std::cout << "body size is " << body_size << std::endl;
  buffer.resize(body_size);
  size = socket.receive(asio::mutable_buffers_1(&buffer[0], body_size));
  std::cout << "command " << std::hex << command << std::dec << " sent and received reply with cmd " << std::hex << (unsigned int)(uint8_t)command << std::dec << " received body size " << size << std::endl;
}

template <bool GetReply = true, typename S>
void send_dummy_command(S& socket, uint8_t command, std::vector<char> buffer = {})
{
  namespace asio = boost::asio;
  if(buffer.size() < 32) buffer.resize(32);
  buffer[0]  = (uint8_t)command;
  socket.send(asio::const_buffers_1(&buffer[0], buffer.size()));

  if(GetReply)
  {
    get_reply(socket);
  }
}

template <typename S>
void send_dummy_get_system_info_command(S& socket, uint8_t infotype, uint8_t bitstream
                                        , std::vector<char> buffer = {})
{
  if(buffer.size() < 32) buffer.resize(32);
  buffer[8]  = infotype;
  buffer[12] = bitstream;
  send_dummy_command(socket, 0xa4, buffer);
}

template <typename S>
void send_dummy_get_media_cap_command(S& socket, uint8_t infotype
                                      , std::vector<char> buffer = {})
{
  if(buffer.size() < 32) buffer.resize(32);
  buffer[9]  = infotype;
  send_dummy_command(socket, 0x83, buffer);
}

template <typename S>
void send_dummy_get_config_param(S& socket, uint8_t infotype
                                 , std::vector<char> buffer = {})
{
  if(buffer.size() < 32) buffer.resize(32);
  std::string config("config");
  std::copy(config.begin(), config.end(), &buffer[8]);
  buffer[16] = infotype;
  send_dummy_command(socket, 0xa3, buffer);
}

template <typename S>
void send_set_alarm(S& socket, uint8_t alarm_type
                    , std::vector<char> buffer = {})
{
  if(buffer.size() < 32) buffer.resize(32);
  buffer[8] = 2;
  buffer[12] = alarm_type;
  send_dummy_command(socket, 0x68, buffer);
}

template <typename S>
void send_dummy_extension_string(S& socket, std::string body_string
                                         , std::vector<char> buffer = {})
{
  if(buffer.size() < 32) buffer.resize(32 + body_string.size());
  *(uint32_t*)&buffer[4]  = body_string.size();
  std::copy(body_string.begin(), body_string.end(), &buffer[32]);
  send_dummy_command(socket, 0xf4, buffer);
}

int main()
{
  namespace asio = boost::asio;

  asio::io_service io_service;
  
  asio::ip::tcp::socket socket(io_service);

  asio::ip::tcp::socket::endpoint_type endpoint{asio::ip::address::from_string("192.168.95.6"), 37777};
  socket.connect(endpoint);

  std::vector<char> buffer;
  if(0)
  {
    std::string user_pass("admin&&admin");
    buffer.resize(32 + user_pass.size() + 1);
    buffer[0]  = 0xa0;
    buffer[3]  = 0x60;
    buffer[24] = 0x04;
    buffer[25] = 0x01;
    buffer[30] = 0xa1;
    buffer[31] = 0xaa;
    std::copy(user_pass.begin(), user_pass.end(), &buffer[32]);
    *(uint32_t*)&buffer[4] = user_pass.size();
  }
  else
  {
    std::string up = "admin";
    buffer.resize(32);
    buffer[0]  = 0xa0;
    buffer[3]  = 0x60;
    buffer[24] = 0x04;
    buffer[25] = 0x01;
    buffer[30] = 0xa1;
    buffer[31] = 0xaa;
    std::copy(up.begin(), up.end(), &buffer[8]);
    std::copy(up.begin(), up.end(), &buffer[16]);
  }
  socket.send(asio::const_buffers_1(&buffer[0], buffer.size()));

  std::vector<char> recv_buffer(1024);
  int size = socket.receive(asio::mutable_buffers_1(&recv_buffer[0], 1024));
  
  std::cout << "received " << size << " cmd " << std::hex << (unsigned int)(unsigned char)recv_buffer[0] << std::dec << std::endl;

  switch(recv_buffer[28])
    {
    case 0:
      std::cout << "video PAL" << std::endl;
      break;
    case 1:
      std::cout << "video NTSC" << std::endl;
      break;
    default:
      std::cout << "video unknown" << std::endl;
    }
  switch(recv_buffer[11])
    {
    case 8:
      std::cout << "video MPEG4" << std::endl;
      break;
    case 9:
      std::cout << "video H264" << std::endl;
      break;
    default:
      std::cout << "video encoder unknown" << std::endl;
    }

  int i = *static_cast<std::uint32_t*>(static_cast<void*>(&recv_buffer[0] + 4u));
  std::cout << "body size " << i << std::endl;

  std::size_t login_symbol = *(uint32_t*)&recv_buffer[16];

  // send_dummy_get_system_info_command(socket, 1, 0);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_dummy_get_system_info_command(socket, 7, 0);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_dummy_get_system_info_command(socket, 2, 0);

  // send_dummy_get_media_cap_command(socket, 0);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_dummy_get_media_cap_command(socket, 1);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);

  // send_dummy_get_config_param(socket, 0x10);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);

  // send_dummy_get_system_info_command(socket, 0x20, 0);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_dummy_get_system_info_command(socket, 0x04, 0xff);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);

  // send_dummy_extension_string(socket, "TransactionID:1\r\n"
  //                                     "Method:GetParameterNames\r\n"
  //                                     "ParameterName:Dahua.Device.VideoOut.General\r\n"
  //                                     "\r\n");
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);

  // send_dummy_get_system_info_command(socket, 0x0a, 0);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_dummy_get_system_info_command(socket, 2, 0);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_dummy_get_system_info_command(socket, 8, 0);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);

  // send_set_alarm(socket, 0x01);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x02);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x03);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x05);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x06);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x07);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x08);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x09);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x0a);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x0b);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x0c);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x0d);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x0e);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x0f);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x10);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x12);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x13);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x14);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0x9c);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0xa1);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);
  // send_set_alarm(socket, 0xa2);
  // send_dummy_command<false>(socket, 0xa1); // NOP
  // discard_until_nop(socket);

  // for(int i = 0; i != 40; ++i)
  // {
  //   send_dummy_get_system_info_command(socket, i, 0);
  //   send_dummy_command<false>(socket, 0xa1); // NOP
  //   discard_until_nop(socket);
  // }

  send_dummy_command<false>(socket, 0xa1); // NOP
  discard_until_nop(socket);

  asio::ip::tcp::socket stream_socket(io_service);
  stream_socket.connect(endpoint);

  {
    std::vector<char> buffer(32);
    *(uint32_t*)&buffer[8] = login_symbol;
    buffer[12] = 1;
    buffer[13] = 1;
    send_dummy_command(stream_socket, 0xf1, buffer);

    // send_dummy_command<false>(socket, 0xa1); // NOP
    // discard_until_nop(socket);
  }

  // get_reply(socket);

  // {
  //   std::vector<char> buffer(6000);
  //   socket.receive(asio::mutable_buffers_1(&buffer[0], buffer.size()));
  //   std::cout << " sent and received reply with size " << size << " cmd " << std::hex << (unsigned int)(uint8_t)buffer[0] << std::hex << std::endl;
  // }
  
  buffer.resize(32 + 16);
  std::fill(buffer.begin(), buffer.end(), 0);
  buffer[0] = 0x11;
  // buffer[3] = 1;
  buffer[4] = 0x10;
  buffer[8] = 1;
  buffer[32 + 0] = 0;
  send_dummy_command<false>(socket, 0x11, buffer);
  // socket.send(asio::const_buffers_1(&buffer[0], buffer.size()));
  std::cout << "sent 0x11" << std::endl;

  get_reply(stream_socket);
  // {
  //   std::vector<char> buffer(1024);
  //   socket.receive(asio::mutable_buffers_1(&buffer[0], 1024));
  //   std::cout << " sent and received reply with cmd " << std::hex << (unsigned int)(uint8_t)buffer[0] << std::hex << std::endl;
  // }
  
  std::vector<char> recv_ctrl_buffer(1600);
  std::vector<char> recv_data_buffer(1600);

  std::function<void(boost::system::error_code const&, std::size_t)> recv_ctrl = [&] (auto& ec, auto size)
    {
      if(ec)
        {
          std::cout << "error reading " << ec << std::endl;
        }
      else
        {
          std::cout << "received in control socket size " << size << " cmd " << std::hex << (unsigned int)(unsigned char)recv_ctrl_buffer[0] << std::dec << std::endl;
          socket.async_receive(asio::mutable_buffers_1(&recv_ctrl_buffer[0], recv_ctrl_buffer.size()), recv_ctrl);
        }
    };
  std::function<void(boost::system::error_code const&, std::size_t)> recv_data = [&] (auto& ec, auto size)
    {
      if(ec)
        {
          std::cout << "error reading from stream " << ec << std::endl;
        }
      else
        {
          std::cout << "received in stream socket size " << size << " cmd " << std::hex << (unsigned int)(unsigned char)recv_ctrl_buffer[0] << std::dec << std::endl;
          stream_socket.async_receive(asio::mutable_buffers_1(&recv_data_buffer[0], recv_data_buffer.size()), recv_data);
        }
    };

  socket.async_receive(asio::mutable_buffers_1(&recv_ctrl_buffer[0], recv_ctrl_buffer.size()), recv_ctrl);
  stream_socket.async_receive(asio::mutable_buffers_1(&recv_data_buffer[0], recv_data_buffer.size()), recv_data);
  std::cout  << "waiting for data" << std::endl;

  io_service.run();
}

