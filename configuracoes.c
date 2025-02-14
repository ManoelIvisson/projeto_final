// Arquivo que configura os pinos, as credencias do Wi-Fi e as credenciais e conexões do ThingSpeak

#define LED_VERMELHO 13 // Define o pino do LED de falha de Conexão
#define LED_VERDE 11 // Define o pino do LED de conexão bem sucedida 
#define BOTAO_PIN 6 // Define o pino do Botao, que no caso é o B

//Wi-Fi
#define WIFI_SSID "Vava"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "Akira#@2718" // Substitua pela senha da sua rede Wi-Fi

// Thingspeak
#define CHANNEL_ID 2836570  // Id do canal
#define WRITE_API_KEY "8AS7O6H17JAT1HHH" // API Key para o canal Tempo de Olho

// URL do ThingSpeak e sua porta de acesso
#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT 80