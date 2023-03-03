#include <stdio.h>
#include "boards.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "nrf_drv_twi.h"



#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "EEPROM.h"

//Initializing TWI0 instance
#define TWI_INSTANCE_ID     0

// A flag to indicate the transfer state
static volatile bool m_xfer_done = false;

// Create a Handle for the twi communication
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

//========Event Handler==========//
void twi_handler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
    //Check the event to see what type of event occurred
    switch (p_event->type)
    {
        //If data transmission or receiving is finished
	case NRF_DRV_TWI_EVT_DONE:
        m_xfer_done = true;//Set the flag
        break;
        
        default:
        // do nothing
          break;
    }
}



//=========Initialize the TWI as Master device==========//
void twi_master_init(void)
{
    ret_code_t err_code;

    // Configure the settings for twi communication
    const nrf_drv_twi_config_t twi_config = {
       .scl                = 22,  //SCL Pin
       .sda                = 23,  //SDA Pin
       .frequency          = NRF_DRV_TWI_FREQ_400K, //Communication Speed
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH, //Interrupt Priority(Note: if using Bluetooth then select priority carefully)
       .clear_bus_init     = false //automatically clear bus
    };


    //A function to initialize the twi communication
    err_code = nrf_drv_twi_init(&m_twi, &twi_config, twi_handler, NULL);
    APP_ERROR_CHECK(err_code);
    
    //Enable the TWI Communication
    nrf_drv_twi_enable(&m_twi);
}


/**
 * @brief Function for initializing the TWI driver instance.
 *
 * @param[in] p_instance      Pointer to the driver instance structure.
 * @param[in] p_config        Initial configuration.
 * @param[in] event_handler   Event handler provided by the user. If NULL, blocking mode is enabled.
 * @param[in] p_context       Context passed to event handler.

//========= A function to write a single byte into a predefined memory address in the EEPROM =========//
bool EEPROM_Byte_Write( uint16_t write_address, uint8_t data_value)
{
/*  1. start the process
    2. send the device address(7bit) + R_W_bit(= 0)
    3. wait for acknowledge(logic 0)
    4. 1st data address(8bit)
    5. wait for acknowledge
    6. 2nd data address
    7. wait for acknowledge
    8. send the data byte
    9. wait for acknowledge
    10. stop the process*/

    ret_code_t err_code;
    uint8_t tx_buf[10];

    //Write the register address and data into transmit buffer
    tx_buf[0] = (uint8_t)((write_address) & (0x00FF));
    tx_buf[1] = (uint8_t)((write_address) & (0xFF00));
    tx_buf[2] = (uint8_t)data_value;

    //Set the flag to false to show the transmission is not yet completed
    m_xfer_done = false;
    
    //Transmit the data over TWI Bus
    /* @param[in] no_stop    If set, the stop condition is not generated on the bus
                             after the transfer has completed successfully (allowing
                             for a repeated start in the next transfer). the final parameter of the _tx function
                             keeps the tansmission process repeating without generating the stop condition in the SDA line.*/
 
    //the '3' before the last parameter is the number of bytes to be sent
    //was used like this without repeating the same thing for all three bytes in examples also
    err_code = nrf_drv_twi_tx(&m_twi, EEPROM_DEVICE_ADDRS, tx_buf, 3, false);
    
    //Wait until the transmission of the data is finished
    while (m_xfer_done == false)
    {
      }

    // if there is no error then return true else return false
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }
    
    return true;


}

//========= A function to write a multiple bytes into a predefined memory address in the EEPROM =========//
bool EEPROM_Page_Write( uint16_t write_address, uint8_t data_values[], uint8_t number_of_bytes)
{
    ret_code_t err_code;
    uint8_t byte_count = number_of_bytes; //might cause an error 
    uint8_t tx_buff[byte_count +2];

    //Write the register address and data into transmit buffer
    tx_buff[0] = (uint8_t)((write_address) & (0x00FF));
    tx_buff[1] = (uint8_t)((write_address) & (0xFF00));

    for( uint8_t i=2 ; i <= (byte_count+2) ; i++)
    {
      *(tx_buff+i) = data_values[i - 2];

    }

    //Set the flag to false to show the receiving is not yet completed
    m_xfer_done = false;

    err_code = nrf_drv_twi_tx(&m_twi, EEPROM_DEVICE_ADDRS, tx_buff, number_of_bytes, false);
    
    //Wait until the transmission of the data is finished
    while (m_xfer_done == false)
    {
      }

    // if there is no error then return true else return false
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }
    
    return true;

}

