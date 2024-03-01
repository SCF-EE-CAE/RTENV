// PROGRAMA MQTT CLIENT EM ESP8266
// Antonio A. F. Quevedo - FEEC / UNICAMP
// Sketch em Arduino IDE, usando as três bibliotecas abaixo incluídas

// includes
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#define VERSION "MQTT CLIENT v1.0" // Definição de versão

// Definicao de variaveis
char lastfield; // índice do último campo não-nulo da linha de comando (iniciando em 0)
bool enter; // Flag de linha de comando completa (ENTER)
String buf, field[6]; // Buffer de entrada serial e array dos campos da linha de comando já separados
bool isWiFiConnected = false; // flag que indica que está conectado ao WiFi
bool isBrokerConnected = false; // flag que indica que está conectado ao Broker

// Objetos
WiFiClient CLIENT;
PubSubClient MQTT(CLIENT);

// Função de callback para receber mensagens do Broker
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MESSAGE [");
  Serial.print(topic);
  Serial.print("],[");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("]");
}

// Função para limpar as strings de campos e o buffer serial
void FieldClear(void) {
  char i; // Variavel de loop
  
  buf = "";
  for(i = 0; i < 6; i++) {
    field[i] = "";
  }
}

// Função para separar os campos da linha de comando
void SplitCMD(void) {
  bool finish = false; // Flag de chegada ao último campo não-nulo da linha de comando
  char i; // Variável de loop
  int istart, iend; // índices de início e final do trecho do buffer que contém o campo sendo extraído
  
  istart = 0;
  for(i = 0; i < 6; i++) {
    if(!finish) { // Se não chegou ao final
      if(i == 0) { // Se for o primeiro field (comando)
        iend = buf.indexOf(' '); // Procura um espaço
      } else { // para outros campos
        iend = buf.indexOf(',', istart); // procura pela vírgula
      }
      if(iend == -1) { // último field
        iend = buf.length() - 2; // encontra a posição do último caractere (2 antes do final)
        finish = true; // sinaliza que é o último campo
        lastfield = i;
      }
      field[i] = buf.substring(istart, iend);
      istart = iend + 1; // passa o start para o primeiro caractere após a vírgula (ou do \r)
    }
  }
}

// Executa comando CONNWIFI
void ConnWiFi(void) {
  String ssid, pwd; // strings da SSID e senha da rede WiFi
  char le; // Variável para guardar comprimento de strings, e também auxiliar de loop
  
  if(lastfield < 2) { // Menos de 2 parâmetros
    Serial.println("MISSING PARS");
  } else {
    if(!isWiFiConnected) { // Testa antes se o WiFi já está conectado. Se estiver, ignora tudo e parte para a impressão da mensagem de resposta
      le = field[1].length();
      ssid = field[1].substring(1, (le - 1));
      //Serial.println(ssid);
      le = field[2].length();
      pwd = field[2].substring(1, (le - 1));
      //Serial.println(pwd);
      WiFi.begin(ssid, pwd);
      for(le = 0; le < 20; le++) { // 20 verificações com 500ms de intervalo entre elas, espera total 10s
        if (WiFi.status() != WL_CONNECTED) { // Se não conectado, aguarda
          delay(500);
        } else { // Se conectar
          isWiFiConnected = true;
          le = 20; // Força a saída do loop
        }
      }
    }
    // Ao sair do loop ou ignorar a tentativa de conexão, verifica o status da flag de conexão e retorna a mensagem adequada
    if(isWiFiConnected) {
      Serial.println("CONNECT WIFI");
    } else {
      Serial.println("ERROR WIFI");
    } 
  }
}

// Executa comando GETIP
void GetIP(void) {
  //Serial.println("GETIP");
  if(isWiFiConnected) {
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("NOIP");
  }
}

// Executa comando GETMAC
void GetMAC(void) {
  //Serial.println("GETMAC");
  Serial.println(WiFi.macAddress());
}

