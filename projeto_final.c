#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include <stdlib.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "hardware/adc.h"

#include "exibicao_display.c"
#include "configuracoes.c"

typedef struct {
    float tempo;
    int id_produto;
} dados_envio_t;

uint cliente_presente = 0;

// Variável global para controle de tempo
static uint64_t tempo_inicio = 0;

struct tcp_pcb *pcb;

// Callback de conexão TCP
static err_t tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar ao servidor: %d\n", err);
        tcp_close(tpcb);
        free(arg); // Libera a memória alocada
        return err;
    }

    // Recuperar os valores corretamente
    dados_envio_t *dados = (dados_envio_t *)arg;
    float tempo = dados->tempo;
    int id_produto = dados->id_produto;

    printf("Conectado ao ThingSpeak!\n");
    printf("Enviando -> Tempo: %.2f | Produto ID: %d\n", tempo, id_produto);

    // Criar requisição HTTP
    char request[256];
    snprintf(request, sizeof(request),
             "GET /update?api_key=%s&field1=%.2f&field2=%d HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n\r\n",
             WRITE_API_KEY, tempo, id_produto, THINGSPEAK_HOST);

    err = tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("Erro ao tentar enviar dados ao ThingSpeak\n");
        tcp_close(tpcb);
        free(dados);  // Libera memória
        return err;
    }

    tcp_output(tpcb);
    printf("Dados enviados para ThingSpeak!\n");

    // Libera a memória após o envio
    free(dados);
    return ERR_OK;
}

// Callback do DNS para resolver o nome do host
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr == NULL) {
        printf("Erro: DNS não conseguiu resolver %s\n", name);
        free(callback_arg);
        return;
    }

    // Criar PCB TCP
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB TCP\n");
        free(callback_arg);
        return;
    }

    // Conectar ao ThingSpeak e passar os dados corretamente
    err_t err = tcp_connect(pcb, ipaddr, 80, tcp_connected);
    if (err != ERR_OK) {
        printf("Erro ao conectar ao ThingSpeak: %d\n", err);
        tcp_close(pcb);
        free(callback_arg);
    } else {
        // Guardar os dados no PCB (importante para que o callback os receba corretamente)
        tcp_arg(pcb, callback_arg);
    }
}

// Função para iniciar o envio de dados ao ThingSpeak
void enviar_dados_thingspeak(float tempo, uint id_produto) {
    ip_addr_t dest_ip;

    // Alocar a estrutura dinamicamente para manter os dados até a conexão
    dados_envio_t *dados = (dados_envio_t *)malloc(sizeof(dados_envio_t));
    if (dados == NULL) {
        printf("Erro ao alocar memória para os dados\n");
        return;
    }

    dados->tempo = tempo;
    dados->id_produto = id_produto;


    err_t err = dns_gethostbyname(THINGSPEAK_HOST, &dest_ip, dns_callback, dados);

    if (err == ERR_OK) {
        // O endereço IP já está em cache, podemos conectar diretamente
        dns_callback(THINGSPEAK_HOST, &dest_ip, dados);
    } else if (err == ERR_INPROGRESS) {
        // A resolução DNS será feita de forma assíncrona
        printf("Resolvendo DNS...\n");
    } else {
        printf("Erro ao iniciar resolução DNS: %d\n", err);
        free(dados); // Libera a memória se ocorrer uma falha
    }
}

void gerenciadorInterrupcoes(uint gpio, uint32_t events){
    static uint32_t ultimo_tempo = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if (tempo_atual - ultimo_tempo < 200) return; // Debounce de 200ms
    ultimo_tempo = tempo_atual;

    // Verifica se não há nenhum produto sendo exibido, caso não haja o display alterna para o outro produto (Somente para testes
    // de envio dos dados para o ThingSpeak)
    if (!cliente_presente) {
        if (produto_atual.id == 1) {
            produto_atual.id = produto2.id;

            // Declaração especial já que nome é um array de char e não pode ser atribúido diretamente igual os outros atributos
            strncpy(produto_atual.nome, produto2.nome, sizeof(produto_atual.nome) - 1);  
            produto_atual.nome[sizeof(produto_atual.nome) - 1] = '\0';  // Garante que a string seja terminada com '\0'

            produto_atual.preco = produto2.preco;
            produto_atual.promocao = produto2.promocao;
        } else {
            produto_atual.id = produto1.id;
      
            strncpy(produto_atual.nome, produto1.nome, sizeof(produto_atual.nome) - 1);  
            produto_atual.nome[sizeof(produto_atual.nome) - 1] = '\0'; 

            produto_atual.preco = produto1.preco;
            produto_atual.promocao = produto1.promocao;
        }
    }
}

