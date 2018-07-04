// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Please use an Arduino IDE 1.6.8 or greater

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>

#include "config.h"

static bool messagePending = false;
static bool messageSending = true;

static char *connectionString;
static char *ssid;
static char *pass;

static int interval = INTERVAL;

void initSensors();

//Carrega a biblioteca do sensor ultrassonico
 
//Define os pinos para o trigger e echo
#define external_trigger D3
#define external_echo D2
#define internal_trigger D1
#define internal_echo D5
#define setup_iterations 30
#define threshold 0.30

int ignore_message;

float avg_external_distance,avg_internal_distance;
long time_internal, time_external;
boolean acquired_external, acquired_internal;
int people_in, people_out;

void blinkLED()
{
   
}

void initWifi()
{
    // Attempt to connect to Wifi network:
    Serial.printf("Attempting to connect to SSID: %s.\r\n", ssid);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        // Get Mac Address and show it.
        // WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The huzzah board should
        // start from mac[0] to mac[5], but some other kinds of board run in the oppsite direction.
        uint8_t mac[6];
        WiFi.macAddress(mac);
        Serial.printf("You device with MAC address %02x:%02x:%02x:%02x:%02x:%02x connects to %s failed! Waiting 10 seconds to retry.\r\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ssid);
        WiFi.begin(ssid, pass);
        delay(10000);
    }
    Serial.printf("Connected to wifi %s.\r\n", ssid);
}

void initTime()
{
    time_t epochTime;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    while (true)
    {
        epochTime = time(NULL);

        if (epochTime == 0)
        {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
            break;
        }
    }
}

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
void setup()
{
  ignore_message=0;
  initSensors();

    initSerial();
    delay(2000);
    readCredentials();
    

    initWifi();
    initTime();
    initSensor();
    
    /*
    * Break changes in version 1.0.34: AzureIoTHub library removed AzureIoTClient class.
    * So we remove the code below to avoid compile error.
    */
    // initIoThubClient();

    iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
    if (iotHubClientHandle == NULL)
    {
        Serial.println("Failed on IoTHubClient_CreateFromConnectionString.");
        while (1);
    }

    IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, NULL);
    IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, NULL);
    IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, twinCallback, NULL);

    acquired_external = false;
  acquired_internal = false;

  people_in = 0;
  people_out = 0;

  setDistances();
  delay(8000);

  Serial.print("Distancia média sensor externo: ");
  Serial.println(avg_external_distance);
  Serial.print("Distancia média sensor interno: ");
  Serial.println(avg_internal_distance);
}

static int messageCount = 1;
void loop()
{    
  boolean external_status, internal_status;
  external_status = external_sensor_status();
  internal_status = internal_sensor_status();
  evaluate(external_status, internal_status);
  if(ignore_message % 200 == 0)
    if (!messagePending && messageSending)
    {
        char messagePayload[MESSAGE_MAX_LEN];
        bool temperatureAlert = readMessage(messageCount, messagePayload);
        sendMessage(iotHubClientHandle, messagePayload, temperatureAlert);
        messageCount++;
    }
    ignore_message = ignore_message + 1;
    IoTHubClient_LL_DoWork(iotHubClientHandle);
    delay(10);
}



void setDistances(){
  Serial.println("Lendo dados do sensor...");
  avg_external_distance = 0;
  avg_internal_distance = 0;
  float cmMsec = 0;
 
  for(int i=1;i<=setup_iterations; i = i + 1 ){
    cmMsec = external_distance();
    avg_external_distance = ((i-1)*avg_external_distance + cmMsec)/i;
  }

  for(int i=1;i<=setup_iterations; i = i + 1 ){
    cmMsec = internal_distance();
    avg_internal_distance = ((i-1)*avg_internal_distance + cmMsec)/i;
  }
}

void print_distances(){
  float ext_distance, int_distance;
  ext_distance = external_distance();
  int_distance = internal_distance();

  Serial.print(ext_distance);
  Serial.print("\t");
  Serial.println(int_distance);
}

void print_status(boolean external_status, boolean internal_status){
  if(external_status)Serial.print("External was detected\t");
  else Serial.print("External not detected\t");
  Serial.print(time_external);
  if(internal_status)Serial.print("\tInternal was detected\t");
  else Serial.print("\tInternal not detected\t");
  Serial.print(time_internal);
  Serial.print("\t");
  Serial.print(people_in);
  Serial.print("\t");
  Serial.print(people_out);
  Serial.print("\t");
  Serial.println(people_in-people_out);
  
}

boolean external_sensor_status(){
  float cmMsec = external_distance();

  return cmMsec < avg_external_distance - (threshold) * avg_external_distance;
}

boolean internal_sensor_status(){
  float cmMsec = internal_distance();

  return cmMsec < avg_internal_distance - (threshold) * avg_internal_distance;
}

void evaluate(boolean external_status, boolean internal_status){
  if (external_status) {
    if(!acquired_external){
      acquired_external=true;
      time_external = millis();
    }
  }

  if (internal_status) {
    if(!acquired_internal){
      acquired_internal = true;
      time_internal = millis();
    }
  }

  if(acquired_internal and acquired_external){
    long diff = time_external - time_internal;
    if(diff < 0){
      people_in = people_in + 1;
      Serial.println("Entered");
    }else if(diff > 0){
      people_out = people_out + 1;  
      Serial.println("Quited");
    }
    reset_sensors();   
  }
}

void reset_sensors(){
  Serial.println("Reseting sensors....");
  boolean external_status, internal_status;
  do{
    delay(300);
    external_status = external_sensor_status();
    internal_status = internal_sensor_status();
    Serial.println("Attempting reset....");
  }while(external_status or internal_status);
  acquired_internal = false;
  acquired_external = false;
  Serial.println("Sensors reseted....");
  delay(300);
}

float external_distance(){
  return get_distance(external_trigger, external_echo);
}

float internal_distance(){
  return get_distance(internal_trigger, internal_echo);
}

float get_distance(int trigPin, int echoPin){
  float duration;
  digitalWrite(trigPin, LOW);
  delayMicroseconds (2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds (10);
  digitalWrite (trigPin,LOW);
  duration = (float)pulseIn (echoPin, HIGH);
  return (duration/2) /29.1;
}

void initSensors()
{
  pinMode(external_trigger, OUTPUT);
  pinMode(external_echo, INPUT);
  pinMode(internal_trigger, OUTPUT);
  pinMode(internal_echo, INPUT);         
}