// Executa comando CONNMQTT
void ConnMQTT(void) {
  String temp; // string para valor temporário (conversão para array de chars)
  char ipc[15], clientID[25], user[20], pwd[20]; //Vetores char para IP, Client ID, e username e senha do broker
  int porta, intval; // Variáveis para número da porta e intermediária no cálculo de número de parte do IP
  uint8_t le; // Variável para guardar comprimento de strings, e também auxiliar de loop
  bool anonymous = true; // Flag que indica se o usuário é anônimo (sem username e password)
  uint8_t ipfield[4];
  uint8_t part = 0; // Número da parte do endereço IP na separação

  // Primeiro vamos ver se já está conectado
  if(isBrokerConnected) {
    Serial.println("CONNECT MQTT"); // Apenas responde que está conectado
  } else { // Agora verifica se tem conexão WiFi
    if(!isWiFiConnected) {
      Serial.println("NOWIFI");
    } else { // Neste ponto, há conexão WiFi mas não ao Broker
      if(lastfield < 3) { // Menos de 3 parâmetros (podem haver 3 ou 5)
        Serial.println("MISSING PARS");
      } else {
        //Serial.println("CONNMQTT");
        // vamos remover as aspas das strings
        le = field[1].length();
        temp = field[1].substring(1, (le - 1));
        temp.toCharArray(ipc, 15); // passa para char array
        le = field[3].length();
        temp = field[3].substring(1, (le - 1));
        temp.toCharArray(clientID, 25);
        // Parâmetros opcionais. É necessário verificar se há 5 parâmetros. 3 ou 4 parâmetros, considera conexão anônima
        if(lastfield == 5) {
          anonymous = false;
          le = field[4].length();
          temp = field[4].substring(1, (le - 1));
          temp.toCharArray(user, 20);
          le = field[5].length();
          temp = field[5].substring(1, (le - 1)); 
          temp.toCharArray(pwd, 20);
        }

        // Vamos separar as partes do endereço IP e montar em uma variável de tipo adequado:
        le = 0;
        part = 0;
        intval = 0;
        while(part != 4) { // percorre o array até ter decodificado os 4 campos
          if (ipc[le] == '.' || ipc[le] == 0) { // Separador ou fim de string
            ipfield[part++] = intval; // Armazena o byte e avança para o próximo elemento do vetor ipfield
            intval = 0; // Prepara para converter a próxima parte
          } else if('0' <= ipc[le] && ipc[le] <= '9') { // Valores numéricos
            intval = (intval * 10) + ipc[le] - '0' ;  // constrói valor a partir dos dígitos decimais
          }
          le++; // Avança para o próximo caractere
        }
        IPAddress ip(ipfield[0], ipfield[1], ipfield[2], ipfield[3]);

        // Agora vamos extrair o número de porta, convertendo string para int
        porta = field[2].toInt();
        
        // Pronto para definir o servidor e realizar a conexão
        MQTT.setServer(ip, 1883); // Define endereço e porta do broker mas não conecta ainda
        MQTT.setCallback(callback); // Define a função de callback
        if(anonymous) { // Se for anônimo, tenta conexão apenas com clientID
          isBrokerConnected = MQTT.connect(clientID);
        } else {
          isBrokerConnected = MQTT.connect(clientID, user, pwd); // Senão, faz a conexão com identificação
        }
        // Após a tentativa de conexão (a função aguarda resposta), verifica a flag "isBrokerConnected" para confirmar se foi bem sucedida
        if(isBrokerConnected) {
          Serial.println("CONNECT MQTT");
        } else {
          Serial.print("ERROR: ");
          Serial.println(MQTT.state()); // Passa o código do erro
        }
      }
    }
  }
}

// Executa comando SUBSCRIBE
void Subscribe(void) {
  String temp; // string para valor temporário (conversão para array de chars)
  char topic[64]; //Vetor char para o tópico
  char le; // Variável para guardar comprimento de strings
  bool result; // Resultado do subscribe
  
  if(lastfield < 1) { // Menos de 1 parâmetro
    Serial.println("MISSING PARS");
  } else {
    //Serial.println("SUBSCRIBE");
    // vamos remover as aspas da string
    le = field[1].length();
    temp = field[1].substring(1, (le - 1));
    temp.toCharArray(topic, 64); // passa para char array

    // Agora verifica se está conectado antes de realizar o subscribe
    if(isBrokerConnected) {
      result = MQTT.subscribe(topic);
      if(result) {
        Serial.println("OK SUBSCRIBE");
      } else {
        Serial.println("ERROR SUBSCRIBE");
      }
    } else {
      Serial.println("NOT CONNECTED");
    }
  }
}

// Executa comando UNSUBSCRIBE
void Unsubscribe(void) {
  String temp; // string para valor temporário (conversão para array de chars)
  char topic[64]; //Vetor char para o tópico
  char le; // Variável para guardar comprimento de strings
  bool result; // Resultado do subscribe
  
  if(lastfield < 1) { // Menos de 1 parâmetro
    Serial.println("MISSING PARS");
  } else {
    //Serial.println("UNSUBSCRIBE");
    // vamos remover as aspas da string
    le = field[1].length();
    temp = field[1].substring(1, (le - 1));
    temp.toCharArray(topic, 64); // passa para char array

    // Agora verifica se está conectado antes de realizar o subscribe
    if(isBrokerConnected) {
      result = MQTT.unsubscribe(topic);
      if(result) {
        Serial.println("OK UNSUBSCRIBE");
      } else {
        Serial.println("ERROR UNSUBSCRIBE");
      }
    } else {
      Serial.println("NOT CONNECTED");
    }
  }
}

