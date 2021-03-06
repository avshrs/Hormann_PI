#include <stdint.h>
#include <stdbool.h>
#include "hoermann.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <unistd.h>
#include <algorithm>    // std::fill
#include "Mosquitto.h"
#include <thread>
#include <ctime>   // localtime
#include <stdlib.h>
#include <sstream> // stringstream

std::string Hoermann_pi::date(){
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);      
    std::stringstream ss; 
    ss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S | ") ;
    return ss.str();
}


void Hoermann_pi::init(const char* serial_name, int boudrate, uint8_t lead_zero)
{
    serial.serial_open(serial_name, boudrate, lead_zero);
}


void Hoermann_pi::run_loop(void)
{   
    auto start = timer.now();
    RX_Buffer rx_buf;
    TX_Buffer tx_buf;
    
    while (true)
    { 
        rx_buf.buf.clear();
        tx_buf.buf.clear();
        serial.serial_read(rx_buf);

        start = timer.now();
        if(is_frame_corect(rx_buf))
        {     
            if(is_broadcast(rx_buf))
            {
                if(is_broadcast_lengh_correct(rx_buf))
                    {
                        update_broadcast_status(rx_buf);
                    }
            }

            else if(is_slave_query(rx_buf))
            {   
                if(is_slave_scan(rx_buf))
                {
                    make_scan_responce_msg(rx_buf, tx_buf);
                    while(true)
                    {
                        auto deltaTime = std::chrono::duration_cast<mi>(timer.now() - start).count();
                        if( deltaTime > (tx_buf.timeout) )
                        {   
                            if(deltaTime > max_frame_delay)
                            {
                                std::cout << date() << "Send time excited\n";
                                break;
                            }
                            serial.serial_send(tx_buf);
                            print_buffer(tx_buf.buf.data(),tx_buf.buf.size());

                            std::cout << date()  << "Time delta: "<< std::chrono::duration_cast<mi>(timer.now() - start).count() <<"\n";
                            break;
                            
                        }

                        usleep(10);
                    }                    
                    
                }    
                else if(is_slave_status_req(rx_buf))
                {
                    make_status_req_msg(rx_buf, tx_buf);
                    while(true)
                    {
                        
                        auto deltaTime = std::chrono::duration_cast<mi>(timer.now() - start).count();
                        if( deltaTime > (tx_buf.timeout))
                        {   
                            if(deltaTime > max_frame_delay)
                            {
                                std::cout << date()<< " STATUS RESPONCE Frame building to long "<<deltaTime <<"\n";

                                break;
                            }
                           
                            // print_buffer(rx_buf.buf.data(),rx_buf.buf.size());
                            // print_buffer(tx_buf.buf.data(),tx_buf.buf.size());
                            serial.serial_send(tx_buf);
                            // std::cout   << "-------"<< std::chrono::duration_cast<mi>(timer.now() - start).count() <<"-------\n";
                            
                            
                            break;
                        }

                        usleep(10);
                    }
                }
            }
        }
    } 
}       


uint8_t Hoermann_pi::get_length(RX_Buffer &buf)
{   
    if(buf.buf.size() > 2)
    {
        return buf.buf.at(1) & 0x0F;
    }
    else
        return 0x00;
}

uint8_t Hoermann_pi::get_counter(RX_Buffer &buf)
{
    if(buf.buf.size() > 2)
    {
        return (buf.buf.at(1) & 0xF0) + 0x10;
    }
    else
        return 0x00;

}

bool Hoermann_pi::is_broadcast(RX_Buffer &buf)
{   
    if(buf.buf.size() == 5)
    {
        if(buf.buf.at(0) == BROADCAST_ADDR && calc_crc8(buf.buf.data(), 4) == buf.buf.at(4))
        {   
            // print_buffer(buf.buf.data(), buf.buf.size());
            return true;
        }
        else
            return false;
    }
    else
    {
        return false;
    }
}

bool Hoermann_pi::is_slave_query(RX_Buffer &buf)
{   
    if(buf.buf.size() > 3 && buf.buf.size() < 6 )
    {
        if(buf.buf.at(0) == UAP1_ADDR)
            return true;
        else
            return false;
    }
    else
    {
       return false; 
    }
}
bool Hoermann_pi::is_frame_corect(RX_Buffer &buf)
{   
    if(buf.buf.size() > 3 && buf.buf.size() < 6)
    {
        if(calc_crc8(buf.buf.data(), buf.buf.size()-1) == buf.buf.at(buf.buf.size()-1) )
            return true; 
        else 
            return false;
    }
    else
    {
       return false; 
    }
}

bool Hoermann_pi::is_slave_scan(RX_Buffer &buf)
{
    if(buf.buf.size() == 5)
    {
        if(is_broadcast_lengh_correct(buf) && (buf.buf.at(2) == CMD_SLAVE_SCAN))
            return true;
        else
            return false;
    }
    else
    {
        return false;    
    }
}

bool Hoermann_pi::is_slave_status_req(RX_Buffer &buf)
{
    if(buf.buf.size() == 4)
    {    
    if(is_req_lengh_correct(buf) && (buf.buf.at(2) == CMD_SLAVE_STATUS_REQUEST))
        return true;
    else
        return false;
    }
    else
    {
        return false;
    }
}

bool Hoermann_pi::is_broadcast_lengh_correct(RX_Buffer &buf)
{
    if(get_length(buf) == broadcast_lengh)
        return true;
    else
        return false;
}

bool Hoermann_pi::is_req_lengh_correct(RX_Buffer &buf)
{
    if(get_length(buf) == reguest_lengh)
        return true;
    else
        return false;
}

