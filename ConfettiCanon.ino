#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/* MQTT Settings */
const char* mqttTopic1 = "unknownpain/canon/power";  // MQTT Topic for firing the canon
const char* mqttPubPrefix =   "/status";             // MQTT topic for publishing relay state

#define ENABLE_FEEDBACK                              // enables the mqtt status feedback after changing relay state

IPAddress broker(192,168,0,106);                     // Address of the MQTT broker
#define CLIENT_ID "client-canon1"                    // Client ID to send to the broker

char onlineTopic[128];

/* WiFi Settings */
const char* ssid     = "SSID";
const char* password = "PASS";

// uncomment USE_MQTT_AUTH if you want to connect anonym
#define USE_MQTT_AUTH
#define MQTT_USER "mqttUser"
#define MQTT_PASSWORD "password"

const int relayPin = 0;
int relayState = 0;
int desiredRelayState = 0;
int closeDelay = 0;

WiFiClient wificlient;
PubSubClient client(wificlient);

void setup() 
{
  pinMode(relayPin, OUTPUT);
  digitalWrite(0,1);
  
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  reconnectWifi();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /* Prepare MQTT client */
  client.setServer(broker, 1883);
  client.setCallback(mqttMessage);
  
  sprintf(onlineTopic, "%s/%s", mqttTopic1, CLIENT_ID); 
}

void loop() 
{
  if (WiFi.status() != WL_CONNECTED)
  {
    reconnectWifi();
    Serial.println("WIFI");
  }
  else if (!client.connected())
  {
    reconnectMQTT();
    Serial.println("MQTT");
  }
  else  
  {
    client.loop();
    refreshRelay();
    delay(100);
  }
}

void reconnectWifi()
{
  int counter = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {      
    if(counter > 30)
    { 
      // to many connecting attempts.. try to restart
      Serial.println("");
      Serial.println("Restarting...");
      ESP.restart();
    }
    
    delay(500);
  }
}

void reconnectMQTT() 
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting to connect to MQTT broker...");
    // Attempt to connect
#ifdef USE_MQTT_AUTH     
    if (client.connect(CLIENT_ID, MQTT_USER, MQTT_PASSWORD, onlineTopic, 0, true, "#ff0000"))
#else
    if(client.connect(CLIENT_ID))
#endif
    {
      Serial.println("connected to MQTT broker");
      client.subscribe(mqttTopic1);
      publishOnlineState(1);
    } 
    else 
    {
      Serial.print(" Reconnect failed. State=");
      Serial.println(client.state());
      Serial.println("Retry in 3 seconds...");
      // Wait 5 seconds before retrying
      delay(3000);
    }
  }
}

void mqttMessage(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) 
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  
  if (!strcmp(topic, mqttTopic1)) 
  {
    setRelay(0, getDesiredRelayState(payload, length), topic);
  }
  else
  {
    Serial.println("UKNOWN TOPIC");
  }
}

int getDesiredRelayState(byte* payload, unsigned int length)
{
  
    if (!strncasecmp_P((char *)payload, "OFF", length)) 
    {
      return 0;
    }
    else if (!strncasecmp_P((char *)payload, "ON", length)) 
    {
      return 1;
    }
    else if (!strncasecmp_P((char *)payload, "TOGGLE", length))
    {
      return -1;
    }
}

void setRelay(int state)
{
  setRelay(relayPin, state, (char*)mqttTopic1);
}

void setRelay(int pinIndex, int state, char* topic)
{
    if (state == -1)
    {
      state = !relayState;
    }
    
    desiredRelayState = state;
}

void refreshRelay()
{
  if(relayState == 1 && closeDelay < millis())
  {
    desiredRelayState = 0;
  }

  
  
  if(relayState != desiredRelayState)
  {
    digitalWrite(relayPin, !desiredRelayState);
    relayState = desiredRelayState;
    
    #ifdef ENABLE_FEEDBACK
      char mqttPubTopic[128];
      sprintf(mqttPubTopic, "%s%s", mqttTopic1, mqttPubPrefix); 
      
      if(relayState == 1)
      {
        client.publish(mqttPubTopic, "ON");
        publishOnlineState(2);
        closeDelay = millis() + 3000;
      }
      else
      { 
        client.publish(mqttPubTopic, "OFF");
        publishOnlineState(1);
      }
    #endif
  }  
}

void publishOnlineState(int online)
{
  switch(online)
  {
    case 0:
      client.publish(onlineTopic, "#ff0000");
      break;
    case 1:
      client.publish(onlineTopic, "#00ff00");
      break;
    case 2:
      client.publish(onlineTopic, "#ffff00");
      break;
  }
}




