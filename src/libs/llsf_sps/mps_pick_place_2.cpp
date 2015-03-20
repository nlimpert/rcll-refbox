/*!
* \file mps_pick_place_2.cpp
* \brief Implementation of communication between Refbox and Pick&Place 1
* \author David Masternak
* \version 1.0
*/

#include "mps_pick_place_2.h"
#include "mps_pick_place_2_message.h"
#include <iostream>

/*!
* \fn MPSPickPlace1(const char* ip, int port)
* \brief Constructor
* \param ip address of mps
* \param port port of modbus communication
*/
MPSPickPlace2::MPSPickPlace2(const char* ip, int port) : MPS(ip, port) {}

/*!
 * \fn ~MPSPickPlace2()
 * \brief Destructor
 */
MPSPickPlace2::~MPSPickPlace2() {}

/*!
* \fn produceRing(int workpiece)
* \brief send produce message over Modbus
* \param workpiece what kind of workpiece have to be produce
*/
void MPSPickPlace2::produceRing(int workpiece) {
  uint16_t send[1] = {(uint16_t)workpiece};
  int rc = modbus_write_registers(this->mb, 1, 1, send);
  
  if(rc == -1) {
    std::cout << "ERROR while sending data" << std::endl;
  }
}

/*!
* \fn ringReady()
* \brief receive ringReady command
* \return true if ring is ready and false if ring not is ready
*/
bool MPSPickPlace2::ringReady() {
  uint16_t rec[1];
  int rc = modbus_read_input_registers(this->mb, 2, 1, rec);

  if(rc == -1) {
    std::cout << "ERROR while sending data" << std::endl;    
  }
}

/*!
* \fn isEmptyLine()
* \brief receive isEmptyLine1 command
* \return true if line 1 is empty
*/
bool MPSPickPlace2::isEmpty() {
  uint16_t rec[1];
  int rc = modbus_read_input_registers(this->mb, 3, 1, rec);

  if(rc == -1) {
    std::cout << "ERROR while sending data" << std::endl;    
  }
}

/*!
 * \fn processQueue()
 * \brief processing the queue
 */
void MPSPickPlace2::processQueue() {
  if(!this->messages.empty() && !this->lock) {
    this->lock = true;
    MPSPickPlace2ProduceRingMessage* tmp = (MPSPickPlace2ProduceRingMessage*)messages.front();
    messages.pop();

    this->produceRing(tmp->getRing());
  }
  else if(this->ringReady()) {
    this->lock = false;
  }
}
