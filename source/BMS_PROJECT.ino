//Battery Management System Engine

#define NUM_CELLS 4
#define RELAY 19
#define RELAY_FEEDBACK 18
#define SDA 21
#define SCL 22

//Auth Token
#define BLYNK_TEMPLATE_ID "TMPL3-ZkdXwFY"
#define BLYNK_TEMPLATE_NAME "BMS Safety System"
#define BLYNK_AUTH_TOKEN "gWo3WmFtHsOsUV2brgC8GAxxpl2QUWIR"

#define BLYNK_SEND_INTERVAL 1000
#define EVENT_VOLTAGE_CHANGE 0.02
#define EVENT_RSSI_CHANGE    5

//Libraries 
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>

//Blynk IOT
unsigned long lastBlynkUpdate = 0;
// Graph sampling
unsigned long lastGraphUpdate = 0;
#define GRAPH_UPDATE_INTERVAL 5000

int lastBatteryStatus = -1;
int lastRelayState = -1;
int lastSystemState = -1;
int lastFaultDetected = -1;

float lastCellVoltage[NUM_CELLS] = {
  -1, -1, -1, -1
};

int lastRSSI = -1000;

// OFFLINE TELEMETRY EVENT QUEUE
#define OFFLINE_QUEUE_SIZE 20

enum TelemetryEventType
{
  EVENT_CELL_VOLTAGE,
  EVENT_RELAY_STATE,
  EVENT_FAULT_STATE,
  EVENT_SYSTEM_STATE,
  EVENT_RSSI
};

struct TelemetryEvent
{
  TelemetryEventType type;

  unsigned long timestamp;

  int cellIndex;

  float value;

  int intValue;

  char text[20];
};

TelemetryEvent offlineQueue[OFFLINE_QUEUE_SIZE];

int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

//Enterprise Analytics
char operatorRecommendation[80] = "SYSTEM OPERATING NORMALLY";

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

//Initialise LCD
LiquidCrystal_I2C lcd(0x27 , 16 , 2);

#define PAGE_INTERVAL 3000
#define DISPLAY_INTERVAL 500

//adding a 2 second delay between the intro page and the status pages
#define STARTUP_INTERVAL 2000

unsigned long lastPageChange = 0;
unsigned long lastDisplayCheck = 0;
unsigned long startupTime = 0;
int startupComplete = 0;

int currentPage = 0;

char lastLine1[17] = "              ";
char lastLine2[17] = "              ";

//TEST
#define CELL1 34
#define CELL2 35
#define CELL3 32
#define CELL4 33

#define HIGH_SOC_LIMIT 80
#define LOW_SOC_LIMIT  40 

#define HIGH_SOC_THRESHOLD 0.10
#define NORMAL_SOC_THRESHOLD 0.15
#define LOW_SOC_THRESHOLD 0.20

int cell[NUM_CELLS] = {
  CELL1,CELL2,CELL3,CELL4
};

int weakestCell;
int strongestCell;
int batteryStatus; // 0 - BALANCED, 1 - IMBALANCED

float voltageImbalance;    //VOLTAGE IMBALANCE

//Imbalance trend
float previousImbalance;
int imbalanceTrend; // 0 - STABLE, 1 - INCREASING, 2 - DECREASING
int firstReading = 1;

// To calculate the voltage imbalance
float weakestVoltage;
float strongestVoltage;

//State of Charge
float cellSoC[NUM_CELLS];

//Adaptive Imbalance
float averageSoC;
float adaptiveThreshold;

//Array to store voltage values of every cell
float cellVoltage[NUM_CELLS];

//Raw ESP-32 ADC value to voltage value
float readVoltage(int pin)
{
  int rawValue = analogRead(pin);
  return (rawValue / 4095.0) * 3.3;
}

//Obtain Percentage value from the CELL
int getSoC(float voltage)
{
  return constrain((voltage / 3.3) * 100, 0, 100);
}

//Voltage Protection Limits
#define OVER_PROTECTION_LIMIT 3.20
#define UNDER_PROTECTION_LIMIT 0.80

//Hysteresis 
#define OVER_VOLTAGE_RELEASE 3.10
#define UNDER_VOLTAGE_RELEASE 0.90

//Timing
#define DEBOUNCE_TIME 1000
#define RECOVERY_TIME 5000
#define RELAY_MISMATCH_TIME 1000


//Sensor Jump Detection
#define MAX_VOLTAGE_JUMP 0.50

//Valid Cell Voltage Range
#define MIN_VALID_VOLTAGE 0.0
#define MAX_VALID_VOLTAGE 3.3

//Relay State
int relayState; //0 - NORMAL , 1 - FAULT, 2 - RECOVERY
#define NORMAL 0
#define FAULT 1
#define RECOVERY 2 
int faultDetected = 0;

//Relay Feedback 
int expectedRelayState = LOW;
int actualRelayState = HIGH;
unsigned long relayMismatchStartTime = 0;
int relayMismatchDetected = 0;
int relayMismatchConfirmed = 0;

// enum function
enum SystemState
{
  STATE_NORMAL,
  STATE_DEGRADED, 
  STATE_FAILSAFE, 
  STATE_SHUTDOWN
};

SystemState systemState = STATE_NORMAL;

// State Machine Timing
unsigned long stateVerificationStartTime = 0; //ensures FAILSAFE doesn't jump directly to NORMAL
#define STATE_VERIFICATION_TIME 5000  

int shutdownRequested = 0;  //explicit SHUTDOWN Mechanism
int stateVerificationActive = 0;

//Timer
unsigned long faultStartTime = 0;
unsigned long recoveryStartTime = 0;
unsigned long lastSensorChangeTime = 0;

//Window-based Noise Detection
#define WINDOW_SIZE 5
float voltageHistory[NUM_CELLS][WINDOW_SIZE];
int historyIndex = 0;

//Sample Non-blocking
#define SAMPLE_INTERVAL 1000
unsigned long lastSampleTime;
int sampleCount = 0;

//Debounce
#define JUMP_CONFIRM_TIME 2000
unsigned long jumpStartTime[NUM_CELLS];
int jumpDetected[NUM_CELLS];
int confirmedJump[NUM_CELLS];

//Frozen Sensor
#define FROZEN_TOLERANCE 0.005
#define FROZEN_TIME 5000
unsigned long frozenStartTime[NUM_CELLS];
int frozenDetected[NUM_CELLS];
int confirmedFrozen[NUM_CELLS];
float previousVoltage[NUM_CELLS]; //Frozen Detection - comparing previous and current reading
int firstSensorReading = 1;

//  WIFI / BLYNK CONNECTION 
enum ConnectionState
{
  WIFI_START,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  BLYNK_CONNECTED,
  CONNECTION_OFFLINE
};

ConnectionState connectionState = WIFI_START;

unsigned long wifiConnectStart = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastBlynkAttempt = 0;

#define WIFI_CONNECT_TIMEOUT 10000
#define WIFI_RETRY_INTERVAL 5000
#define BLYNK_RETRY_INTERVAL 5000

