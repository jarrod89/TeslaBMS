#include "config.h"
#include "BMSModule.h"
#include "BMSUtil.h"
#include "Logger.h"


BMSModule::BMSModule()
{
    for (int i = 0; i < 6; i++)
    {
        cellVolt[i] = 0.0f;
        lowestCellVolt[i] = 5.0f;
        highestCellVolt[i] = 0.0f;
    }
    moduleVolt = 0.0f;
    temperatures[0] = 0.0f;
    temperatures[1] = 0.0f;
    lowestTemperature = 200.0f;
    highestTemperature = -100.0f;
    lowestModuleVolt = 200.0f;
    highestModuleVolt = 0.0f;
    exists = false;
    moduleAddress = 0;
}

/*
Reading the status of the board to identify any flags, will be more useful when implementing a sleep cycle
*/
void BMSModule::readStatus()
{
  uint8_t payload[3];
  uint8_t buff[8];
  payload[0] = moduleAddress << 1; //adresss
  payload[1] = REG_ALERT_STATUS;//Alert Status start
  payload[2] = 0x02;//two registers
  BMSUtil::sendData(payload, 3, false);
  delay(2);
  BMSUtil::getReply(buff, 8);
}

/*
Reading the setpoints, after a reset the default tesla setpoints are loaded
Default response : 0x10, 0x80, 0x31, 0x81, 0x08, 0x81, 0x66, 0xff
*/
/*
void BMSModule::readSetpoint()
{
  uint8_t payload[3];
  uint8_t buff[12];
  payload[0] = moduleAddress << 1; //adresss
  payload[1] = 0x40;//Alert Status start
  payload[2] = 0x08;//two registers
  sendData(payload, 3, false);
  delay(2);
  getReply(buff);

  OVolt = 2.0+ (0.05* buff[5]);
  UVolt = 0.7 + (0.1* buff[7]);
  Tset = 35 + (5 * (buff[9] >> 4));
} */

bool BMSModule::readModuleValues()
{
    uint8_t payload[4];
    uint8_t buff[50];
    bool retVal = false;
    float tempCalc;
    float tempTemp;
    
    payload[0] = moduleAddress << 1;
    payload[1] = REG_ADC_CTRL;
    payload[2] = 0b00111101; //ADC Auto mode, read every ADC input we can (Both Temps, Pack, 6 cells)
    BMSUtil::sendData(payload, 3, true);
    delay(2);
    BMSUtil::getReply(buff, 50);     //TODO: we're not validating the reply here. Perhaps check to see if a valid reply came back
 
    payload[1] = REG_IO_CTRL;
    payload[2] = 0b00000011; //enable temperature measurement VSS pins
    BMSUtil::sendData(payload, 3, true);
    delay(2);        
    BMSUtil::getReply(buff, 50);    //TODO: we're not validating the reply here. Perhaps check to see if a valid reply came back
            
    payload[1] = REG_ADC_CONV; //start all ADC conversions
    payload[2] = 1;
    BMSUtil::sendData(payload, 3, true);
    delay(2);        
    BMSUtil::getReply(buff, 50);    //TODO: we're not validating the reply here. Perhaps check to see if a valid reply came back
            
    payload[1] = REG_GPAI; //start reading registers at the module voltage registers
    payload[2] = 0x12; //read 18 bytes (Each value takes 2 - ModuleV, CellV1-6, Temp1, Temp2)
    BMSUtil::sendData(payload, 3, false);
    delay(2);
    if (BMSUtil::getReply(buff, 50) > 0x14)
    {
        if (buff[0] == (moduleAddress << 1) && buff[1] == REG_GPAI && buff[2] == 0x12)
        {
            //payload is 2 bytes gpai, 2 bytes for each of 6 cell voltages, 2 bytes for each of two temperatures (18 bytes of data)
            moduleVolt = (buff[3] * 256 + buff[4]) * 0.002034609f;
            if (moduleVolt > highestModuleVolt) highestModuleVolt = moduleVolt;
            if (moduleVolt < lowestModuleVolt) lowestModuleVolt = moduleVolt;            
            for (int i = 0; i < 6; i++) 
            {
                cellVolt[i] = (buff[5 + (i * 2)] * 256 + buff[6 + (i * 2)]) * 0.000381493f;
                if (lowestCellVolt[i] > cellVolt[i]) lowestCellVolt[i] = cellVolt[i];
                if (highestCellVolt[i] < cellVolt[i]) highestCellVolt[i] = cellVolt[i];
            }
            
            //Logger::debug("Raw temp0: %i", buff[17] * 256 + buff[18]);
            tempTemp = (1.78f / ((buff[17] * 256 + buff[18] + 2) / 33046.0f) - 3.56f);
            tempCalc = 77.16013459f - (tempTemp * 7.284328039f);
            tempCalc += 0.239392948f * (tempTemp * tempTemp);
            tempCalc -= .00271619635 * (tempTemp * tempTemp * tempTemp);            
            
            temperatures[0] = tempCalc;            
            
            //Logger::debug("Raw temp1: %i", buff[19] * 256 + buff[20]);
            tempTemp = 1.78f / ((buff[19] * 256 + buff[20] + 9) / 33068.0f) - 3.56f;
            tempCalc = 77.16013459f - (tempTemp * 7.284328039f);
            tempCalc += 0.239392948f * (tempTemp * tempTemp);
            tempCalc -= .00271619635 * (tempTemp * tempTemp * tempTemp);            
            temperatures[1] = tempCalc;
            
            if (getLowTemp() < lowestTemperature) lowestTemperature = getLowTemp();
            if (getHighTemp() > highestTemperature) highestTemperature = getHighTemp();

            Logger::debug("Got voltage and temperature readings");
            retVal = true;
        }        
    }            
    
    //payload[1] = REG_IO_CTRL;
    //payload[2] = 0b00000000; //turn off temperature measurement pins
    //BMSUtil::sendData(payload, 3, true);
    //delay(2);        
    //BMSUtil::getReply(buff, 50);    //TODO: we're not validating the reply here. Perhaps check to see if a valid reply came back    
    
    return retVal;
}

