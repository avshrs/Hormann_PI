#include "hoermann.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <unistd.h>



void Hoermann_pi::open_serial(char * serial_name, int boudrate)
{
    //serial.serial_open(serial_name, boudrate, false, NULL);
    serial.serial_open(serial_name, boudrate);
}

void Hoermann_pi::send_action(std::string action)
{
  for(int i = 0; i < 7; i++)
  {
    if(action == actions[i])
    {
      uint8_t frame[4] = {master_id, 
                          0x00, 
                          0x14, 
                          device_id};
    }
  }
}


void Hoermann_pi::start_frame_listener()
{   
    while (1)
    {
        serial.serial_read(in_bufer, 6);
        if(in_bufer[0] == device_id && in_bufer[1]==0x02 && in_bufer[3]== master_id)
        {
          std::cout << "Ack Message: ";//<<std::endl;
          for(int i=0; i<5 ; i++){
          std::cout << " 0x"<<std::setw(2) << std::setfill('0')<<std::hex << static_cast<int>(in_bufer[i]);
          }
          std::cout << std::endl;

            //0x28 0x02 0x01 0x80 0x0D - master requet frame
            //0x28 0x82 0x01 0x80 0x0D - master requet frame ? also?
            //------------------------------------------------------
            //0x80 0x12 0x14 0x28 0xA7 - sleve responce frame
            //0x14 - device type?

            uint8_t frame[4] = {master_id, 
                                static_cast<uint8_t>(((sync_seq_number << 4) | seq_sign)), 
                                0x14, 
                                device_id};
            send_command(frame, 4);
            sync_seq_number ++; 
            if(sync_seq_number == 16) 
            {
              sync_seq_number = 1;
            }
        }
        else if (in_bufer[0] == broadcast_id && (in_bufer[1] & seq_mask) == seq_sign && in_bufer[3] == 0x00)
        {
            // std::cout << "broadcast Message"<<std::endl;
            // for(int i=0; i<5 ; i++)
            // {
            // std::cout << " 0x"<<std::setw(2) << std::setfill('0')<<std::hex << static_cast<int>(in_bufer[i]);
            // }
            // std::cout << std::endl;
            //0x00 0x12 0x02 0x00 0x56 Broadcast door close for supramatic e3 / my case
            std::string active_status = parse_state(in_bufer[2]);
            
            

            if (bufferred_state != active_status)
            {
                std::cout<<"active status is: "<< active_status << std::endl;
                // mqtt.publish_state(active_status);  
                bufferred_state = active_status;
            }
        }
    }
}

std::string Hoermann_pi::parse_state(char data)
{
  if ((data & 0x01) == 0x01)
  {
    return states[1];
  }
  else if ((data & 0x02) == 0x02)
  {
    return states[2];
  }
  else if ((data & 0x80) == 0x80)
  {
    return states[3];
  }
  else if ((data & 0x60) == 0x40)
  {
    return  states[4];
  }
  else if ((data & 0x60) == 0x60)
  {
    return  states[5];
  }
  else if ((data & 0x10) == 0x10)
  {
    return states[6];
  }
  else if (data == 0x00)
  {
    return states[0];
  }
  else 
    return states[7];
 
}

void Hoermann_pi::send_command(uint8_t* frame, uint8_t len)
{ 
  uint8_t final_len= len + 1; 
  char *buf = new char[final_len];
  char crc = static_cast<char>(calc_checksum(frame, len));
  for(int i = 0 ; i < final_len; i++)
  {
    buf[i] = static_cast<char>(frame[i]);
    if(i==len)
    {
       buf[i] = crc;
    }
  }
  

  std::cout << "command send: ";
    for(int i=0; i<final_len ; i++){
    std::cout << " 0x"<<std::setw(2) << std::setfill('0')<<std::hex << static_cast<int>(buf[i]);
    }
    std::cout << std::endl;
  
  serial.serial_send(&buf[0], final_len);
  delete[] buf;
}


uint8_t Hoermann_pi::calc_checksum(uint8_t *p_data, uint8_t len)
{
size_t i;
uint8_t crc = 0xF3;
    while(len--){
        crc ^= *p_data++;
        for(i = 0; i < 8; i++){
            if(crc & 0x80){
                crc <<= 1;
                crc ^= 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    //std::cout << " 0x"<<std::setw(2) << std::setfill('0')<<std::hex << static_cast<int>(crc);
    return(crc);
}