int wifiConnected = 0;
int blynkConnected = 0;
int previousWifiConnected = -1;
int previousBlynkConnected = -1;

// Fault Definitions
enum FaultID
{
  NO_FAULT,
  OVER_VOLTAGE_FAULT,
  UNDER_VOLTAGE_FAULT,
  VOLTAGE_JUMP_FAULT,
  FROZEN_ADC_FAULT,
  OUT_OF_RANGE_FAULT,
  RELAY_MISMATCH_FAULT,
  COMMUNICATION_FAULT,
  ADC_FAILURE_FAULT
};

FaultID lastBlynkFaultID = NO_FAULT;

enum FaultSource
{
  SOURCE_NONE,
  SOURCE_BATTERY_CELL,
  SOURCE_RELAY,
  SOURCE_COMMUNICATION,
  SOURCE_ADC
};

FaultID faultID = NO_FAULT;
FaultSource faultSource = SOURCE_NONE;
int faultCell = -1;
// Fault history
FaultID previousAnalyticsFault = NO_FAULT;
FaultID lastFaultID = NO_FAULT;
FaultSource lastFaultSource = SOURCE_NONE;
int lastFaultCell = -1;
int adcFailureTest = 0;
int commsFailureTest = 0;

// Task 6 - Analytics
unsigned long totalFaultCount = 0;
unsigned long faultEventCount = 0;
int previousFaultDetected = 0;

int compositeRiskScore = 0;

const char* batteryHealth = "HEALTHY";
const char* maintenanceRecommendation = "System operating normally";

//Fault History Update
void updateFaultHistory()
{
  if(faultDetected == 1 && faultID != NO_FAULT)
  {
    if(previousAnalyticsFault == NO_FAULT)
    {
      totalFaultCount++;
    }
    else if(previousAnalyticsFault != faultID)
    {
      totalFaultCount++;
    }

    previousAnalyticsFault = faultID;
  }
  else
  {
    previousAnalyticsFault = NO_FAULT;
  }
}

//System State Name
const char* getSystemStateName(int state)
{
  if(state == STATE_NORMAL)
  {
    return "NORMAL";
  }
  else if(state == STATE_DEGRADED)
  {
    return "DEGRADED"; 
  }
  else if(state == STATE_FAILSAFE)
  {
    return "FAILSAFE";
  }
  else if(state == STATE_SHUTDOWN)
  {
    return "SHUTDOWN";
  }
  else
  {
    return "UNKNOWN";
  }
}

//System State Transition
void logSystemStateTransition(int previousState, int newState)
{
  Serial.print("STATE TRANSITION | TIME: ");
  Serial.print(millis());
  Serial.print(" ms | PREVIOUS: ");
  Serial.print(getSystemStateName(previousState));
  Serial.print(" | NEW: ");
  Serial.print(getSystemStateName(newState));
  Serial.print(" | FAULT-ID: ");
  if(faultID == OVER_VOLTAGE_FAULT)
  {
    Serial.print("OVER VOLTAGE");
  }
  else if(faultID == UNDER_VOLTAGE_FAULT)
  {
    Serial.print("UNDER VOLTAGE");
  }
  else if(faultID == VOLTAGE_JUMP_FAULT)
  {
    Serial.print("VOLTAGE JUMP");
  }
  else if(faultID == FROZEN_ADC_FAULT)
  {
    Serial.print("FROZEN ADC");
  }
  else if(faultID == OUT_OF_RANGE_FAULT)
  {
    Serial.print("OUT OF RANGE");
  }
  else if(faultID == RELAY_MISMATCH_FAULT)
  {
    Serial.print("RELAY MISMATCH");
  }
  else if(faultID == COMMUNICATION_FAULT)
  {
    Serial.print("COMMUNICATION");
  }
  else if(faultID == ADC_FAILURE_FAULT)
  {
    Serial.print("ADC FAILURE");
  }
  else
  {
    Serial.print("NO_FAULT");
  }
  Serial.println();
}

void updateVoltageHistory()
{
  for(int i = 0; i < NUM_CELLS; i++)
  {
    voltageHistory[i][historyIndex] = cellVoltage[i];
  }
  historyIndex++;
  if(historyIndex >= WINDOW_SIZE)
  {
    historyIndex = 0;
  }
  if(sampleCount < WINDOW_SIZE)
  {
    sampleCount++;
  }
}

float getWindowAverage(int cellIndex)
{
  float sum = 0;
  for(int i = 0; i < sampleCount; i++)
  {
    sum = sum + voltageHistory[cellIndex][i];
  }
  if(sampleCount == 0)
  {
    return cellVoltage[cellIndex];
  }
  return sum / sampleCount;
}

int detectVoltageJump(int cellIndex)
{
  float averageVoltage = getWindowAverage(cellIndex);
  float voltageDifference = cellVoltage[cellIndex] - averageVoltage;
  if(voltageDifference < 0)
  {
    voltageDifference = -voltageDifference;
  }
  if(voltageDifference > MAX_VOLTAGE_JUMP)
  {
    return 1;
  }
  return 0;
}

int detectOutOfRange(int cellIndex)
{
  if(cellVoltage[cellIndex] < MIN_VALID_VOLTAGE || cellVoltage[cellIndex] > MAX_VALID_VOLTAGE)
  {
    return 1;
  }
  return 0;
}

//Debounce Logic
void checkVoltageJumps()
{
  for(int i = 0; i < NUM_CELLS; i++)
  {
    if(detectVoltageJump(i) == 1)
    {
      if(jumpDetected[i] == 0)
      {
        jumpDetected[i] = 1;
        jumpStartTime[i] = millis();
      }
      else if(millis() - jumpStartTime[i] >= JUMP_CONFIRM_TIME)
      {
        confirmedJump[i] = 1;
        Serial.print("CONFIRMED VOLTAGE JUMP - CELL ");
        Serial.println(i + 1);
      }
    }
    else
    {
      jumpDetected[i] = 0;
      jumpStartTime[i] = 0;
      confirmedJump[i] = 0;
    }
  }
}

