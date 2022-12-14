// Communication Syntax:
//    INPUT: [ .... ]
//    ERROR: || .... ||
//    Status: ## .... ##
//    Registers: (( .... ))

#include <CmdMessenger.h>
#include <string.h>
#include <EEPROM.h>

#define SIZE_ADDRESSPINS 8
#define SIZE_DATAPINS 8
#define SIZE_CARDPINS 4

#define INPUT_SELECTOR 0
#define OUTPUT_SELECTOR 1

#define REGISTER_PERMANENT 0
#define REGISTER_BOARDNR 1
#define REGISTER_VOLTAGE 2

enum class Register
{
  // 00H
  MEASURE = 0,
  // 01H
  BUSCON0 = 1,
  // 02H
  BUSCON1 = 2,
  // 03H
  GNDCON0 = 3,
  // 04H
  GNDCON1 = 4,
  // 05H
  SOURCE = 5,
  // 06H
  DACDATA0 = 6,
  // 07H
  DACDATA1 = 7,
  // 08H
  ERROR_FLAGS = 8,
  // 09H
  RANGE = 9,
  // 0AH
  WAVE_CTRL = 10,
  // 0BH
  WAVE_AMPL0 = 11,
  // 0CH
  WAVE_AMPL1 = 12,
  // 0DH
  WAVE_FREQ0 = 13,
  // 0EH
  WAVE_FREQ1 = 14,
  // 0FH
  WAVE_DUTY = 15,
  // FAH
  READ_SOURCE = 250,
  // FBH
  READ_BUSCON0 = 251,
  // FCH
  READ_BUSCON1 = 252,
  // FDH
  READ_GNDCON0 = 253,
  // FE
  READ_GNDCON1 = 254,
  // FFH
  IDENT = 255,
};
enum class MeasRange
{
  // Do not strip down.
  Bi10 = 1,
  // Strip down with a divider factor of 3
  Bi30 = 3,
  // Strip down with a divider factor of 12
  Bi120 = 12,
};

// BoardNr
// Must be able to be changed in GUI
int boardNumber;

// Data pins
// const int d0_pin = 2;
// const int d1_pin = 3;
// const int d2_pin = 4;
// const int d3_pin = 5;
// const int d4_pin = 6;
// const int d5_pin = 7;
// const int d6_pin = 8;
// const int d7_pin = 9;
const int datapins[SIZE_DATAPINS] = {2, 3, 4, 5, 6, 7, 8, 9};

// Address pins
// const int a0_pin = 18;
// const int a1_pin = 19;
// const int a2_pin = 20;
// const int a3_pin = 21;
// const int a4_pin = 22;
// const int a5_pin = 23;
// const int a6_pin = 24;
// const int a7_pin = 25;
const int addresspins[SIZE_ADDRESSPINS] = {18, 19, 20, 21, 22, 23, 24, 25};

// Card address pins
// const int a8_pin = 37;
// const int a9_pin = 38;
// const int a10_pin = 39;
// const int a11_pin = 40;
const int cardAddresspins[SIZE_CARDPINS] = {37, 38, 39, 40};

// Controller pins
// Remark: active low
const int WR = 51;
const int RD = 52;
const int RESET = 53;

// Pull_up pins
const int ACK = 28;
const int ERR = 29;

// Analog read pins (to measure current/voltage)
const byte AD0 = A14;
const byte AD1 = A13;

// The maximum number of acknowledge check retries.
const int MAX_ACK_CHECK_RETRIES = 100;
// The number of AIO channels for one board.
const int AIO_CHANNELS = 16;
// Time-out to switch relay on.
static int RELAY_ON_SETTLING = 5;
// Time-out to switch relay off.
static int RELAY_OFF_SETTLING = 1;
// The input impedance of the measure circuit. (1M2)
const double MEAS_INPUT_IMP = 1200000;

// 2 registers - each 1 byte - in total 2 bytes
// The data 0 status register, needed for u an i source
int dacData0Status;
// The data 1 status register, needed for u an i source
int dacData1Status;

// The source status register
int sourceStatus;

// 2 registers - each 1 byte - in total 2 bytes
// The bus cofnection 0 status register
int busCon0Status;
// The bus cofnection 1 status register
int busCon1Status;

// 2 registerf - each 1 byte - in total 2 bytes
// The ground connection 0 status register.
int gndCon0Status;
// The ground connection 1 status register.
int gndCon1Status;

// The measure status register.
int measureStatus;
// The U/I bus status register.
int rangeStatus = 0;

// Current set Voltage
float currentVoltage = 0;

