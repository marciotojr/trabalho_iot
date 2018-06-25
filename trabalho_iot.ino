 
//Carrega a biblioteca do sensor ultrassonico
#include <Ultrasonic.h>
 
//Define os pinos para o trigger e echo
#define external_trigger D3
#define external_echo D2
#define internal_trigger D1
#define internal_echo D0
#define setup_iterations 30
#define threshold 0.20
 
//Inicializa o sensor nos pinos definidos acima
Ultrasonic external_sensor(external_trigger, external_echo);
Ultrasonic internal_sensor(internal_trigger, internal_echo);

float avg_external_distance,avg_internal_distance;
long time_internal, time_external;
boolean acquired_external, acquired_internal;
int entries, exits;


void setup()
{
  Serial.begin(9600);
  Serial.println("Lendo dados do sensor...");
  avg_external_distance = 0;
  long microsec = external_sensor.timing();
  float cmMsec = external_sensor.convert(microsec, Ultrasonic::CM);
 
  for(int i=1;i<=setup_iterations; i = i + 1 ){
    microsec = external_sensor.timing();
    cmMsec = external_sensor.convert(microsec, Ultrasonic::CM);
    avg_external_distance = ((i-1)*avg_external_distance + cmMsec)/i;
  }

  for(int i=1;i<=setup_iterations; i = i + 1 ){
    microsec = internal_sensor.timing();
    cmMsec = internal_sensor.convert(microsec, Ultrasonic::CM);
    avg_internal_distance = ((i-1)*avg_internal_distance + cmMsec)/i;
  }
  acquired_external = false;
  acquired_internal = false;

  entries = 0;
  exits = 0;
  
  Serial.print("Distancia média sensor externo: ");
  Serial.println(avg_external_distance);
  Serial.print("Distancia média sensor interno: ");
  Serial.println(avg_internal_distance);
}
 
void loop()
{
  boolean external_status, internal_status;
  external_status = external_sensor_status();
  internal_status = internal_sensor_status();

  print_distances();
  //print_status(external_status, internal_status);
 
  evaluate(external_status, internal_status);

  delay(100);
}

void print_distances(){
  float external_distance, internal_distance;
  long microsec = external_sensor.timing();
  external_distance = external_sensor.convert(microsec, Ultrasonic::CM);
  microsec = internal_sensor.timing();
  internal_distance = internal_sensor.convert(microsec, Ultrasonic::CM);

  Serial.print(external_distance);
  Serial.print("\t");
  Serial.println(internal_distance);
}

void print_status(boolean external_status, boolean internal_status){
  if(external_status)Serial.print("External was detected\t");
  else Serial.print("External not detected\t");
  Serial.print(time_external);
  if(internal_status)Serial.print("\tInternal was detected\t");
  else Serial.print("\tInternal not detected\t");
  Serial.print(time_internal);
  Serial.print("\t");
  Serial.print(entries);
  Serial.print("\t");
  Serial.print(exits);
  Serial.print("\t");
  Serial.println(entries-exits);
  
}

boolean external_sensor_status(){
  long microsec = external_sensor.timing();
  float cmMsec = external_sensor.convert(microsec, Ultrasonic::CM);

  return cmMsec < avg_external_distance - (threshold) * avg_external_distance;
}

boolean internal_sensor_status(){
  long microsec = internal_sensor.timing();
  float cmMsec = internal_sensor.convert(microsec, Ultrasonic::CM);

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
      entries = entries + 1;
      Serial.println("Entered");
    }else if(diff > 0){
      exits = exits + 1;    
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