//Frozen Sensor Detection
void checkFrozenSensors()
{
  //Check whether the battery has any meaningful voltage variation
  float maxChange = 0;
  
  for(int i = 0; i < NUM_CELLS; i++)
  {
    float change = cellVoltage[i] - previousVoltage[i];
    if(change < 0)
    {
      change = -change;
    }
    if(change > maxChange)
    {
      maxChange = change;
    }
  }
  //If the battery is changing, look for the frozen cell
  if(maxChange > FROZEN_TOLERANCE)
  {
    for(int i = 0; i < NUM_CELLS; i++)
    {
      float difference = cellVoltage[i] - previousVoltage[i];
      if(difference < 0)
      {
        difference = -difference;
      }
      if(difference <= FROZEN_TOLERANCE)
      {
        if(frozenDetected[i] == 0)
        {
          frozenDetected[i] = 1;
          frozenStartTime[i] = millis();
        }
        else if(millis() - frozenStartTime[i] >= FROZEN_TIME)
        {
          if(confirmedFrozen[i] == 0)
          {
            confirmedFrozen[i] = 1;
            Serial.print("CONFIRMED FROZEN SENSOR - CELL ");
            Serial.println(i + 1);
          }
        }
      }
      else
      {
        frozenDetected[i] = 0;
        frozenStartTime[i] = 0;
        confirmedFrozen[i] = 0;
      }
    }
  }
  else
  {
    //Battery is Stable, so no classifying stable readings as frozen
    for(int i = 0; i < NUM_CELLS; i++)
    {
      frozenDetected[i] = 0;
      frozenStartTime[i] = 0;
    }
  }

  //Store current readings for next sample
  for(int i = 0; i < NUM_CELLS ; i++)
  {
    previousVoltage[i] = cellVoltage[i];
  }
}

int readRelayFeedback()
{
  return digitalRead(RELAY_FEEDBACK);
}

//Relay Mismatch Detection
void checkRelayMismatch()
{
  actualRelayState = readRelayFeedback();
  Serial.print("RELAY CHECK | Expected: ");
  Serial.print(expectedRelayState);
  Serial.print(" | Actual: ");
  Serial.println(actualRelayState);
  if(actualRelayState != expectedRelayState)
  {
    if(relayMismatchDetected == 0)
    {
      relayMismatchDetected = 1;
      relayMismatchStartTime = millis();
    }
    else if(millis() - relayMismatchStartTime >= RELAY_MISMATCH_TIME)
    {
      relayMismatchConfirmed = 1;
    }
  }
  else
  {
    relayMismatchConfirmed = 0;
    relayMismatchDetected = 0;
    relayMismatchStartTime = 0;
  }
}

//Store Fault
void storeFault()
{
  lastFaultID = faultID;
  lastFaultSource = faultSource;
  lastFaultCell = faultCell;
}

//Print Last Fault
void printLastFault()
{
  Serial.print(" | STORED FAULT | ID: ");
  if(lastFaultID == OVER_VOLTAGE_FAULT)
  {
    Serial.print("OVER VOLTAGE");
  }
  else if(lastFaultID == UNDER_VOLTAGE_FAULT)
  {
    Serial.print("UNDER VOLTAGE");
  }
  else if(lastFaultID == VOLTAGE_JUMP_FAULT)
  {
    Serial.print("VOLTAGE JUMP");
  }
  else if(lastFaultID == FROZEN_ADC_FAULT)
  {
    Serial.print("FROZEN ADC");
  }
  else if(lastFaultID == OUT_OF_RANGE_FAULT)
  {
    Serial.print("OUT OF RANGE");
  }
  else if(lastFaultID == RELAY_MISMATCH_FAULT)
  {
    Serial.print("RELAY MISMATCH");
  }
  else if(lastFaultID == ADC_FAILURE_FAULT)
  {
    Serial.print("ADC FAILURE");
  }
  else if(lastFaultID == COMMUNICATION_FAULT)
  {
    Serial.print("COMMUNICATION");
  }
  else
  {
    Serial.print("NO FAULT");
  }
  Serial.print(" | SOURCE: ");
  if(lastFaultSource == SOURCE_BATTERY_CELL)
  {
    Serial.print("BATTERY CELL");
  }
  else if(lastFaultSource == SOURCE_ADC)
  {
    Serial.print("ADC");
  }
  else if(lastFaultSource == SOURCE_RELAY)
  {
    Serial.print("RELAY");
  }
  else if(lastFaultSource == SOURCE_COMMUNICATION)
  {
    Serial.print("COMMUNICATION");
  }
  else
  {
    Serial.print("NONE");
  }
  Serial.print(" | CELL: ");
  if(lastFaultCell >= 0)
  {
    Serial.println(lastFaultCell + 1);
  }
  else
  {
    Serial.println("N/A");
  }
}

//Fault Detection
void detectFaults()
{
  faultDetected = 0;
  faultID = NO_FAULT;
  faultSource = SOURCE_NONE;
  faultCell = -1;

  if(adcFailureTest == 1)
  {
    faultDetected = 1;
    faultID = ADC_FAILURE_FAULT;
    faultSource = SOURCE_ADC;
    faultCell = -1;
    storeFault();
    return;
  }

  if(commsFailureTest == 1)
  {
    faultDetected = 1;
    faultID = COMMUNICATION_FAULT;
    faultSource = SOURCE_COMMUNICATION;
    faultCell = -1;
    storeFault();
    return;
  }

  for(int i = 0; i < NUM_CELLS; i++)
  {
    if(detectOutOfRange(i) == 1)
    {
      faultDetected = 1;
      faultID = OUT_OF_RANGE_FAULT;
      faultSource = SOURCE_BATTERY_CELL;
      faultCell = i;
      storeFault();
      return; 
    }
    if(cellVoltage[i] >= OVER_PROTECTION_LIMIT)
    {
      faultDetected = 1;
      faultID = OVER_VOLTAGE_FAULT;
      faultSource = SOURCE_BATTERY_CELL;
      faultCell = i;
      storeFault();
      return;
    }
    if(cellVoltage[i] <= UNDER_PROTECTION_LIMIT)
    {
      faultDetected = 1;
      faultID = UNDER_VOLTAGE_FAULT;
      faultSource = SOURCE_BATTERY_CELL;
      faultCell = i;
      storeFault();
      return;
    }
    if(confirmedJump[i] == 1)
    {
      faultDetected = 1;
      faultID = VOLTAGE_JUMP_FAULT;
      faultSource = SOURCE_BATTERY_CELL;
      faultCell = i;
      storeFault();
      return;
    }
    if(confirmedFrozen[i] == 1)
    {
      faultDetected = 1;
      faultID = FROZEN_ADC_FAULT;
      faultSource = SOURCE_ADC;
      faultCell = i;
      storeFault();
      return;
    }
  }
  if(relayMismatchConfirmed == 1)
  {
    faultDetected = 1;
    faultID = RELAY_MISMATCH_FAULT;
    faultSource = SOURCE_RELAY;
    faultCell = -1; // -1 because no cell representation here.
    storeFault();
    return;
  }
}

