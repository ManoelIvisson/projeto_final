# Código do Gráfico do ThingSpeak: Tempo Médio de Exibição por Produto
## Gráfico: https://thingspeak.mathworks.com/apps/matlab_visualizations/605373?height=auto&width=auto

% Configuração  
channelID = 2836570; % ID do canal  
readAPIKey = '8AS7O6H17JAT1HHH'; % Chave de leitura 

% Obtendo os últimos 100 valores dos Fields 1 (Tempo) e 2 (Produto ID)  
data = thingSpeakRead(channelID, 'Fields', [1, 2], 'NumPoints', 100, 'ReadKey', readAPIKey);  

% Extraindo valores dos dois campos  
field1_values = data(:, 1); % Tempo de exibição  
field2_values = data(:, 2); % ID do Produto  

% Identificar produtos únicos  
produtos_unicos = unique(field2_values); % Lista de produtos distintos  
tempos_medios = zeros(size(produtos_unicos)); % Inicializa vetor para armazenar médias  

% Calcular a média do tempo para cada produto  
for i = 1:length(produtos_unicos)  
    produto_id = produtos_unicos(i);  
    tempos_medios(i) = mean(field1_values(field2_values == produto_id));  
end  

% Criando o gráfico de barras  
bar(tempos_medios);  
set(gca, 'XTickLabel', string(produtos_unicos)); % Rótulos do eixo X com IDs dos produtos  
xlabel('Produtos');  
ylabel('Tempo Médio (ms)');  
title('Tempo Médio de Exibição por Produto');  
grid on;  
