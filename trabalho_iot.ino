#include <ESP8266WiFi.h> // Importa a Biblioteca ESP8266WiFi
#include <PubSubClient.h> // Importa a Biblioteca PubSubClient

//defines:

// ATENCAO: coloque um id unico. Sugestao -> iotGrupo1, iotGrupo2 e assim por diante.                                                    
#define ID_MQTT  "iotGrupo6"     //id mqtt (para identificação de sessão)

#define subscribe_topic "subscribe"


// WIFI
const char* SSID = "GVT-0D0D"; // SSID / nome da rede WI-FI que deseja se conectar
const char* PASSWORD = "9607463381"; // Senha da rede WI-FI que deseja se conectar
 
// MQTT
const char* BROKER_MQTT = "test.mosquitto.org";//URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883; // Porta do Broker MQTT


//Variáveis e objetos globais
WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient
char EstadoSaida = '0';  //variável que armazena o estado atual da saída
 
//Prototypes
void initSerial();
void initWiFi();
void initMQTT();
void reconectWiFi(); 
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void publish(char* publish_topic, String message);
void initSensors();

//Carrega a biblioteca do sensor ultrassonico
 
//Define os pinos para o trigger e echo
#define external_trigger D3
#define external_echo D2
#define internal_trigger D1
#define internal_echo D0
#define setup_iterations 30
#define threshold 0.20

float avg_external_distance,avg_internal_distance;
long time_internal, time_external;
boolean acquired_external, acquired_internal;
int people_in, people_out;


void setup()
{
  initSensors();
  initSerial();
  initWiFi();
  initMQTT();
  
  acquired_external = false;
  acquired_internal = false;

  people_in = 0;
  people_out = 0;

  setDistances();
  publish("people_in", "0");
  publish("people_out", "0");
  publish("current_no_people", "0");
  delay(8000);
  
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
      publish("people_in", String(people_in));
      Serial.println("Entered");
    }else if(diff > 0){
      people_out = people_out + 1;  
      publish("people_out", String(people_out));  
      Serial.println("Quited");
    }
    publish("current_no_people", String(people_in-people_out));
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







//Função: inicializa comunicação serial com baudrate 115200 (para fins de monitorar no terminal serial 
//        o que está acontecendo.
//Parâmetros: nenhum
//Retorno: nenhum
void initSerial() 
{
    Serial.begin(9600);
}

//Função: inicializa e conecta-se na rede WI-FI desejada
//Parâmetros: nenhum
//Retorno: nenhum
void initWiFi() 
{
    delay(10);
    Serial.println("------WI-FI Connection------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
    
    reconectWiFi();
}
 
//Função: inicializa parâmetros de conexão MQTT(endereço do 
//        broker, porta e seta função de callback)
//Parâmetros: nenhum
//Retorno: nenhum
void initMQTT() 
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);   //informa qual broker e porta deve ser conectado
    MQTT.setCallback(mqtt_callback);            //atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
}
 
//Função: função de callback 
//        esta função é chamada toda vez que uma informação de 
//        um dos tópicos subescritos chega)
//Parâmetros: nenhum
//Retorno: nenhum
void mqtt_callback(char* topic, byte* payload, unsigned int length) 
{
    
}
 
//Função: reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
//        em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito.
//Parâmetros: nenhum
//Retorno: nenhum
void reconnectMQTT() 
{
    while (!MQTT.connected()) 
    {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) 
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            MQTT.subscribe(subscribe_topic); 
        } 
        else 
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentatica de conexao em 2s");
            delay(2000);
        }
    }
}
 
//Função: reconecta-se ao WiFi
//Parâmetros: nenhum
//Retorno: nenhum
void reconectWiFi() 
{
    //se já está conectado a rede WI-FI, nada é feito. 
    //Caso contrário, são efetuadas tentativas de conexão
    if (WiFi.status() == WL_CONNECTED)
        return;
        
    WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI
    
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        Serial.print(".");
    }
  
    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
}

//Função: verifica o estado das conexões WiFI e ao broker MQTT. 
//        Em caso de desconexão (qualquer uma das duas), a conexão
//        é refeita.
//Parâmetros: nenhum
//Retorno: nenhum
void VerificaConexoesWiFIEMQTT()
{
    if (!MQTT.connected()) 
        reconnectMQTT(); //se não há conexão com o Broker, a conexão é refeita
    
     reconectWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

//Função: envia ao Broker o estado atual do output 
//Parâmetros: nenhum
//Retorno: nenhum
void publish(char* publish_topic, String message)
{
  MQTT.connect(ID_MQTT);
  char msg[message.length()+1];
  message.toCharArray(msg, message.length()+1);

  MQTT.publish(publish_topic, msg);
}

//Função: inicializa o output em nível lógico baixo
//Parâmetros: nenhum
//Retorno: nenhum
void initSensors()
{
  pinMode(external_trigger, OUTPUT);
  pinMode(external_echo, INPUT);
  pinMode(internal_trigger, OUTPUT);
  pinMode(internal_echo, INPUT);         
}