// Status of channels (connected to ground/connected to bus)
bool gndChannelStatus[16];
bool busChannelStatus[16];

// ---------------------------  S E T U P  C M D   M E S S E N G E R ----------------------------------------
// Cmd Messenger setup and config for serial communication
char field_separator = ',';
char command_separator = ';';
CmdMessenger cmdMessenger = CmdMessenger(Serial, field_separator, command_separator);
// Defining possible commands
enum class CommandCalls
{
  PUT_VOLTAGE = 1,
  CONNECT_TO_GROUND = 2,
  CONNECT_TO_BUS = 3,
  MEASURE_VOLTAGE = 4,
  MEASURE_CURRENT = 5,
  CHANGE_BOARDNUMBER = 6,
  GET_BOARDNUMBER = 7,
  DISCONNECT_VOLTAGE = 8,
  RESET = 9,
  PERMANENT_WRITE = 10,
  GET_PEVIOUS_STATE = 11
};
// Linking command id's to correct functions
void attachCommandCallbacks()
{
  cmdMessenger.attach(onUnknownCommand);
  cmdMessenger.attach(static_cast<int>(CommandCalls::PUT_VOLTAGE), setVoltageSerial);
  cmdMessenger.attach(static_cast<int>(CommandCalls::DISCONNECT_VOLTAGE), disconnectVoltageSerial);
  cmdMessenger.attach(static_cast<int>(CommandCalls::CONNECT_TO_GROUND), connectToGroundSerial);
  cmdMessenger.attach(static_cast<int>(CommandCalls::CONNECT_TO_BUS), connectToBusSerial);
  cmdMessenger.attach(static_cast<int>(CommandCalls::MEASURE_VOLTAGE), measureVoltageSerial);
  cmdMessenger.attach(static_cast<int>(CommandCalls::MEASURE_CURRENT), measureCurrentSerial);
  cmdMessenger.attach(static_cast<int>(CommandCalls::CHANGE_BOARDNUMBER), setBoardNumber);
  cmdMessenger.attach(static_cast<int>(CommandCalls::GET_BOARDNUMBER), getBoardNumber);
  cmdMessenger.attach(static_cast<int>(CommandCalls::RESET), setup);
  cmdMessenger.attach(static_cast<int>(CommandCalls::PERMANENT_WRITE), permanentWriteSerial);
  cmdMessenger.attach(static_cast<int>(CommandCalls::GET_PEVIOUS_STATE), getPreviousState);
}
// ------------------ E N D   D E F I N E   C A L L B A C K S +   C M D   M E S S E N G E R------------------

// ----------------------------------- C A L L B A C K S  M E T H O D S -------------------------------------
void onUnknownCommand()
{
  Serial.println("||Invalid command received by Arduino, there must be a fault in the communication... The function index received does not match an index stored in the program... Indicating a fault in the communication (retreving data from serial port, ...) ||");
}
void setVoltageSerial()
{
  // 2 inputs from GUI, the integral part and fractional part
  int voltage_int = cmdMessenger.readInt32Arg();
  int voltage_frac = cmdMessenger.readInt32Arg();
  connectVoltageSource(true);
  String combined = String(String(voltage_int) + "." + String(voltage_frac));

  float voltage = combined.toFloat();
  setVoltage(voltage);
}
// Connect correct serial port to the ground
void connectToGroundSerial()
{
  int channel;
  bool connect;
  for (int i = 0; i < 8; i++)
  {
    channel = cmdMessenger.readInt16Arg();
    connect = cmdMessenger.readBoolArg();
    connectToGround(channel, connect);
  }
}
// Connect correct serial port to the bus
void connectToBusSerial()
{
  int channel;
  bool connect;
  for (int i = 0; i < 8; i++)
  {
    channel = cmdMessenger.readInt16Arg();
    connect = cmdMessenger.readBoolArg();
    connectToBus(channel, connect);
  }
}
// Change BoardNumbers
void setBoardNumber()
{
  int boardNr = cmdMessenger.readInt16Arg();
  boardNumber = boardNr;
  Serial.print("##Succesfully changed boardNr to: ");
  Serial.print(boardNr);
  Serial.println("##");
}
void getBoardNumber()
{
  Serial.println("BoardNumber: [" + String(boardNumber) + "]");
}
void measureVoltageSerial()
{
  int channel = cmdMessenger.readInt32Arg();
  double voltage = measureVoltage(channel);
  Serial.println("Measured Voltage: [" + String(voltage) + "]");
}
void measureCurrentSerial()
{
  double measuredCurrent = measureCurrentUsource();
  Serial.println("Measured current: [" + String(measuredCurrent) + "]");
}
void disconnectVoltageSerial()
{
  connectVoltageSource(false);
}
void permanentWriteSerial()
{
  bool permanent = cmdMessenger.readBoolArg();
  permanentWrite(permanent);
  if (permanent)
    Serial.println("##Enabled permanent storage##");
  else
    Serial.println("##Disabled permanent storage##");
}
void getPreviousState()
{
  Serial.println("##Trying to retrieve previous state##");
  byte eeprom_value;

  eeprom_value = EEPROM.read(REGISTER_PERMANENT);
  Serial.println("Permanent: [" + String(eeprom_value) + "] ");
  if (eeprom_value != 0)
  {
    Serial.println("Voltage: [" + String(currentVoltage) + "] ");
    eeprom_value = EEPROM.read(REGISTER_BOARDNR);
  }
}
// -------------------------------- E N D  C A L L B A C K  M E T H O D S ----------------------------------