float BMSModule::getCellVoltage(int cell)
{
    if (cell < 0 || cell > 5) return 0.0f;
    return cellVolt[cell];
}

float BMSModule::getLowCellV()
{
    float lowVal = 10.0f;
    for (int i = 0; i < 6; i++) if (cellVolt[i] < lowVal) lowVal = cellVolt[i];
    return lowVal;
}

float BMSModule::getHighCellV()
{
    float hiVal = 0.0f;
    for (int i = 0; i < 6; i++) if (cellVolt[i] > hiVal) hiVal = cellVolt[i];
    return hiVal;
}

float BMSModule::getAverageV()
{
    float avgVal = 0.0f;
    for (int i = 0; i < 6; i++) avgVal += cellVolt[i];
    avgVal /= 6.0f;
    return avgVal;    
}

float BMSModule::getHighestModuleVolt()
{
    return highestModuleVolt;
}

float BMSModule::getLowestModuleVolt()
{
    return lowestModuleVolt;
}

float BMSModule::getHighestCellVolt(int cell)
{
    if (cell < 0 || cell > 5) return 0.0f;
    return highestCellVolt[cell];    
}

float BMSModule::getLowestCellVolt(int cell)
{
    if (cell < 0 || cell > 5) return 0.0f;
    return lowestCellVolt[cell];
}

float BMSModule::getHighestTemp()
{
    return highestTemperature;
}

float BMSModule::getLowestTemp()
{
    return lowestTemperature;
}

float BMSModule::getLowTemp()
{
   return (temperatures[0] < temperatures[1]) ? temperatures[0] : temperatures[1]; 
}

float BMSModule::getHighTemp()
{
   return (temperatures[0] < temperatures[1]) ? temperatures[1] : temperatures[0];     
}

float BMSModule::getAvgTemp()
{
    return (temperatures[0] + temperatures[1]) / 2.0f;
}

float BMSModule::getModuleVoltage()
{
    return moduleVolt;
}

float BMSModule::getTemperature(int temp)
{
    if (temp < 0 || temp > 1) return 0.0f;
    return temperatures[temp];
}

void BMSModule::setAddress(int newAddr)
{
    if (newAddr < 0 || newAddr > MAX_MODULE_ADDR) return;
    moduleAddress = newAddr;
}

int BMSModule::getAddress()
{
    return moduleAddress;
}

bool BMSModule::isExisting()
{
    return exists;
}


void BMSModule::setExists(bool ex)
{
    exists = ex;
}