// Executa comando PUBLISH
void Publish(void) {
  String temp; // string para valor temporário (conversão para array de chars)
  char topic[64], msg[128]; //Vetor char para o tópico e para a mensagem
  char le; // Variável para guardar comprimento de strings
  bool result; // Resultado do publish
  
  if(lastfield < 2) { // Menos de 2 parâmetros
    Serial.println("MISSING PARS");
  } else {
    //Serial.println("PUBLISH");
    // Vamos remover as aspas das strings
    le = field[1].length();
    temp = field[1].substring(1, (le - 1));
    temp.toCharArray(topic, 64); // passa para char array
    le = field[2].length();
    temp = field[2].substring(1, (le - 1));
    temp.toCharArray(msg, 128); // passa para char array

    // Agora verifica se está conectado antes de realizar o publish
    if(isBrokerConnected) {
      result = MQTT.publish(topic, msg);
      if(result) {
        Serial.println("OK PUBLISH");
      } else {
        Serial.println("ERROR PUBLISH");
      }
  } else {
    Serial.println("NOT CONNECTED");
  }
}
}

// Executa comando PING no broker (para manter conexão)
void Ping(void) {
  if(MQTT.connected()) {
    Serial.println("PONG");
  } else {
    Serial.println("NOPONG");
  }
}
  
// Executa comando DISCONNECT
void Disconnect(void) {
  //Serial.println("DISCONNECT");
  if(isBrokerConnected) {
    MQTT.disconnect();
    while(MQTT.state() > -1); // Espera por algum status de desconectado
    isBrokerConnected = false;
  }
  Serial.println("OK DISCONNECT"); // Responde após a desconexão ou se já estiver desconectado
}

// Executa comando QUIT
void Quit(void) {
  //Serial.println("QUIT");
  if(isWiFiConnected) {
    WiFi.disconnect();
    while(WiFi.status() == WL_CONNECTED); // Aguarda concluir a desconexão
    isWiFiConnected = false;
  }
  Serial.println("OK QUIT"); // Responde após a desconexão ou se já estiver desconectado
}

// Função para fazer o parser de comandos, incluindo verificação de erros e execução dos comandos
void Parser(void) {
  SplitCMD(); // Separa os campos da linha de comando
  // Agora vamos comparar o field[0] com as palavras de comando, uma a uma
  // Conecta à rede Wifi
  if(field[0] == "VER") {
    Serial.println(VERSION);
  } else if(field[0] == "RESTART") {
    Serial.println("Restarting...");
    ESP.restart();
  } else if(field[0] == "CONNWIFI") {
    ConnWiFi();
  } else if(field[0] == "GETIP") {
    GetIP();
  } else if(field[0] == "GETMAC") {
    GetMAC();
  } else if(field[0] == "CONNMQTT") {
    ConnMQTT();
  } else if(field[0] == "SUBSCRIBE") {
    Subscribe();
  } else if(field[0] == "UNSUBSCRIBE") {
    Unsubscribe();
  } else if(field[0] == "PUBLISH") {
    Publish();
  } else if(field[0] == "PING") {
    Ping();
  } else if(field[0] == "DISCONNECT") {
    Disconnect();
  } else if(field[0] == "QUIT") {
    Quit();
  } else {
    Serial.println("INVALID CMD");
  }
}

void setup() {
  Serial.begin(115200);
  enter = false;
  FieldClear();
}

void loop() {
  char ch; // caractere lido da serial
  char k; // variavel de loop
  
  if(Serial.available()) { // Começa vendo se há caracteres no buffer serial
    ch = Serial.read();
    buf = buf + ch; // Adiciona o caractere na variável de buffer
    if(ch == '\n') { // Verifica se chegou ao final da linha
      enter = true;
    }
  }

  if(enter) { // Se o comando foi concluído vamos fazer o parser
    //Serial.println(buf);
    Parser();
    FieldClear(); // Depois da execução do comando, limpa os campos e a flag de linha de comando completa
    enter = false;
  }

  // Na continuação, vamos verificar se ainda estamos conectados ao WiFi e ao Broker
  // Antes de verificar, vemos se os flags correspondentes já não estão zerados.
  // Se isto não for feito, as mensagens de desconectado ficarão se repetindo. A mensagem só deve aparecer se o status ainda for conectado
  if ((WiFi.status() != WL_CONNECTED) && isWiFiConnected) { // Se não conectado ao wifi, está tudo desconectado
    isWiFiConnected = false;
    isBrokerConnected = false;
    Serial.println("WIFI_DISCONNECTED");
  } else if((!MQTT.connected()) && isBrokerConnected) { // Broker desconectado
    isBrokerConnected = false;
    Serial.println("MQTT_DISCONNECTED");
  }

  // Por fim, fazemos a chamada regular do processamento de mensagens
  if(isWiFiConnected && isBrokerConnected) {
    MQTT.loop();  
  }
}

