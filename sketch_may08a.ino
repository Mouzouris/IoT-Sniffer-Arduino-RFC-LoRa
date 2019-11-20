

#include <SPI.h>
#include <RH_RF95.h>
#include <stdio.h>
#include <stdarg.h>
#define LED 8
//#define ENABLE_FREQ_SCAN
//#define ENABLE_BW_SCAN
//#define ENABLE_SF_SCAN
#define RFM95_CS  10
#define RFM95_RST 9
#define RFM95_INT 2


//#define LED       LED_BUILTIN




#define RH_FLAGS_ACK            0x80

#define LOG_HEADER              "MS,Freq,FreqPacketCount,RSSI,From,To,MsgId,Flags,Ack,DataLen,DataBytes,Data"

// Define the frequencies to scan
#ifdef ENABLE_FREQ_SCAN
#define FREQ_COUNT 10
float _frequencies[] =
{
  
   
       868.1, 868.3, 868.5, 867.1, 867.3, 867.5, 867.7, 867.9, 868.8,    // Uplink (and PJON)
   869.525 

};
#else
#define FREQ_COUNT 1
float _frequencies[] =
{
    868.1

};
#endif

#ifdef ENABLE_BW_SCAN
#define BW_COUNT 3
long int _bandwidths[] =
{
    125000,
    250000,
    500000
    
    
    // etc.
};
#else
#define BW_COUNT 1
long int _bandwidths[] =
{
    125000

};
#endif
#ifdef ENABLE_SF_SCAN

#define SF_COUNT 4
int _spreadingfactors[] =
{
    7,8,9,10,11,12

};
#else
#define SF_COUNT 1
int _spreadingfactors[] =
{
    7
};
#endif

// How long should a frequency be monitored
#define FREQ_TIME_MS            4000

// Used to track how many packets we have seen on each frequency above
uint16_t _packetCounts[FREQ_COUNT][BW_COUNT][SF_COUNT];

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);


uint8_t  _rxBuffer[RH_RF95_MAX_MESSAGE_LEN];                // receive buffer
uint8_t  _rxRecvLen;                                        // number of bytes actually received
char     _printBuffer[512]  = "\0";                         // to send output 
uint32_t _freqExpire        = 0;                            // Millisecond at which frequency should change
uint32_t _freqIndex         = 0;                            // Which frequency is currently monitored
uint32_t _bwIndex         = 0;                            // Which bandwidth is currently monitored
uint32_t _sfIndex         = 0;                            // Which spreading factor is currently monitored

void rf95_setFrequency(uint32_t index)
{
    if (!rf95.setFrequency(_frequencies[index]))
    {
        Serial.println(F("setFrequency failed "));
        while (1);
    }
        rf95.setCodingRate4(5);

    Serial.print(F("Freq: "));
    Serial.print(_frequencies[index]);
    Serial.print(F("\t"));
}
void rf95_setBandwidth(uint32_t index)
{
    rf95.setSignalBandwidth(_bandwidths[index]);
    rf95.setCodingRate4(5);
    Serial.print(F("BW: "));
    Serial.print(_bandwidths[index]);
    Serial.print(F("\t"));
}

void rf95_setSpreadingFactor(uint32_t index)
{
    rf95.setSpreadingFactor(_spreadingfactors[index]);

    Serial.print(F("SF: "));
    Serial.print(_spreadingfactors[index]);
    Serial.print(F("\t"));
}

void setup()
{
      
 
      for(int i=0; i < FREQ_COUNT; ++i)
        for(int j=0; j < BW_COUNT; ++j)
            for(int k=0; k < SF_COUNT; ++k)
                _packetCounts[i][j][k] = 0;
                
    // Configure pins            
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);
    digitalWrite(RFM95_CS, HIGH);

    pinMode(LED, OUTPUT);


    
   // while (!Serial);

    // Initialize Serial
    Serial.begin(115200);
    delay(100);
    Serial.println(F("LoRa Network Probe"));