//System State Update
void updateSystemState()
{
  SystemState previousState = systemState;

  //HIGHEST PRIORITY FOR SHUTDOWN
  if(shutdownRequested == 1)
  {
    systemState = STATE_SHUTDOWN;
  }

  // NORMAL STATE
  else if(systemState == STATE_NORMAL)
  {
    if(faultDetected == 1)
    {
      if(faultID == COMMUNICATION_FAULT)
      {
        systemState = STATE_DEGRADED;
      }
      else
      {
        systemState = STATE_FAILSAFE;
      }
    }
    else if(batteryStatus == 1)
    {
      systemState = STATE_DEGRADED;
    }
  }

  // DEGRADED STATE
  else if(systemState == STATE_DEGRADED)
  {
    if(faultDetected == 1)
    {
      if(faultID == COMMUNICATION_FAULT)
      {
        systemState = STATE_DEGRADED;
      }
      else
      {
        systemState = STATE_FAILSAFE;
      }
    }
    else if(batteryStatus == 0)
    {
      systemState = STATE_NORMAL;
    }
  }

  // FAILSAFE STATE
  else if(systemState == STATE_FAILSAFE)
  {
    if(faultDetected == 1 || faultCleared() == 0 || relayMismatchConfirmed == 1 || actualRelayState != HIGH)
    {
      systemState = STATE_FAILSAFE;
      stateVerificationStartTime = 0;
      stateVerificationActive = 0;
    }
    else
    {
      if(stateVerificationActive == 0)
      {
        stateVerificationStartTime = millis();
        stateVerificationActive = 1;
        Serial.println("FAILSAFE RECOVERY VERIFICATION STARTED");
      }
      else if(millis() - stateVerificationStartTime >= STATE_VERIFICATION_TIME)
      {
        systemState = STATE_NORMAL;
        stateVerificationStartTime = 0;
        stateVerificationActive = 0;
        Serial.println("FAILSAFE RECOVERY VERIFIED");
      }
    }
  }

  // SHUTDOWN STATE
  else if(systemState == STATE_SHUTDOWN)
  {
    systemState = STATE_SHUTDOWN;
  }

  // STRUCTURED TRANSITION LOG
  if(previousState != systemState)
  {
    Serial.print("STATE TRANSITION | TIME ");
    Serial.print(millis());
    Serial.print(" ms | PREVIOUS: ");
    if(previousState == STATE_NORMAL)
    {
      Serial.print("NORMAL");
    }
    else if(previousState == STATE_DEGRADED)
    {
      Serial.print("DEGRADED");
    }
    else if(previousState == STATE_FAILSAFE)
    {
      Serial.print("FAILSAFE");
    }
    else if(previousState == STATE_SHUTDOWN)
    {
      Serial.print("SHUTDOWN");
    }

    Serial.print(" | NEW: ");
    if(systemState == STATE_NORMAL)
    {
      Serial.print("NORMAL");
    }
    else if(systemState == STATE_DEGRADED)
    {
      Serial.print("DEGRADED");
    }
    else if(systemState == STATE_FAILSAFE)
    {
      Serial.print("FAILSAFE");
    }
    else if(systemState == STATE_SHUTDOWN)
    {
      Serial.print("SHUTDOWN");
    }

    Serial.print(" | FAULT-ID: ");
    if(faultID == OVER_VOLTAGE_FAULT)
    {
      Serial.print("OVER VOLTAGE");
    }
    else if(faultID == UNDER_VOLTAGE_FAULT)
    {
      Serial.print("UNDER VOLTAGE");
    }
    else if(faultID == VOLTAGE_JUMP_FAULT)
    {
      Serial.print("VOLTAGE JUMP");
    }
    else if(faultID == FROZEN_ADC_FAULT)
    {
      Serial.print("FROZEN ADC");
    }
    else if(faultID == RELAY_MISMATCH_FAULT)
    {
      Serial.print("RELAY MISMATCH");
    }
    else if(faultID == OUT_OF_RANGE_FAULT)
    {
      Serial.print("OUT OF RANGE");
    }
    else if(faultID == COMMUNICATION_FAULT)
    {
      Serial.print("COMMUNICATION");
    }
    else if(faultID == ADC_FAILURE_FAULT)
    {
      Serial.print("ADC FAILURE");
    }
    else
    {
      Serial.print("NO FAULT");
    }
    Serial.println();
  }
}

//Relay Control based on SystemState
void controlRelay()
{
  if(systemState == STATE_FAILSAFE)
  {
    relayState = FAULT;
    expectedRelayState = HIGH;
    digitalWrite(RELAY, HIGH);
  }
  else if(systemState == STATE_SHUTDOWN)
  {
    relayState = FAULT;
    expectedRelayState = HIGH;
    digitalWrite(RELAY, HIGH);
  }
  else if(systemState == STATE_DEGRADED)
  {
    relayState = NORMAL;
    expectedRelayState = LOW;
    digitalWrite(RELAY, LOW);
  }
  else if(systemState == STATE_NORMAL)
  {
    relayState = NORMAL;
    expectedRelayState = LOW;
    digitalWrite(RELAY, LOW);
  }
}

//Controlled Trigger for SHUTDOWN Mechanism
void checkShutdownCommand()
{
  if(Serial.available() > 0)
  {
    char command = Serial.read();
    if((command == 'S' || command == 's') && shutdownRequested == 0)
    {
      shutdownRequested = 1;
      Serial.println("SHUTDOWN REQUESTED");
    }
  }
}

//Hysteresis Function
int faultCleared()
{
  for(int i = 0; i < NUM_CELLS; i++)
  {
    if(cellVoltage[i] > OVER_VOLTAGE_RELEASE)
    {
      return 0;
    }
    if(cellVoltage[i] < UNDER_VOLTAGE_RELEASE)
    {
      return 0;
    }
    if(detectOutOfRange(i) == 1)
    {
      return 0;
    }
    if(confirmedJump[i] == 1)
    {
      return 0;
    }
    if(confirmedFrozen[i] == 1)
    {
      return 0;
    }
  }
  return 1;
}

void printSystemStatus()
{
  Serial.print("Battery Status: ");
  if(batteryStatus == 0)
  {
    Serial.println("BALANCED");
  }
  else
  {
    Serial.println("IMBALANCED");
  }

  Serial.print("Relay State: ");
  if(relayState == NORMAL)
  {
    Serial.println("NORMAL");
  }
  else if(relayState == FAULT)
  {
    Serial.println("FAULT");
  }
  else if(relayState == RECOVERY)
  {
    Serial.println("RECOVERY");
  }
}

void printFault()
{
  if(faultDetected == 1)
  {
    Serial.print("FAULT: ");
    if(faultID == OVER_VOLTAGE_FAULT)
    {
      Serial.print("OVER VOLTAGE");
    }
    else if(faultID == UNDER_VOLTAGE_FAULT)
    {
      Serial.print("UNDER VOLTAGE");
    }
    else if(faultID == VOLTAGE_JUMP_FAULT)
    {
      Serial.print("VOLTAGE JUMP");
    }
    else if(faultID == FROZEN_ADC_FAULT)
    {
      Serial.print("FROZEN SENSOR");
    }
    else if(faultID == OUT_OF_RANGE_FAULT)
    {
      Serial.print("OUT OF RANGE");
    }
    else if(faultID == RELAY_MISMATCH_FAULT)
    {
      Serial.print("RELAY MISMATCH");
    }
    else if(faultID == ADC_FAILURE_FAULT)
    {
      Serial.print("ADC FAILURE");
    }
    else if(faultID == COMMUNICATION_FAULT)
    {
      Serial.print("COMMUNICATION");
    }
    
    
    if(faultCell >= 0)
    {
      Serial.print(" - CELL ");
      Serial.println(faultCell + 1);
    }
    else
    {
      Serial.println(" - CELL N/A");
    }
  }
}