int main() {
    // Configura o LED que indica que o Wifi ainda não foi conectado
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_put(LED_VERMELHO, 1); // Já liga o LED para indicar que o WIFI ainda não está conectado

    // Configura o LED que indica que o Wifi foi conectado com sucesso
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);

    // Produtos para exemplo
    // Atribuindo valores ao produto1
    strcpy(produto1.nome, "Feijao Preto");
    produto1.id = 1;
    produto1.preco = 8.98;
    produto1.promocao = 6.75;

    // Atribuindo valores ao produto2
    strcpy(produto2.nome, "Arroz Branco");
    produto2.id = 2;
    produto2.preco = 12.99;
    produto2.promocao = 0;

    // Inicializando o produto atual com "Feijão Preto"
    produto_atual.id = produto1.id;
      
    strncpy(produto_atual.nome, produto1.nome, sizeof(produto_atual.nome) - 1);  
    produto_atual.nome[sizeof(produto_atual.nome) - 1] = '\0'; 

    produto_atual.preco = produto1.preco;
    produto_atual.promocao = produto1.promocao;

    // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    char *mensagem[2]; 

    mensagem[0] = "Conectando no Wifi";
    mensagem[1] = "Aguarde um momento";

    exibirMensagem(mensagem, 2, ssd, &frame_area);

    stdio_init_all();  // Inicializa a saída padrão
    sleep_ms(10000);
    printf("Iniciando conexões...\n");

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }else {
        printf("Connected.\n");
        // Read the ip address in a human readable way
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 1);
    }

    printf("Wi-Fi conectado! Galera\n");

    // Configura o Botao para aplicar a troca de produtos (Apenas para o protótipo, no mundo real não seria assim)
    gpio_init(BOTAO_PIN);
    gpio_set_dir(BOTAO_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_PIN);

    gpio_set_irq_enabled_with_callback(BOTAO_PIN, GPIO_IRQ_EDGE_FALL, true, &gerenciadorInterrupcoes);
    
    // Configuração do Joystick
    adc_init();

    adc_gpio_init(26);
    adc_gpio_init(27);

    // zera o display inteiro
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
    
    // Loop principal
    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo

        // Lendo valores dos eixos do Joystick
        adc_select_input(0);
        uint adc_y_raw = adc_read();

        adc_select_input(1);
        uint adc_x_raw = adc_read();
        
        // Verifica se o usuário mexeu no Joytick para ativar ou desativar o display (Novamente, somente no protótipo)
        if (!cliente_presente) {
            if (adc_y_raw > 4000) {
                exibirProduto(produto_atual, ssd, &frame_area);
                tempo_inicio = time_us_64(); // Salva o tempo atual (em microssegundos)
                cliente_presente = 1;
            } 
        } else {
            if (adc_y_raw < 100) {
                uint64_t tempo_final = time_us_64();  // Tempo atual ao remover o produto
                float tempo_decorrido_ms = (tempo_final - tempo_inicio) / 1000.0;  // Converte para milissegundos

                // Zera o display inteiro
                memset(ssd, 0, ssd1306_buffer_length);
                render_on_display(ssd, &frame_area);

                // Envia os dados ao ThingSpeak
                enviar_dados_thingspeak(tempo_decorrido_ms, produto_atual.id);

                cliente_presente = 0;
                tempo_inicio = 0;
            }
        }
        

        sleep_ms(100);
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}