//     Initialize Radio
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    while (!rf95.init())
    {

          Serial.println(F("Failed"));
        while (1);
    }

          Serial.println(F("Lora init Works"));

    rf95_setFrequency(_freqIndex);
    rf95.setCodingRate4(5);
    rf95_setBandwidth(_bwIndex);
    rf95_setSpreadingFactor(_sfIndex);
    Serial.print(F(" ("));
    Serial.print(_packetCounts[_freqIndex][_bwIndex][_sfIndex]);
    Serial.println(F(")"));
    rf95.setPromiscuous(true);
    rf95.setPayloadCRC(false);
    // Update time for changing frequency
    _freqExpire = millis() + FREQ_TIME_MS;

}

void loop()
{
    _rxRecvLen = sizeof(_rxBuffer);

    digitalWrite(LED, LOW);
    rf95.setModeRx();

    // Handle incoming packet if available
    if (rf95.recv(_rxBuffer, &_rxRecvLen))
    {
        char isAck[4] = { "   " };

        digitalWrite(LED, HIGH);

        _packetCounts[_freqIndex][_bwIndex][_sfIndex]++;

        if (rf95.headerFlags() & RH_FLAGS_ACK)
        {
            memcpy(isAck, "Ack\0", 3);
        }

        _rxBuffer[_rxRecvLen] = '\0';

        Serial.println(millis());

        Serial.print(F("Signal(RSSI)= "));
        Serial.print(rf95.lastRssi(), DEC);
        Serial.print(F(" (Freq: "));
        Serial.print((uint32_t)(_frequencies[_freqIndex] * 10), DEC);
        Serial.println(F(")"));

        Serial.print(F(" "));
        Serial.print(F(" From: "));
        Serial.print(rf95.headerFrom(), DEC);
        Serial.print(F(" >> "));
        Serial.print(F(" To: "));
        Serial.print(rf95.headerTo(), DEC);
        Serial.print(F(" MsgId:"));
        Serial.print(rf95.headerId(), DEC);
        Serial.print(F(" Flags:"));
        Serial.print(rf95.headerFlags(), HEX);
        Serial.print(F(" "));
        Serial.println(isAck);

        Serial.print(F(" "));
        Serial.print(F(" Bytes: "));
        Serial.print(_rxRecvLen, DEC);
        Serial.print(F(" => "));
        Serial.println((char*)_rxBuffer);    
         

        // Add bytes received as hex values
        for (int i = 0; i < _rxRecvLen; i++)
        {
            snprintf(_printBuffer, sizeof(_printBuffer), "%s %02X", _printBuffer, _rxBuffer[i]);
            Serial.print(_rxBuffer[i], HEX);
            Serial.print(" ");

        }

        // Add bytes received as string - this is usually ugly and useless, but
        // it is here just in case. Maybe someone will send something in plaintext
        snprintf(_printBuffer, sizeof(_printBuffer), "%s,%s", _printBuffer, _rxBuffer);
        Serial.println((char*)_rxBuffer);

    }

    // Change frequency if it is time
    if (millis() > _freqExpire)
    {
        rf95.setModeIdle();

        _freqExpire = millis() + FREQ_TIME_MS;
        _freqIndex++;

        if (_freqIndex == FREQ_COUNT)
        {
            _freqIndex = 0;
            ++_bwIndex;
        }
        if (_bwIndex == BW_COUNT)
        {
            _bwIndex = 0;
            ++_sfIndex;
        }

        if (_sfIndex == SF_COUNT)
        {
            _sfIndex = 0;
        }
        
        rf95_setFrequency(_freqIndex);
        rf95_setBandwidth(_bwIndex);
        rf95_setSpreadingFactor(_sfIndex);

                Serial.print(F(" ("));
        Serial.print(_packetCounts[_freqIndex][_bwIndex][_sfIndex]);
        Serial.println(F(")"));
    }
}