void updateLCDline(int row, const char *newText)
{
  char *lastLine;
  if(row == 0)
  {
    lastLine = lastLine1;
  }
  else
  {
    lastLine = lastLine2;
  }
  for(int i = 0 ; i < 16; i++)
  {
    char newChar = newText[i];
    if(newChar == '\0')
    {
      newChar = ' ';
    }
    if(lastLine[i] != newChar)
    {
      lcd.setCursor(i , row);
      lcd.write(newChar);
      lastLine[i] = newChar;
    }
  }
  lastLine[16] = '\0';
}

//Displaying Battery Status
void displayBatteryStatus()
{
  char line1[17];
  char line2[17];

  snprintf(line1, sizeof(line1), "BATTERY STATUS");
  if(batteryStatus == 0)
  {
    snprintf(line2, sizeof(line2), "SOC: %3d%% BAL", (int)averageSoC);
  }
  else
  {
    snprintf(line2, sizeof(line2), "SOC: %3d%% IMBAL", (int)averageSoC);
  }
  updateLCDline(0, line1);
  updateLCDline(1, line2);
}

//Displaying System State
void displaySystemState()
{
  char line1[17];
  char line2[17];

  snprintf(line1, sizeof(line1), "SYSTEM STATUS");
  if(relayState == NORMAL)
  {
    snprintf(line2, sizeof(line2), "NORMAL RELAY:OFF");
  }
  else if(relayState == FAULT)
  {
    snprintf(line2, sizeof(line2), "FAULT RELAY:ON");
  }
  else
  {
    snprintf(line2, sizeof(line2), "RECOVERYRELAY:ON");
  }
  updateLCDline(0,line1);
  updateLCDline(1,line2);
}

//Displaying Fault
void displayFaultScreen()
{
  char line1[17];
  char line2[17];

  if(faultID == OVER_VOLTAGE_FAULT)
  {
    snprintf(line1, sizeof(line1), "OVER VOLTAGE   ");
  }
  else if(faultID == UNDER_VOLTAGE_FAULT)
  {
    snprintf(line1, sizeof(line1), "UNDER VOLTAGE  ");
  }
  else if(faultID == VOLTAGE_JUMP_FAULT)
  {
    snprintf(line1, sizeof(line1), "VOLTAGE JUMP   ");
  }
  else if(faultID == FROZEN_ADC_FAULT)
  {
    snprintf(line1, sizeof(line1), "FROZEN SENSOR  ");
  }
  else if(faultID == OUT_OF_RANGE_FAULT)
  {
    snprintf(line1, sizeof(line1), "OUT OF RANGE   ");
  }
  else
  {
    snprintf(line1, sizeof(line1), "FAULT          ");
  }
  snprintf(line2, sizeof(line2), "CELL %d        ", faultCell + 1);

  updateLCDline(0,line1);
  updateLCDline(1,line2);
}

//TELEMETRY STATUS
void displayTelemetryStatus()
{
  char line1[17];
  char line2[17];

  snprintf(line1, sizeof(line1), "TELEMETRY      ");
  snprintf(line2, sizeof(line2), "W:%.2f S:%.2f  ", weakestVoltage, strongestVoltage);

  updateLCDline(0,line1);
  updateLCDline(1,line2);
}

void analyzeBattery()
{
  weakestCell = 0;
  strongestCell = 0;

  weakestVoltage = cellVoltage[0];
  strongestVoltage = cellVoltage[0];

  for(int i = 1; i < NUM_CELLS; i++)
  {
    if(cellVoltage[i] < weakestVoltage)
    {
      weakestVoltage = cellVoltage[i];
      weakestCell = i;
    }
    if(cellVoltage[i] > strongestVoltage)
    {
      strongestVoltage = cellVoltage[i];
      strongestCell = i;
    }
  }
  voltageImbalance = strongestVoltage - weakestVoltage;

  if(firstReading == 1)
  {
    imbalanceTrend = 0;
    previousImbalance = voltageImbalance;
    firstReading = 0;
  }
  else
  {
    if(previousImbalance < voltageImbalance)
    {
      imbalanceTrend = 1; //Increasing Voltage Imbalance
    }
    else if(previousImbalance > voltageImbalance)
    {
     imbalanceTrend = 2; //Decreasing Voltage Imbalance
    }
    else
    {
     imbalanceTrend = 0; //Stable Voltage Imbalance
    }
    previousImbalance = voltageImbalance;
  }

  for(int i = 0; i < NUM_CELLS; i++)
  {
    cellSoC[i] = getSoC(cellVoltage[i]);
  }
  //Adaptive Imbalance Threshold Logic
  averageSoC = 0;
  for(int i = 0; i < NUM_CELLS; i++)
  {
    averageSoC = averageSoC + cellSoC[i];
  }
  averageSoC = averageSoC / NUM_CELLS;

  if(averageSoC > HIGH_SOC_LIMIT)
  {
    adaptiveThreshold = HIGH_SOC_THRESHOLD;
  }
  else if(averageSoC < LOW_SOC_LIMIT)
  {
    adaptiveThreshold = LOW_SOC_THRESHOLD;
  }
  else
  {
    adaptiveThreshold = NORMAL_SOC_THRESHOLD;
  }

  if(voltageImbalance > adaptiveThreshold)
  {
    batteryStatus = 1; //IMBALANCED
  }
  else
  {
    batteryStatus = 0; //BALANCED
  }
}

// ADD EVENT TO OFFLINE QUEUE
void  enqueueTelemetryEvent(
  TelemetryEventType type,
  float value,
  int intValue,
  int cellIndex,
  const char *text
)
{
  // Queue full
  if(queueCount >= OFFLINE_QUEUE_SIZE)
  {
    Serial.println("OFFLINE QUEUE FULL - OLDEST EVENT DROPPED");

    // Move head forward to discard oldest event
    queueHead++;

    if(queueHead >= OFFLINE_QUEUE_SIZE)
    {
      queueHead = 0;
    }

    queueCount--;
  }

  TelemetryEvent &event = offlineQueue[queueTail];

  event.type = type;
  event.timestamp = millis();
  event.cellIndex = cellIndex;
  event.value = value;
  event.intValue = intValue;

  if(text != NULL)
  {
    strncpy(event.text, text, sizeof(event.text) - 1);
    event.text[sizeof(event.text) - 1] = '\0';
  }
  else
  {
    event.text[0] = '\0';
  }

  queueTail++;

  if(queueTail >= OFFLINE_QUEUE_SIZE)
  {
    queueTail = 0;
  }

  queueCount++;

  Serial.print("EVENT QUEUED | TYPE: ");
  Serial.print(type);
  Serial.print(" | QUEUE DEPTH: ");
  Serial.println(queueCount);
}

