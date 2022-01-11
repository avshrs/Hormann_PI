// based on https://github.com/stephan192/hoermann_door

#include "hoermann.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <unistd.h>

#define SYNC_BYTE   0x55
typedef unsigned char BYTE;


void Hoermann_pi::loop()
{
  if (read_rs232() == true)
  {
    parse_input();
  }

  if (actual_action != hoermann_action_none)
  {
    send_command();
    actual_action = hoermann_action_none;
  }
  
}

hoermann_state_t Hoermann_pi::get_state(void)
{
  return actual_state;
}

std::string Hoermann_pi::get_state_string(void)
{
  return actual_state_string;
}

void Hoermann_pi::trigger_action(hoermann_action_t action)
{
  actual_action = action;
}

void Hoermann_pi::open_serial(char * serial_name, int boudrate)
{
  serial.serial_open2(serial_name, boudrate, false, NULL);
  //serial.serial_open(serial_name, boudrate);
}


bool Hoermann_pi::read_rs232(void)
{

  while (1)
  {
    // read the incoming byte:

    serial.serial_read(buf, 1);
    

    if (buf[0] == SYNC_BYTE)
    {
      serial.serial_read(buf, 1);
   
      
      serial.serial_read(buf, 5);
      
      for (int i = 0 ; i < 5 ;i++){
        rx_buffer[i] = buf[i];
      }

      
      if (calc_checksum(rx_buffer, 4) == rx_buffer[4])
        {
          return true;
        }
      }
  }      
  return false;

}
// bool Hoermann_pi::read_rs232(void)
// {

//   while (1)
//   {
//     // read the incoming byte:
//     for(int i=0; i<16 ; i++)
//       buf[i] = 0x00;
//     serial.serial_read(buf, 16);
//     for(int i=0; i<16 ; i++){
//         if(buf[i] ==0)
//         std::cout << " 0x  ";
//         else
//         std::cout << " 0x"<<std::setw(2) << std::setfill('0')<<std::hex << static_cast<int>(buf[i]);
//         } std::cout << std::endl;

//   }      
//   return false;

// }

void Hoermann_pi::parse_input(void)
{
  if (rx_buffer[0] == 0x00)
  {
    if (rx_buffer[1] == 0x12)
    {
      if ((rx_buffer[2] & 0x01) == 0x01)
      {
        actual_state = hoermann_state_open;
        actual_state_string = "open";
      }
      else if ((rx_buffer[2] & 0x02) == 0x02)
      {
        actual_state = hoermann_state_closed;
        actual_state_string = "closed";
      }
      else if ((rx_buffer[2] & 0x80) == 0x80)
      {
        actual_state = hoermann_state_venting;
        actual_state_string = "venting";
      }
      else if ((rx_buffer[2] & 0x60) == 0x40)
      {
        actual_state = hoermann_state_opening;
        actual_state_string = "opening";
      }
      else if ((rx_buffer[2] & 0x60) == 0x60)
      {
        actual_state = hoermann_state_closing;
        actual_state_string = "closing";
      }
      else if ((rx_buffer[2] & 0x10) == 0x10)
      {
        actual_state = hoermann_state_error;
        actual_state_string = "error";
      }
      else
      {
        actual_state = hoermann_state_stopped;
        actual_state_string = "Partially Open";
      }
    }
  }
}
/*
1 1 1 1  1 1 1 1
0 0 0 0  0 0 0 0 x01  open
0 0 0 1  0 0 0 0 x10 error
0 1 0 1  0 0 0 0 x40 opening
0 1 1 0  0 0 0 0 x60 closing
1 0 0 0  0 0 0 0 x80 venting
*/

void Hoermann_pi::send_command()
{
  output_buffer[0] = 0x55;
  output_buffer[1] = 0x01;
  output_buffer[2] = 0x01;
  output_buffer[3] = (uint8_t)actual_action;
  output_buffer[4] = calc_checksum(output_buffer, 4);
  for (int i =0 ; i<16; i++){
      output_buffer_[i] = (char)output_buffer[i];
  }
  serial.serial_send(&output_buffer_[0], 5);
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
    return(crc);
}




// uint8_t gencrc1(uint8_t *bfr, size_t len)
// {
// size_t i;
// uint8_t crc = 0x00;
//     while(len--){
//         crc ^= *bfr++;
//         for(i = 0; i < 8; i++){
//             if(crc & 0x80){
//                 crc <<= 1;
//                 crc ^= 0x07;
//             } else {
//                 crc <<= 1;
//             }
//         }
//     }
//     return(crc);
// }

// uint8_t gencrc2(uint8_t *bfr, size_t len)
// {
// size_t i;
// uint8_t crc = 0xF3;
//     while(len--){
//         crc ^= *bfr++;
//         for(i = 0; i < 8; i++){
//             if(crc & 0x80){
//                 crc <<= 1;
//                 crc ^= 0x9b;
//             } else {
//                 crc <<= 1;
//             }
//         }
//     }
//     return(crc);
// }