// A test function which executes some basic funcionallities of the program
void testFullFunctionallity()
{
  connectToBus(1, true);
  connectVoltageSource(true);
  setVoltage(11);
  Serial.println("***********");
  double measured = measureCurrentUsource();
  Serial.println("Measured current = " + String(measured));
  measured = measureVoltage(1);
  Serial.println("Measured Voltage = " + String(measured));
  Serial.println("***********");
  Serial.println();
  delay(5000);

  connectToBus(1, false);
  setVoltage(0);
  Serial.println("***********");
  measured = measureCurrentUsource();
  Serial.println("Measured current = " + String(measured));
  measured = measureVoltage(1);
  Serial.println("Measured Voltage = " + String(measured));
  Serial.println("***********");
  Serial.println();
  delay(5000);
}
// Set the initial register statusses in the code
void setupStatus()
{
  dacData0Status = 0x00;
  dacData1Status = 0x80;
  sourceStatus = 0x00;
  busCon0Status = 0x00;
  busCon1Status = 0x00;
  gndCon0Status = 0x00;
  gndCon1Status = 0x00;
  measureStatus = 0x00;
  rangeStatus = 0x00;
  // The DAC is reset
  writeData(Register::DACDATA0, dacData0Status, boardNumber);
  writeData(Register::DACDATA1, dacData1Status, boardNumber);
  // The SOURCE register is reset
  writeData(Register::SOURCE, sourceStatus, boardNumber);
  // Rhe MEASURE register is reset
  writeData(Register::MEASURE, measureStatus, boardNumber);
  // All relays are switched off
  writeData(Register::BUSCON0, busCon0Status, boardNumber);
  writeData(Register::BUSCON1, busCon1Status, boardNumber);
  writeData(Register::GNDCON0, gndCon0Status, boardNumber);
  writeData(Register::GNDCON1, gndCon1Status, boardNumber);
  // The UI-bus register is reset.
  writeData(Register::RANGE, rangeStatus, boardNumber);
  // Read the errorflags to clear the register
  readData(Register::ERROR_FLAGS, boardNumber);
  // settling time
  delay(RELAY_OFF_SETTLING);
}

// 'reset' arduino
void setup()
{
  Serial.begin(115200);
  Serial.println("##Setup Arduino##");
  boardNumber = 0x00;
  setupPins();
  setupStatus();

  // Setup cmdMessenger
  attachCommandCallbacks();
  cmdMessenger.printLfCr();

  // Keep track of which channels connected to bus/gnd
  for (int i = 0; i < 16; i++)
  {
    busChannelStatus[i] = false;
    gndChannelStatus[i] = false;
  }
  Serial.println("##Setup Complete##");
  delay(50);
  restoreSession();
}
// Get previous state out off the EEPROM memory
void restoreSession()
{
  byte eeprom_value;
  eeprom_value = EEPROM.read(REGISTER_PERMANENT);
  delay(50);
  if (eeprom_value == 1)
  {
    Serial.println("##Restoring session##");
    connectToBus(1, true);
    connectVoltageSource(true);
    eeprom_value = EEPROM.read(REGISTER_BOARDNR);
    delay(100);
    boardNumber = eeprom_value;
    Serial.println("##Set boardNumber to " + String(boardNumber) + "##");
    eeprom_value = EEPROM.read(REGISTER_VOLTAGE);
    delay(100);
    currentVoltage = eeprom_value;
    setVoltage(currentVoltage);
    Serial.println("##Finished Restoring session##");
  }
  else
  {
    Serial.println("##Not Restoring session##");
  }
}
// In the loop, the cmdMessenger keeps checking for new input commands
void loop()
{
  // processing incoming commands
  cmdMessenger.feedinSerialData();
}