int getOfflineQueueDepth()
{
  return queueCount;
}
bool isOfflineQueueEmpty()
{
  return queueCount == 0;
}

// SEND ONE QUEUED EVENT TO BLYNK
void sendQueuedEvent(TelemetryEvent &event)
{
  if(!Blynk.connected())
  {
    return;
  }

  switch(event.type)
  {
    case EVENT_CELL_VOLTAGE:

      Blynk.virtualWrite(
        V0 + event.cellIndex,
        event.value
      );

      break;


    case EVENT_RELAY_STATE:

      Blynk.virtualWrite(
        V6,
        event.text
      );

      break;


    case EVENT_FAULT_STATE:

      Blynk.virtualWrite(
        V7,
        event.text
      );

      break;


    case EVENT_SYSTEM_STATE:

      Blynk.virtualWrite(
        V8,
        event.text
      );

      break;


    case EVENT_RSSI:

      Blynk.virtualWrite(
        V9,
        event.intValue
      );

      break;
  }

  Serial.print("QUEUED EVENT SENT | TYPE: ");
  Serial.print(event.type);
  Serial.print(" | ORIGINAL TIME: ");
  Serial.print(event.timestamp);
  Serial.println(" ms");
}

// REPLAY OFFLINE QUEUE
void replayOfflineQueue()
{
  if(!Blynk.connected())
  {
    return;
  }

  if(queueCount == 0)
  {
    return;
  }

  Serial.print("REPLAYING OFFLINE QUEUE | DEPTH: ");
  Serial.println(queueCount);

  while(queueCount > 0 && Blynk.connected())
  {
    TelemetryEvent event = offlineQueue[queueHead];

    sendQueuedEvent(event);

    queueHead++;

    if(queueHead >= OFFLINE_QUEUE_SIZE)
    {
      queueHead = 0;
    }

    queueCount--;

    Serial.print("QUEUE REMAINING: ");
    Serial.println(queueCount);
  }

  Serial.println("OFFLINE QUEUE REPLAY COMPLETE");
}


// BATTERY RISK ANALYSIS
void calculateRiskAnalysis()
{
  int riskScore = 0;


  // 1. VOLTAGE IMBALANCE RISK
  // Maximum contribution: 40 points

  int imbalanceRisk = (int)((voltageImbalance / 0.20) * 40.0);

  imbalanceRisk = constrain(imbalanceRisk, 0, 40);

  riskScore += imbalanceRisk;


  // 2. IMBALANCE TREND
  // Maximum contribution: 15 points

  if(imbalanceTrend == 1)       // Increasing
  {
    riskScore += 15;
  }
  else if(imbalanceTrend == 0)  // Stable
  {
    riskScore += 5;
  }


  // 3. FAULT ACTIVITY
  // Maximum contribution: 20 points

  int faultRisk = faultEventCount * 5;

  faultRisk = constrain(faultRisk, 0, 20);

  riskScore += faultRisk;


  // 4. STATE OF CHARGE
  // Maximum contribution: 20 points

  if(averageSoC < LOW_SOC_LIMIT)
  {
    riskScore += 20;
  }
  else if(averageSoC > HIGH_SOC_LIMIT)
  {
    riskScore += 10;
  }


  // 5. SYSTEM STATE
  // Maximum contribution: 20 points

  if(systemState == STATE_DEGRADED)
  {
    riskScore += 10;
  }
  else if(systemState == STATE_FAILSAFE)
  {
    riskScore += 20;
  }
  else if(systemState == STATE_SHUTDOWN)
  {
    riskScore += 20;
  }


  // Keep score between 0 and 100
  compositeRiskScore = constrain(riskScore, 0, 100);


  // BATTERY HEALTH CLASSIFICATION

  if(compositeRiskScore < 25)
  {
    batteryHealth = "HEALTHY";
  }
  else if(compositeRiskScore < 50)
  {
    batteryHealth = "WATCH";
  }
  else if(compositeRiskScore < 75)
  {
    batteryHealth = "DEGRADED";
  }
  else
  {
    batteryHealth = "CRITICAL";
  }


  // MAINTENANCE RECOMMENDATION

  if(systemState == STATE_SHUTDOWN)
  {
    maintenanceRecommendation = "System shutdown - inspect immediately";
  }
  else if(faultID == RELAY_MISMATCH_FAULT)
  {
    maintenanceRecommendation = "Inspect relay and feedback";
  }
  else if(faultID == ADC_FAILURE_FAULT ||
          faultID == FROZEN_ADC_FAULT)
  {
    maintenanceRecommendation = "Inspect ADC and voltage sensors";
  }
  else if(faultID == COMMUNICATION_FAULT)
  {
    maintenanceRecommendation = "Check communication link";
  }
  else if(faultID == OVER_VOLTAGE_FAULT ||
          faultID == UNDER_VOLTAGE_FAULT)
  {
    maintenanceRecommendation = "Inspect affected battery cell";
  }
  else if(voltageImbalance > adaptiveThreshold)
  {
    maintenanceRecommendation = "Monitor cell voltage imbalance";
  }
  else if(imbalanceTrend == 1)
  {
    maintenanceRecommendation = "Monitor increasing imbalance";
  }
  else
  {
    maintenanceRecommendation = "System operating normally";
  }
}

