#include <YunClient.h>
#include <Console.h>
#include <Bridge.h>
#include <BridgeServer.h>
#include <Mailbox.h>
#include <BridgeSSLClient.h>
#include <HttpClient.h>
#include <FileIO.h>
#include <Process.h>
#include <BridgeClient.h>
#include <YunServer.h>
#include <SPI.h>
#include <YunClient.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>
#include <BridgeUdp.h>
#include <dht11.h>

// Define necessary variables
#define MQTT_MAX_PACKET_SIZE 100
#define SIZE 100
#define MQTT_PORT 1883
#define PUBLISH_TOPIC "iot-2/evt/status/fmt/json"
#define SUBSCRIBE_TOPIC "iot-2/cmd/+/fmt/json"
#define AUTHMETHOD "use-token-auth"

// Authenticationec
#define CLIENT_ID "d:3gyk83:arduinoyun:Arduino_Yun"
#define MS_PROXY "3gyk83.messaging.internetofthings.ibmcloud.com"
#define AUTHTOKEN "Uo?vI2T(vUNR?&o-NO"

YunClient c;
IPStack ipstack(c);

MQTT::Client<IPStack, Countdown, 100, 1> client = MQTT::Client<IPStack, Countdown, 100, 1>(ipstack);

void messageArrived(MQTT::MessageData& md);

String deviceEvent;
int decider = 0;

void setup() {

  //Default setup
  Bridge.begin();
  Console.begin();
  Serial.begin(9600);

  // Set pins as I/O
  pinMode(7,OUTPUT);
  pinMode(13, OUTPUT);
  delay(1000);
  
  
}

void loop() {

  /* INPUTS */
  
  // Smoke sensor
  int smokePin = 14;
  int smoke = 0;     
  smoke = analogRead(smokePin);     

  // Movement sensor
  int movePin = 15;
  int movement = 0;          
  movement = analogRead(movePin);  

  // Temperature
  int tempPin=7; 
  dht11 tempSensor;
  int chk = tempSensor.read(tempPin);

  
/****************************************************/
  int rc = -1;
  if (!client.isConnected()) {
    Serial.print("Connecting using Registered mode with clientid : ");
    Serial.print(CLIENT_ID);
    Serial.print("\tto MQTT Broker : ");
    Serial.print(MS_PROXY);
    Serial.print("\ton topic : ");
    Serial.println(PUBLISH_TOPIC);
    
    ipstack.connect(MS_PROXY, MQTT_PORT);
    
    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    options.MQTTVersion = 3;
    options.clientID.cstring = CLIENT_ID;
    options.username.cstring = AUTHMETHOD;
    options.password.cstring = AUTHTOKEN;
    options.keepAliveInterval = 10;
    rc = -1;
    while ((rc = client.connect(options)) != 0)
      ;
    //unsubscribe the topic, if it had subscribed it before.
 
    client.unsubscribe(SUBSCRIBE_TOPIC);
    //Try to subscribe for commands
    if ((rc = client.subscribe(SUBSCRIBE_TOPIC, MQTT::QOS0, messageArrived)) != 0) {
            Serial.print("Subscribe failed with return code : ");
            Serial.println(rc);
    } else {
          Serial.println("Subscribed\n");
    }
    Serial.println("Subscription tried......");
    Serial.println("Connected successfully\n");
    Serial.println("Sensor Values");
    Serial.println("____________________________________________________________________________");
  }

  MQTT::Message message;
  message.qos = MQTT::QOS0; 
  message.retained = false;

  /****************************************************/
  
  String smokeJson = "{\"d\":{\"device\":\"Arduino Yun\",\"s\":" + String(smoke) + "   }}";
  String humJson = "{\"d\":{\"device\":\"Arduino Yun\",\"h\":" + (String)tempSensor.humidity + "    }}";
  String tempJson = "{\"d\":{\"device\":\"Arduino Yun\",\"t\":" + (String)tempSensor.temperature + "    }}";
  String dewJson = "{\"d\":{\"device\":\"Arduino Yun\",\"dp\":" + (String)tempSensor.dewPoint() +   "}}";
  String moveJson = "{\"d\":{\"device\":\"Arduino Yun\",\"m\":" + (String)movement + "   }}";

  String json = "";
  
  switch(decider) {
      case 0 :
         json = smokeJson;
         decider += 1;
         break;
      case 1 :
         json = moveJson;
         decider += 1;
         break;
      case 2 :
         json = humJson;
         decider += 1;
         break;
      case 3 :
         json = tempJson;
         decider += 1;
         break;
      case 4 :
         json = dewJson;
         decider += 1;
         break;
      default :
         printf("Invalid message\n" );
  }

  int i;
  char *msg =  (char*)malloc (json.length() * sizeof (char));
  for (i = 0; i < json.length(); i++) {
    msg[i] = json[i]; 
  }

  for (i = 0; i < json.length(); i++) {
    Serial.print(msg[i]);
  }
  Serial.println();
  
  message.payload = msg; 
  message.payloadlen = strlen(msg);
  
  rc = client.publish(PUBLISH_TOPIC, message);
  if (rc != 0) {
    Serial.print("Message publish failed with return code : ");
    Serial.println(rc); 
  }

  if (decider == 5) {
    decider = 0;
    client.yield(5000);
    Serial.println("Waiting...");
  }
  client.yield(2000);
  free(msg);
}

void messageArrived(MQTT::MessageData& md) {
    Serial.print("\nMessage Received\t");
    MQTT::Message &message = md.message;
    int topicLen = strlen(md.topicName.lenstring.data) + 1;

    char * topic = md.topicName.lenstring.data;
    topic[topicLen] = '\0';
    
    int payloadLen = message.payloadlen + 1;

    char * payload = (char*)message.payload;
    payload[payloadLen] = '\0';
    
    String topicStr = topic;
    String payloadStr = payload;
    int payloadInt = *payload;
    Serial.print(payloadInt);
    Serial.print("-");
    Serial.print(1);

    // Lights on - relay IN1
    if (payloadInt == 49) {
      digitalWrite(13, LOW);
    }
    // Lights off - relay IN1
    else if (payloadInt == 50) {
      digitalWrite(13, HIGH);
    }
    
    // Air conditioner on - relay IN2
    if (payloadInt == 51) {
      digitalWrite(12, HIGH);
    }
    // Air conditioner off - relay IN2
    else if (payloadInt == 53) {
      digitalWrite(12, LOW);
    }

    // Curtain on - relay IN3
    if (payloadInt == 54) {
      digitalWrite(11, HIGH);
    }
    // Curtaion off - relay IN3
    else if (payloadInt == 55) {
      digitalWrite(11, LOW);
    }
  /*
    //Command topic: iot-2/cmd/blink/fmt/json
    if(strstr(topic, "/cmd/blink") != NULL) {
      Serial.print("Command IS Supported : ");
      Serial.print(payload);
      Serial.println("\t.....\n");
      

      //Blink
      for(int i = 0 ; i < 2 ; i++ ) {
        digitalWrite(13, HIGH);
        delay(1000);
        digitalWrite(13, LOW);
        delay(1000);
      }

    } else {
      Serial.println("Command Not Supported:");            
    }*/
}
