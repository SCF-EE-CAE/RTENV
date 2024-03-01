# RTENV

Código do módulo ambiental do projeto.

- _/aux_ contém arquivos auxiliares que foram utilizados para montar o programa
- _/main_ contém o código principal do módulo, que é efeitavemente executado por ele

Algumas bibliotecas são utilizadas no código e podem ser baixadas pela IDE do Arduino. Suas referências podem ser encontradas a seguir

- [PubSubClient](https://pubsubclient.knolleary.net/): Abstração do Cliente MQTT
- [NTPClient](https://github.com/arduino-libraries/NTPClient): Abstração do Cliente NTP, para sincronização do tempo
- [Bonezegei_DHT11](https://github.com/bonezegei/Bonezegei_DHT11): Para a leitura do sensor DHT11
- [ArduinoLog](https://github.com/thijse/Arduino-Log/): Framework de geração de log (Conexão Serial)

Para enviar o código para ESP-01, também é necessário adicionar uma URL de Board Manager nas preferências da IDE:

https://arduino.esp8266.com/stable/package_esp8266com_index.json

E selecionar a placa "Generic ESP8266 Module"