void sendBlynkTelemetry()
{
  unsigned long now = millis();

  if(now - lastBlynkUpdate < BLYNK_SEND_INTERVAL)
  {
    return;
  }

  lastBlynkUpdate = now;

  bool online = Blynk.connected();
  // =====================================================
// PERIODIC GRAPH DATA
// =====================================================

if(online && now - lastGraphUpdate >= GRAPH_UPDATE_INTERVAL)
{
  lastGraphUpdate = now;

  for(int i = 0; i < NUM_CELLS; i++)
  {
    Blynk.virtualWrite(V0 + i, cellVoltage[i]);
  }
}

  // =====================================================
  // CELL VOLTAGES
  // V0 = Cell 1
  // V1 = Cell 2
  // V2 = Cell 3
  // V3 = Cell 4
  // =====================================================

  for(int i = 0; i < NUM_CELLS; i++)
  {
    if(lastCellVoltage[i] < 0 ||
       fabs(cellVoltage[i] - lastCellVoltage[i]) >= EVENT_VOLTAGE_CHANGE)
    {
      if(online)
      {
        Blynk.virtualWrite(V0 + i, cellVoltage[i]);
      }
      else
      {
        enqueueTelemetryEvent(
          EVENT_CELL_VOLTAGE,
          cellVoltage[i],
          0,
          i,
          ""
        );
      }

      lastCellVoltage[i] = cellVoltage[i];
    }
  }


  // =====================================================
  // WEAKEST CELL
  // V4
  // =====================================================

  if(online)
  {
    Blynk.virtualWrite(V4, weakestVoltage);
  }


  // =====================================================
  // STRONGEST CELL
  // V5
  // =====================================================

  if(online)
  {
    Blynk.virtualWrite(V5, strongestVoltage);
  }


  // =====================================================
  // RELAY STATE
  // V6
  // =====================================================

  if(relayState != lastRelayState)
  {
    const char* relayText;

    if(relayState == NORMAL)
    {
      relayText = "NORMAL";
    }
    else if(relayState == FAULT)
    {
      relayText = "FAULT";
    }
    else
    {
      relayText = "RECOVERY";
    }

    if(online)
    {
      Blynk.virtualWrite(V6, relayText);
    }
    else
    {
      enqueueTelemetryEvent(
        EVENT_RELAY_STATE,
        0,
        relayState,
        -1,
        relayText
      );
    }

    lastRelayState = relayState;
  }


  // =====================================================
  // FAULT STATE
  // V7
  // =====================================================

  if(faultDetected != lastFaultDetected ||
     faultID != lastBlynkFaultID)
  {
    const char* faultText;

    if(faultDetected == 0)
    {
      faultText = "NO FAULT";
    }
    else
{
  switch(faultID)
  {
    case OVER_VOLTAGE_FAULT:
      faultText = "OVER VOLTAGE";
      break;

    case UNDER_VOLTAGE_FAULT:
      faultText = "UNDER VOLTAGE";
      break;

    case VOLTAGE_JUMP_FAULT:
      faultText = "VOLTAGE JUMP";
      break;

    case FROZEN_ADC_FAULT:
      faultText = "FROZEN ADC";
      break;

    case OUT_OF_RANGE_FAULT:
      faultText = "OUT OF RANGE";
      break;

    case RELAY_MISMATCH_FAULT:
      faultText = "RELAY MISMATCH";
      break;

    case COMMUNICATION_FAULT:
      faultText = "COMMUNICATION";
      break;

    case ADC_FAILURE_FAULT:
      faultText = "ADC FAILURE";
      break;

    default:
      faultText = "NO FAULT";
      break;
  }
}

    if(online)
    {
      Blynk.virtualWrite(V7, faultText);
    }
    else
    {
      enqueueTelemetryEvent(
        EVENT_FAULT_STATE,
        0,
        faultDetected,
        faultCell,
        faultText
      );
    }

    lastFaultDetected = faultDetected;
    lastBlynkFaultID = faultID;
  }


  // =====================================================
  // SYSTEM STATE
  // V8
  // =====================================================

  if(systemState != lastSystemState)
  {
    const char* systemText = getSystemStateName(systemState);

    if(online)
    {
      Blynk.virtualWrite(V8, systemText);
    }
    else
    {
      enqueueTelemetryEvent(
        EVENT_SYSTEM_STATE,
        0,
        systemState,
        -1,
        systemText
      );
    }

    lastSystemState = systemState;
  }


  // =====================================================
  // WIFI RSSI
  // V9
  // =====================================================

  if(WiFi.status() == WL_CONNECTED)
  {
    int currentRSSI = WiFi.RSSI();

    if(lastRSSI == -1000 ||
       abs(currentRSSI - lastRSSI) >= EVENT_RSSI_CHANGE)
    {
      if(online)
      {
        Blynk.virtualWrite(V9, currentRSSI);
      }
      else
      {
        enqueueTelemetryEvent(
          EVENT_RSSI,
          0,
          currentRSSI,
          -1,
          ""
        );
      }

      lastRSSI = currentRSSI;
    }
  }


  // =====================================================
  // QUEUE DEPTH
  // V10
  // =====================================================

  if(online)
  {
    Blynk.virtualWrite(V10, getOfflineQueueDepth());
  }


  // =====================================================
  // TELEMETRY STATUS
  // V11
  // =====================================================

  if(online)
  {
    Blynk.virtualWrite(V11, "LIVE");
  }


  // =====================================================
  // FAULT ID
  // V12
  // =====================================================

  if(online)
  {
    if(faultDetected == 0)
    {
      Blynk.virtualWrite(V12, "NO_FAULT");
    }
    else
    {
      switch(faultID)
      {
        case OVER_VOLTAGE_FAULT:
          Blynk.virtualWrite(V12, "OVER_VOLTAGE");
          break;

        case UNDER_VOLTAGE_FAULT:
          Blynk.virtualWrite(V12, "UNDER_VOLTAGE");
          break;

        case VOLTAGE_JUMP_FAULT:
          Blynk.virtualWrite(V12, "VOLTAGE_JUMP");
          break;

        case FROZEN_ADC_FAULT:
          Blynk.virtualWrite(V12, "FROZEN_ADC");
          break;

        case OUT_OF_RANGE_FAULT:
          Blynk.virtualWrite(V12, "OUT_OF_RANGE");
          break;

        case RELAY_MISMATCH_FAULT:
          Blynk.virtualWrite(V12, "RELAY_MISMATCH");
          break;

        case COMMUNICATION_FAULT:
          Blynk.virtualWrite(V12, "COMMUNICATION");
          break;

        case ADC_FAILURE_FAULT:
          Blynk.virtualWrite(V12, "ADC_FAILURE");
          break;

        default:
          Blynk.virtualWrite(V12, "UNKNOWN");
          break;
      }
    }
  }


  // =====================================================
  // VOLTAGE IMBALANCE
  // V13
  // =====================================================

  if(online)
  {
    Blynk.virtualWrite(V13, voltageImbalance);
  }
 // =====================================================
// COMPOSITE RISK SCORE
// V14
// =====================================================

if(online)
{
  Blynk.virtualWrite(V14, compositeRiskScore);
}


// =====================================================
// BATTERY HEALTH
// V15
// =====================================================

if(online)
{
  Blynk.virtualWrite(V15, batteryHealth);
}


// =====================================================
// MAINTENANCE RECOMMENDATION
// V16
// =====================================================

if(online)
{
  Blynk.virtualWrite(V16, maintenanceRecommendation);
}


// =====================================================
// UPTIME
// V17
// =====================================================

if(online)
{
  Blynk.virtualWrite(V17, millis() / 1000);
}


// =====================================================
// FAULT COUNT
// V18
// =====================================================

if(online)
{
  Blynk.virtualWrite(V18, faultEventCount);
}


// =====================================================
// OPERATOR RECOMMENDATION
// V19
// =====================================================

if(online)
{
  Blynk.virtualWrite(V19, operatorRecommendation);
}


// =====================================================
// UPTIME
// V20
// =====================================================

if(online)
{
  unsigned long uptimeSeconds = millis() / 1000;
  Blynk.virtualWrite(V20, uptimeSeconds);
}


// =====================================================
// STATE SEVERITY
// V21
// =====================================================

if(online)
{
  int severity = 0;

  if(systemState == STATE_NORMAL)
  {
    severity = 0;
  }
  else if(systemState == STATE_DEGRADED)
  {
    severity = 1;
  }
  else if(systemState == STATE_FAILSAFE)
  {
    severity = 2;
  }
  else if(systemState == STATE_SHUTDOWN)
  {
    severity = 3;
  }

  Blynk.virtualWrite(V21, severity);
}
}