void Hoermann_pi::update_broadcast_status(RX_Buffer &buf)
{
//   uint16_t br = buf.buf.at(2);
//   br |= (uint16_t)buf.buf.at(3) << 8;
  
  uint8_t br = buf.buf.at(2);
  if (static_cast<uint8_t>(broadcast_status) != br)
  {
    broadcast_status = static_cast<uint16_t>(br);
  
//   if (broadcast_status != br)
//   {
//     broadcast_status = br;
    
    std::thread t(&Hoermann_pi::pub_thread, this);
    t.detach();
  }
}


void Hoermann_pi::pub_thread()
{
    std::string msg = get_state();
    mqtt->pub_door_state(msg);
}

void Hoermann_pi::print_buffer(uint8_t *buf, int len)
{   
    std::cout << date()<< "Len: "<< std::dec <<len << "|";
    for(int i = 0; i < len  ; i++)
        {
        std::cout << " 0x" << std::setw(2);
        std::cout << std::setfill('0') << std::hex;
        std::cout << static_cast<int>(buf[i]);
        }
    std::cout <<" | \n";
}

uint8_t Hoermann_pi::get_master_address()
{
    return master_address;
}

void Hoermann_pi::make_scan_responce_msg(RX_Buffer &rx_buf, TX_Buffer &tx_buf)
{
    tx_buf.buf.push_back(get_master_address());
    tx_buf.buf.push_back(0x02 | get_counter(rx_buf));
    tx_buf.buf.push_back(UAP1_TYPE);
    tx_buf.buf.push_back(UAP1_ADDR);
    tx_buf.buf.push_back(calc_crc8(tx_buf.buf.data(), 4));
    tx_buf.timeout = 2000;
}

void Hoermann_pi::make_status_req_msg(RX_Buffer &rx_buf, TX_Buffer &tx_buf)
{
    tx_buf.buf.push_back(get_master_address());
    tx_buf.buf.push_back(0x03 | get_counter(rx_buf));

    tx_buf.buf.push_back(CMD_SLAVE_STATUS_RESPONSE);
    if(slave_respone_data == RESPONSE_STOP)
    {
        tx_buf.buf.push_back(0x00);
        tx_buf.buf.push_back(0x00);
    }
    else 
    {
        tx_buf.buf.push_back(static_cast<uint8_t>(slave_respone_data));
        tx_buf.buf.push_back(0x10);   
    }
    slave_respone_data = RESPONSE_DEFAULT;
    tx_buf.buf.push_back(calc_crc8(tx_buf.buf.data(), 5));
    tx_buf.timeout = 2000;
}


std::string Hoermann_pi::get_state()
{
  if ((broadcast_status) == 0x00)
  {
    return cfg->get_venting_string();
  }
  else if ((broadcast_status) == 0x02)
  {
    return cfg->get_closed_string();
  }
  else if ((broadcast_status) == 0x01)
  {
    return cfg->get_open_string();
  }
    else
    return cfg->get_error_string();
 
}
// std::string Hoermann_pi::get_state()
// {
//   if ((broadcast_status & 0x01) == 0x01)
//   {
//     return cfg->get_stopped_string();
//   }
//   else if ((broadcast_status & 0x02) == 0x02)
//   {
//     return cfg->get_open_string();
//   }
//   else if ((broadcast_status & 0x80) == 0x80)
//   {
//     return cfg->get_closed_string();
//   }
//   else if ((broadcast_status & 0x60) == 0x40)
//   {
//     return  cfg->get_venting_string();
//   }
//   else if ((broadcast_status & 0x60) == 0x60)
//   {
//     return  cfg->get_opening_string();
//   }
//   else if ((broadcast_status & 0x10) == 0x10)
//   {
//     return cfg->get_closing_string();
//   }
//   else if (broadcast_status == 0x00)
//   {
//     return cfg->get_error_string();
//   }
//   else 
//     return cfg->get_offline_string();
 
// }

void Hoermann_pi::set_state(std::string action)
{
    if(action == cfg->set_stop_string())
    {
      std::cout<< date() <<"Executing RESPONSE_STOP\n";
      slave_respone_data = RESPONSE_STOP;
    }
    else if(action == cfg->set_open_string())
    {
      slave_respone_data = RESPONSE_OPEN;
      std::cout << date()<<"Executing RESPONSE_OPEN\n";
    }
    else if(action == cfg->set_close_string())
    {
      std::cout << date()<<"Executing RESPONSE_CLOSE\n";
      slave_respone_data = RESPONSE_CLOSE;
    }
    else if(action == cfg->set_venting_string())
    {
      std::cout << date()<<"Executing RESPONSE_VENTING\n";
      slave_respone_data = RESPONSE_VENTING;
    }
    else if(action == cfg->toggle_Light_string())
    {
      std::cout<< date() <<"Executing RESPONSE_TOGGLE_LIGHT\n";
      slave_respone_data = RESPONSE_TOGGLE_LIGHT;
    }
    
}



uint8_t Hoermann_pi::calc_crc8(uint8_t *p_data, uint8_t len)
{
size_t i;
uint8_t crc = CRC8_INITIAL_VALUE;
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

void Hoermann_pi::door_open()
{
    set_state(cfg->set_open_string());
}

void Hoermann_pi::door_close()
{
    set_state(cfg->set_close_string());
}

void Hoermann_pi::door_venting()
{
    set_state(cfg->set_venting_string());
}

void Hoermann_pi::door_toggle_light()
{
    set_state(cfg->toggle_Light_string());
}

void Hoermann_pi::door_stop()
{
    set_state( cfg->set_stop_string());
}

void Hoermann_pi::register_mqtt(Mqtt_Client *mqtt_){
    mqtt = mqtt_;
}

void Hoermann_pi::register_cfg(Config_manager *cfg_){
    cfg = cfg_;
}


