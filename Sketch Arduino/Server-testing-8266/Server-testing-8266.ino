#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#ifndef DEBUG_WEBSOCKETS
#define DEBUG_WEBSOCKETS(...)
#define NODEBUG_WEBSOCKETS
#endif


#include <WebSocketsServer.h>

#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "******"
#endif
#define LED_RED D0
#define LED_BLUE D4

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

//Functions prototypes
void sendToSocketClient(uint8_t,  String);
void processAction(int);
String castStringFromPayload(uint8_t*, size_t);
boolean getLedStatus();
String createResponseJson(bool,String);

static const char PROGMEM INDEX_HTML[] = R"===(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=1024, initial-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="ie=edge">
    <title>ESP8266 Test System</title>

    <!--CDNS-->
    <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css" integrity="sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T"
        crossorigin="anonymous">



</head>

<body>
    <div class="container">
        <div class="header">
            <p>Aqui va el header</p>
            <h1>TESTEO PARA EL ESP8266</h1>
        </div>
        <div class="main">
            <div class="row" style="text-align:center">
                <button type="button" class="btn btn-primary" style="border-radius: 50%; width: 300px; height: 300px" onclick="toggleLEd()">Toogle LED</button>
            </div>
            <div class="row">
                <p>LED VALUE: </p>
                <p id="ledValue" style="background: red"></p>
            </div>
               
            <p> el main content</p>
        </div>
        <div class=""></div>
        <p>
            CONTAINER
        </p>
    </div>
    <div class="footer"></div>
</body>

<!--Funcion para el manejo de WebSockets-->
<script>
    var operation = {};
    var ledState = 'OFF';
    var ledValueElement = document.getElementById('ledValue');
    operation.socket = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
    //operation.socket = new WebSocket('ws://' + '192.168.31.66' + ':81/', ['arduino']);

    operation.socket.onopen = function () {
        operation.socket.send('Connected  -  ' + new Date());
    }
    operation.socket.onmessage = function (event) {
        console.log('Server (incomming): ', event.data);
        var payload = JSON.parse(event.data);
        console.log(ledValueElement.innerHTML);
        ledValueElement.innerHTML = payload.status;

        
    }
    operation.socket.onerror = function (error) {
        console.log('WebSocket Error!!!', error);
    }


    function toggleLEd() {
        var ledValueElement = document.getElementById('ledValue');
        var value = ledValueElement.innerText;
        console.log('Capturing value innerText: ',value);
        switch (value) {
            case 'true':
            sendToSocket(false);
                break;
            case 'false':
            sendToSocket(true);
                break;
            default:
            sendToSocket(true);
            break;



        }

    }
    function sendToSocket(value) {
        console.log('Client (sending): ' + value);
        operation.socket.send(value);
    }
</script>


</html>
)===";



void handleRoot() {
  digitalWrite(LED_BLUE, 1);
  delay(5000);
  server.send(200, "text/plain", "hello from esp8266!");
  digitalWrite(LED_BLUE, 0);
}

void indexPage(){
  server.send_P(200, "text/html", INDEX_HTML);
}

void toggleLed(){
  digitalWrite(LED_BLUE,!digitalRead(LED_BLUE));
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  digitalWrite(LED_BLUE, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  } 
  server.send(404, "text/plain", message);
  digitalWrite(LED_BLUE, 0);
}


void serverRouting(){
    server.on("/", handleRoot);
  server.on("/toggle", toggleLed);
  server.on("/testing", indexPage);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("Payload: %s\n", payload);
        
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        
        sendToSocketClient(num,createResponseJson(getLedStatus(),"Connected..."));
             
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] Message text received from client: %s\n", num, payload);
      
      String value = castStringFromPayload(payload, lenght);

      if (value == "true") {  
        
        Serial.print("Payload received ON and taking action...");

        sendToSocketClient(num,createResponseJson(true,"Processing request..."));
        
        processAction(HIGH);
      
        sendToSocketClient(num,"{\"status\": true, \"message\": \"Done...\"}");
      } else if (value == "false") {                      
        
        Serial.print("Payload received OFF and taking action...");
        sendToSocketClient(num,"{\"status\": false, \"message\": \"Processing request...\"}");
        processAction(LOW);
        sendToSocketClient(num,"{\"status\": false, \"message\": \"\"}");
        sendToSocketClient(num,"{\"status\": false, \"message\": \"Done...\"}");
      }
      break;
  }
}

String createResponseJson(bool statusLed,String message){
  DynamicJsonDocument  doc(200);
  String jsonSerialized = "";
  doc["status"] = statusLed;
  doc["message"] = message;
  serializeJson(doc,jsonSerialized);
  return jsonSerialized;
}

void processAction(int action){
  if(action){
    digitalWrite(LED_BLUE, LOW); // inverse because that led are inverted polarity
    
  }else{
    digitalWrite(LED_BLUE, HIGH);
  }

}

boolean getLedStatus(){
  return digitalRead(LED_BLUE)==1 ? false: true;
}

void sendToSocketClient(uint8_t num, String information){

webSocket.sendTXT(num, information);

}

String castStringFromPayload(uint8_t* payload, size_t lenght){
  String dato = "";
  for(int i = 0; i< lenght ;i++){
    dato += ((char)payload[i]);
  }
  return dato;
}


void startHttpServer(){
   serverRouting();
   server.begin();
  Serial.println("HTTP server started");
}

void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void setup(void) {
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, 0);
  Serial.begin(115200);
  delay(50); // Espero la inicializacion del puerto serial para evitar caracteres 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");


  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
 startHttpServer();

  startWebSocket();
}

void loop(void) {
  server.handleClient();

  if(WiFi.status() == WL_CONNECTED) {
      webSocket.loop();
    } else {
      webSocket.disconnect();
    }   

  MDNS.update();
}
