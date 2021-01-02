//////////////////////////////////////////////////////////////////////////////////////////
//
//   Arduino Library for ADS1292R Shield/Breakout
//
//   Copyright (c) 2017 ProtoCentral
//   Heartrate and respiration computation based on original code from Texas Instruments
//   Note: This example is not compatible with arduino uno as the algorithm requires higher RAM size for its computations.
//
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   Requires g4p_control graphing library for processing.  Built on V4.1
//   Downloaded from Processing IDE Sketch->Import Library->Add Library->G4P Install
//   If you have bought the breakout the connection with the Arduino board is as follows:
//
//  |ads1292r pin label| Arduino Connection   |Pin Function      |
//  |----------------- |:--------------------:|-----------------:|
//  | VDD              | +5V                  |  Supply voltage  |
//  | PWDN/RESET       | D4                   |  Reset           |
//  | START            | D5                   |  Start Input     |
//  | DRDY             | D6                   |  Data Ready Outpt|
//  | CS               | D7                   |  Chip Select     |
//  | MOSI             | D11                  |  Slave In        |
//  | MISO             | D12                  |  Slave Out       |
//  | SCK              | D13                  |  Serial Clock    |
//  | GND              | Gnd                  |  Gnd             |
//
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentralAds1292r.h"
#include "ecgRespirationAlgo.h"
#include <SPI.h>

volatile uint8_t globalHeartRate;
volatile uint8_t globalRespirationRate=0;

const int ADS1292_DRDY_PIN = 26;
const int ADS1292_CS_PIN = 13;
const int ADS1292_START_PIN = 14;
const int ADS1292_PWDN_PIN = 27;

int16_t ecgWaveBuff, ecgFilterout;
int16_t resWaveBuff,respFilterout;

long timeElapsed=0;

ads1292r ADS1292R;
ecg_respiration_algorithm ECG_RESPIRATION_ALGORITHM;

void setup()
{
  delay(2000);

  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  //CPOL = 0, CPHA = 1
  SPI.setDataMode(SPI_MODE1);
  // Selecting 1Mhz clock for SPI
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  pinMode(ADS1292_DRDY_PIN, INPUT);
  pinMode(ADS1292_CS_PIN, OUTPUT);
  pinMode(ADS1292_START_PIN, OUTPUT);
  pinMode(ADS1292_PWDN_PIN, OUTPUT);
  Serial.begin(57600);

  ADS1292R.ads1292Init(ADS1292_CS_PIN,ADS1292_PWDN_PIN,ADS1292_START_PIN);  //initalize ADS1292 slave
  Serial.println("Initiliziation is done");
}

void loop()
{
  ads1292OutputValues ecgRespirationValues;
  boolean ret = ADS1292R.getAds1292EcgAndRespirationSamples(ADS1292_DRDY_PIN,ADS1292_CS_PIN,&ecgRespirationValues);

  if (ret == true)
  {
    ecgWaveBuff = (int16_t)(ecgRespirationValues.sDaqVals[1] >> 8) ;  // ignore the lower 8 bits out of 24bits
    resWaveBuff = (int16_t)(ecgRespirationValues.sresultTempResp>>8) ;

    if(ecgRespirationValues.leadoffDetected == false)
    {
      ECG_RESPIRATION_ALGORITHM.ECG_ProcessCurrSample(&ecgWaveBuff, &ecgFilterout);   // filter out the line noise @40Hz cutoff 161 order
      ECG_RESPIRATION_ALGORITHM.QRS_Algorithm_Interface(ecgFilterout,&globalHeartRate);// calculate
      
      //disable below 2 lines if you want to run with arduino uno. (arduino uno does not have the memory to do all processing together)
      respFilterout = ECG_RESPIRATION_ALGORITHM.Resp_ProcessCurrSample(resWaveBuff);
      ECG_RESPIRATION_ALGORITHM.RESP_Algorithm_Interface(respFilterout,&globalRespirationRate);

    }else{

      ecgFilterout = 0;
      respFilterout = 0;
    }

    if(millis() > timeElapsed)  // update every one second
    {
      if(ecgRespirationValues.leadoffDetected == true) // lead in not connected
      {
        Serial.println("ECG lead error!!! ensure the leads are properly connected");
      }else{

        Serial.print("Heart rate: ");
        Serial.print(globalHeartRate);
        Serial.println("BPM");
        Serial.print("Respiration Rate :");
        Serial.println(globalRespirationRate);
      }
      timeElapsed += 1000;
    }
  }
 }