//========= A function to read a single byte from a predefined memory address in the EEPROM =========//
bool EEPROM_Byte_Read_random(uint16_t read_address, uint8_t * byte_recieved)
{
/*  1. start the process
    2. send the device address(7bit) + R_W_bit(= 1)
    3. wait for acknowledge(logic 0)
    4. reciev the data byte
    5. If the acknowledge is high stop the process*/

    ret_code_t err_code;
    uint8_t tx_buf[10];

    //Write the register address and data into transmit buffer
    tx_buf[0] = (uint8_t)((read_address) & (0x00FF));
    tx_buf[1] = (uint8_t)((read_address) & (0xFF00));

    //Set the flag to false to show the receiving is not yet completed
    m_xfer_done = false;

    // Send the Register address where we want to write the data
    err_code = nrf_drv_twi_tx(&m_twi, EEPROM_DEVICE_ADDRS, tx_buf, 2, true);
    //err_code = nrf_drv_twi_tx(&m_twi, EEPROM_DEVICE_ADDRS, tx_buf[1], 1, true);

    //Wait for the transmission to get completed
    while (m_xfer_done == false){}
    
    // If transmission was not successful, exit the function with false as return value
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }

    //set the flag again so that we can read data from the MPU6050's internal register
    m_xfer_done = false;
	  
    // Receive the data from the MPU6050
    //the last parameter is the number of bytes to be recieved
    err_code = nrf_drv_twi_rx(&m_twi, EEPROM_DEVICE_ADDRS, byte_recieved, 1);

    //wait until the transmission is completed
    while (m_xfer_done == false){}
	
    // if data was successfully read, return true else return false
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }
    
    return true;

}

bool EEPROM_page_Read_random(uint16_t read_address, uint8_t * byte_recieved , uint8_t bytes_req)
{

    ret_code_t err_code;
    uint8_t tx_buf[10];

    //Write the register address and data into transmit buffer
    tx_buf[0] = (uint8_t)((read_address) & (0x00FF));
    tx_buf[1] = (uint8_t)((read_address) & (0xFF00));

    //Set the flag to false to show the receiving is not yet completed
    m_xfer_done = false;

    // Send the Register address where we want to write the data
    err_code = nrf_drv_twi_tx(&m_twi, EEPROM_DEVICE_ADDRS, tx_buf, 2, true);
    //err_code = nrf_drv_twi_tx(&m_twi, EEPROM_DEVICE_ADDRS, tx_buf[1], 1, true);

    //Wait for the transmission to get completed
    while (m_xfer_done == false){}
    
    // If transmission was not successful, exit the function with false as return value
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }

    //set the flag again so that we can read data from the MPU6050's internal register
    m_xfer_done = false;
	  
    // Receive the data from the MPU6050
    //the last parameter is the number of bytes to be recieved
    err_code = nrf_drv_twi_rx(&m_twi, EEPROM_DEVICE_ADDRS, byte_recieved, bytes_req);

    //wait until the transmission is completed
    while (m_xfer_done == false){}
	
    // if data was successfully read, return true else return false
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }
    
    return true;

}

bool EEPROM_page_Read_current(uint8_t * byte_recieved , uint8_t bytes_req)
{

    ret_code_t err_code;
    uint8_t tx_buf[10];

    //Set the flag to false to show the receiving is not yet completed
    m_xfer_done = false;

    //Wait for the transmission to get completed
    while (m_xfer_done == false){}
    
    // If transmission was not successful, exit the function with false as return value
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }

    //set the flag again so that we can read data from the MPU6050's internal register
    m_xfer_done = false;
	  
    // Receive the data from the MPU6050
    //the last parameter is the number of bytes to be recieved
    err_code = nrf_drv_twi_rx(&m_twi, EEPROM_DEVICE_ADDRS, byte_recieved, bytes_req);

    //wait until the transmission is completed
    while (m_xfer_done == false){}
	
    // if data was successfully read, return true else return false
    if (NRF_SUCCESS != err_code)
    {
        return false;
    }
    
    return true;

}