void updateConnectionState()
{
  unsigned long now = millis();

  switch(connectionState)
  {
    case WIFI_START:

      Serial.println("WIFI: STARTING CONNECTION");

      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, pass);

      wifiConnectStart = now;
      connectionState = WIFI_CONNECTING;

      break;


    case WIFI_CONNECTING:

      if(WiFi.status() == WL_CONNECTED)
      {
        wifiConnected = 1;

        Serial.println("WIFI: CONNECTED");
        Serial.print("IP ADDRESS: ");
        Serial.println(WiFi.localIP());

        connectionState = WIFI_CONNECTED;
      }
      else if(now - wifiConnectStart >= WIFI_CONNECT_TIMEOUT)
      {
        Serial.println("WIFI: CONNECTION TIMEOUT");

        WiFi.disconnect();

        lastWifiAttempt = now;
        connectionState = CONNECTION_OFFLINE;
      }

      break;


    case WIFI_CONNECTED:

      if(WiFi.status() != WL_CONNECTED)
      {
        wifiConnected = 0;
        blynkConnected = 0;

        Serial.println("WIFI: CONNECTION LOST");

        connectionState = CONNECTION_OFFLINE;
      }
      else
      {
        // Attempt Blynk connection without blocking
       if(!Blynk.connected())
{
  if(now - lastBlynkAttempt >= BLYNK_RETRY_INTERVAL)
  {
    lastBlynkAttempt = now;

    Serial.println("BLYNK: CONNECTING...");

    Blynk.config(BLYNK_AUTH_TOKEN);

    bool result = Blynk.connect(5000);

    if(result)
    {
      blynkConnected = 1;
      connectionState = BLYNK_CONNECTED;

      Serial.println("BLYNK: CONNECTED");
    }
    else
    {
      blynkConnected = 0;

      Serial.println("BLYNK: CONNECTION FAILED");
    }
  }
}
      }

      break;


    case BLYNK_CONNECTED:

      if(WiFi.status() != WL_CONNECTED)
      {
        wifiConnected = 0;
        blynkConnected = 0;

        Serial.println("WIFI: CONNECTION LOST");

        connectionState = CONNECTION_OFFLINE;
      }
      else if(!Blynk.connected())
      {
        blynkConnected = 0;

        Serial.println("BLYNK: CONNECTION LOST");

        connectionState = WIFI_CONNECTED;
      }

      break;


    case CONNECTION_OFFLINE:

      wifiConnected = 0;
      blynkConnected = 0;

      if(now - lastWifiAttempt >= WIFI_RETRY_INTERVAL)
      {
        lastWifiAttempt = now;

        Serial.println("WIFI: RETRYING...");

        WiFi.begin(ssid, pass);

        wifiConnectStart = now;
        connectionState = WIFI_CONNECTING;
      }

      break;
  }

  // Connection transition logging

  if(wifiConnected != previousWifiConnected)
  {
    Serial.print("WIFI STATUS: ");

    if(wifiConnected)
    {
      Serial.println("ONLINE");
    }
    else
    {
      Serial.println("OFFLINE");
    }

    previousWifiConnected = wifiConnected;
  }

  if(blynkConnected != previousBlynkConnected)
  {
    Serial.print("BLYNK STATUS: ");

    if(blynkConnected)
    {
      Serial.println("ONLINE");
    }
    else
    {
      Serial.println("OFFLINE");
    }

    previousBlynkConnected = blynkConnected;
  }
}

void loop()
{
  updateConnectionState();

  if(Blynk.connected())
{
  Blynk.run();
  replayOfflineQueue();
}

  sendBlynkTelemetry();

 unsigned long currentTime = millis();
 if(currentTime - lastSampleTime >= SAMPLE_INTERVAL)
 {
   lastSampleTime = currentTime;

   for(int i = 0; i < NUM_CELLS; i++)
  {
    cellVoltage[i] = readVoltage(cell[i]);
  }
   if(firstSensorReading == 1)
  {
    for(int i = 0; i < NUM_CELLS; i++)
    {
      previousVoltage[i] = cellVoltage[i];
    }
    firstSensorReading = 0;
  }
  analyzeBattery();
  checkVoltageJumps();
  updateVoltageHistory();
  checkFrozenSensors();
  checkRelayMismatch();
  detectFaults();
  // Count only a NEW fault event
if(faultDetected == 1 && previousFaultDetected == 0)
{
  faultEventCount++;

  Serial.print("FAULT EVENT COUNT: ");
  Serial.println(faultEventCount);
}

previousFaultDetected = faultDetected;

  checkShutdownCommand();
  updateSystemState();
  controlRelay();
  calculateRiskAnalysis();

  if (faultDetected == 1)
  {
   printLastFault(); 
  }
  printFault();
  printSystemStatus();
 }
 unsigned long displayTime = millis();  
 if(startupComplete == 0)
 {
  if(displayTime - startupTime >= STARTUP_INTERVAL)
  {
    startupComplete = 1;
    lcd.clear();
    strcpy(lastLine1, "                ");
    strcpy(lastLine2, "                ");
    lastPageChange = displayTime;
    lastDisplayCheck = displayTime;
  }
 }
 if(startupComplete == 1)
 {
  if(displayTime - lastPageChange >= PAGE_INTERVAL)
  {
    lastPageChange = displayTime;
    currentPage++;
    if(currentPage >= 3)
    {
      currentPage = 0;
    }
  }
   if(displayTime - lastDisplayCheck >= DISPLAY_INTERVAL)
  {
    lastDisplayCheck = displayTime;
    if(faultDetected == 1 || relayState == FAULT)
    {
      displayFaultScreen();
    }
    else
    {
      if(currentPage == 0)
      {
        displayBatteryStatus();
      }
      else if(currentPage == 1)
      {
        displaySystemState();
      }
      else if(currentPage == 2)
      {
        displayTelemetryStatus();
      }
    }  
  }
 }
 static int lastFeedback = -1;

 //Relay Feedback Output
int feedbackTest = digitalRead(RELAY_FEEDBACK);

if(feedbackTest != lastFeedback)
{
  Serial.print("GPIO 18 DIRECT: ");
  Serial.println(feedbackTest);
  lastFeedback = feedbackTest;
}
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA,SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("BMS SAFETY SYS ");
  lcd.setCursor(0,1);
  lcd.print("INITIALISING   ");
  strcpy(lastLine1, "                ");
  strcpy(lastLine2, "                ");
  startupTime = millis();
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);
  pinMode(RELAY_FEEDBACK, INPUT_PULLDOWN);
  relayState = NORMAL;
  for(int i = 0; i < NUM_CELLS; i++)
  {
    previousVoltage[i] = 0;
    jumpDetected[i] = 0;
    frozenDetected[i] = 0;
    confirmedJump[i] = 0;
    confirmedFrozen[i] = 0;
  }
}
