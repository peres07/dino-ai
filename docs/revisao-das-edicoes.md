# Revisão das duas edições

## Referências

- **Com pássaros:** `arquivos/Dino IA Hardmode.exe` (`803541bc…`). A análise original, as capturas e as diferenças da versão web estão em [engenharia-reversa.md](engenharia-reversa.md).
- **Sem pássaros:** `Dino sem pássaros/arquivos/Dino IA Hardmode.exe` (`a9e69b5637a2ccd889c61b18bd85a8daabf32a556a206c8ebbcb27f74e0db44d`). A descompilação direta no Hopper via REA funcionou, com ajuda dos símbolos COFF. O [pseudocódigo selecionado](pseudocodigo-sem-passaros-rea.txt) não é o código-fonte original.

| Sem pássaros — rotina | Evidência REA | Observado no binário |
|---|---|---|
| `criarExperimentoDinoPlus`, `0x4060b0` | `ev_7228eac7ef3623282c862f873e606465b3afa4b93a35c224f79cb0f326e6b950` | 1000 indivíduos e 100 pais; topologia `10→15→15→7`; teste com dois personagens. |
| `getNomesOutputs`, `0x445b80`; `getValorSensores`, `0x446630` | `ev_218a02486158e7ab4412014242a623b497f3378017a259409be5dfa1d2abe58d`, `ev_e29a7e0a8c6792f8f446d7b9d661506dc8349cf6f3c90c2642c6423f5618ce17` | Sete saídas de movimento, nenhuma de mira ou disparo. |
| `salvarEmArquivo`, `0x440310` | `ev_b8c4d44622723f608472ddab79f2fe24fd7af8acb3fd0e2743bcf74accd624bc` | Cabeçalho e pesos. As 19 redes empacotadas têm 4164 bytes, cada uma com 517 pesos. |
| `obstaculo::alocarSprites`, `0x44d4a0`; `prepararObstaculo`, `0x44dd20` | `ev_310e72256750cbce76ab4f0c74eef4bd4f9dfbf230dea7bc0f66cf79074294a2`, `ev_441f4a833f88e098504f6ff7c8806c71bc67e3af881cbcd04b73c66130260990` | Ainda há sprites de pássaros na inicialização de obstáculos. Isso não demonstra sua frequência em jogo. O port sem pássaros não gera aves; os 14 tipos nativos foram reproduzidos no port, com a ave excluída só nessa edição. |
| `adicionarPontuacaoDinos`, `0x449af0`; `criarGeracao`, `0x448190` | `ev_682047023c0ffe1055e6652af75c87eaa809afeb4f04bcb642ed096362eb7c59`, `ev_b46855aded030ed9ca459340029cbc9114b78e17faa2bcc81175e55fcef16e62` | Pontuação e estratégia evolutiva próprias, não apenas uma opção de ocultar o pássaro da outra edição. |
| `experimento::desenhar`, `0x443c40` | `ev_5bd426cc5b752d022a396ea0c60bd5a02d0235b11c1babd2578529c0990e25aa` | Desenho do ambiente e da interface. |

## Port nativo vs. execução do Windows

As rotas do port executam C recompilado para Wasm. Nenhum `.exe`, DLL ou emulador é distribuído no site. Como verificação adicional, tentei executar o segundo `.exe` inalterado no navegador com Boxedwine 26R1. A execução avançou até a abertura da janela, mas falhou por falta de suporte a OpenGL no runtime web (`No matching GL pixel format available`). `PIG_criarJanela` passa `0x6` a `SDL_CreateWindow`, que inclui `SDL_WINDOW_OPENGL` (`ev_aef898b423d71839dc6323d3bee0afbb03254bdc5ea84356206851323257ba78`). O teste não é prova de que toda abordagem de emulação falharia; essa configuração não fornece uma versão idêntica utilizável.

## O que foi conferido e corrigido

- **Interface:** comparação com capturas de Wine/Xvfb a 1920×1080, em `build/native-reference/shots/` e `build/birdless-reference/shots/`. A edição sem pássaros mostra o painel de uma partida por geração e a rede de movimento; as ampliações e o histograma seguem as capturas. Os sprites de neurônio dessa edição são `neuronio2.png`, `neuronio4.png`, `luz.png` e `entrada.png` (`ev_644e753541be41c82e81441db1e730d4de6c3f58964f932e1bcd2064a7dfcfbe`).
- **Simulação:** na edição sem pássaros, as sete ações são evoluídas e os arquivos exportados são redes de movimento legadas. Na edição com pássaros, os testes mantêm os dois modos de treino e a leitura dos arquivos `.dino` e legados. A revisão restaurou a câmera em teste na edição sem pássaros, a redução de velocidade abaixo de `1×`, a rolagem do gráfico livre e a proteção contra seleção sem pais válidos.
- **Distribuição e armazenamento:** `make test` passou nos 17 testes do Wasm e da distribuição. Eles conferem também os 31 arquivos legados, a origem da fonte e dos sprites e os links das duas rotas quando hospedadas sob uma subpasta. Em Chromium, troca de versão, menu, atalhos de gráficos e teste passaram em 23 passos (`ev_4556894790d80ec448317cdd7092bdcad1fabbf2a2a520f8758b728e32128480`). Um segundo cenário validou a persistência do recorde sem pássaros e que ele não aparece na outra rota (`ev_915b46da4f3b059e4f49b909e75fd7792ae5598c6f99f6fe9d3e5e49e61e0cc8`).

## Critério de paridade

Os manuais, as rotinas descompiladas, os arquivos legados e as capturas servem como referência. Compatibilidade de arquivos não prova que a IA toma a mesma sequência de decisões, que o mesmo recorde é obtido ou que a imagem é idêntica. O seletor de edição, os ajustes web, a persistência no navegador e a evolução conjunta opcional na edição com pássaros são diferenças deliberadas. Paridade integral de física, RNG, estratégia de evolução e desenho exigiria uma comparação determinística, quadro a quadro, ou os projetos-fonte completos; não está certificada.

## Movimento, colisão e treino em duas etapas

| Evidência | Aplicação no port |
|---|---|
| Ambos os `processarAmbiente` (`0x446a20` sem pássaros, `0x448b50` com pássaros), `ev_e0a5d3d5abf7d89699d18ef968f5d772ca7f31ac498270cf773d9cde9eea8e29` / `ev_d95e7789eb04935a349112d41e493bbd355c5720e4d4b62651324d456dad206c`. Clamp e inversão observados nas sequências `0x4471fa` e `0x4493d0`. | Rolagem começa em 5 pixels nativos/tick, oscila entre 5 e 10 e muda 0,001 por tick; conversão única para o canvas. |
| Ações e constantes nos dois PEs (`0x4456e0` / `0x447130`), incluindo as inicializações de salto `9` e movimento à esquerda `−4,5`. | Um integrador compartilhado integra os comandos de movimento, com impulso, gravidade e desaceleração do dash separados da mira. Render e testes de colisão usam a mesma pose. |
| Obstáculos e colisão (`0x44dd20`, `0x449f50`, `0x44c400`), `ev_441f4a833f88e098504f6ff7c8806c71bc67e3af881cbcd04b73c66130260990`, `ev_85769594f6da93eb6289224100109df6461dcb973909d4f770f8aa382ce79edf` / `ev_f8f042fa2acd5e38cafd85a8bcf5c1f9f419b0f75920901654e61a1f4d3a1bb7`. | A árvore e a caverna são obstáculos, com dimensões lógicas menores do que a imagem e camadas diante/atrás do dino. A caverna permite passar abaixado; a árvore tem uma janela vertical. As serras de `fundo0.bmp` e `fundo1.bmp` continuam decorativas. A fila agora usa as larguras lógicas para espaçar obstáculos. Partes decorativas da copa não são hitbox. |

O modo **Só movimento** da edição com pássaros exporta exclusivamente os pesos de movimento. **Só disparo** usa uma cópia imutável da rede escolhida e evolui apenas os pesos de mira/disparo. O teste de regressão compara os bytes da rede fixa antes e depois da evolução. `make test` também exercita salto, dash, limites de velocidade e colisões de árvore e caverna.

### Recuperação de energia nos executáveis

Nas duas edições, `game::regenerarStamina` soma **0,01 de energia por passo** aos dinos vivos e limita o total a **100**. A rotina compara o índice do próximo obstáculo antes e depois do passo. Quando o índice muda, consulta o tipo do obstáculo anterior: **+20** para supercactos (8/9), **+50** para o Chrome (10) e a árvore (12), e **recarga completa** para espinhos (13). Cactos comuns e caverna (11) não dão bônus. Isso é recuperação por ultrapassagem, não por tocar no obstáculo.

Verificação estática com REA e conferência das constantes na desmontagem: **sem pássaros**, `0x449840` (`ev_bc6615a08e574f2e9365b3ca323ea12a33fb5a7bd2bd5cd347f1fa5bf38f5600`), com seleção do obstáculo em `0x449ec0` (`ev_16353a63c1ea29356a4bd759e5a94e8e3214c211b0cf6a6cb792b22609045532`); **com pássaros**, `0x44bd30` (`ev_6cf1f8af6b0b0e0fb2174b665feef0154f25ef3fe8a729238a29bbefdb644a89`), com seleção em `0x44c370` (`ev_890c0f43b1ca0e78f76f370d1032b1eba82fb49cbe94ca70e904eaa47a73de8b`).

O port agora aplica essas regras em ambas as edições e em todas as etapas de treino. Compara a identidade do obstáculo, não sua posição na fila interna, para não gerar bônus falso quando a fila é reciclada. A recarga de disparo é separada (**+0,01 por passo**, teto **20**) na edição com pássaros.

### Revisão de comida, aves e disparo

| Mecânica | Evidência nos originais | Aplicação no Wasm |
|---|---|---|
| Comida | `comida::reiniciar` (`0x44bb70`/`0x44eb30`); `aplicarColisaoDinoComida` (`0x449d10`/`0x44c050`; `ev_727e5a7d66d36e07f697b9a3f76abedf3f1b8b11238a5c095975bf9e0347f4b5`, `ev_766be9b5ae5e61337549f22fe17aad91f2e926922f163506d27bfb04cfdc514e`). A coleta incrementa um contador; não altera energia nem munição. | Moeda fixa na tela, com novo X a cada 1001 passos, e coleta individual pela área do sprite. Removidos os bônus de recarga da moeda. A animação de coleta ainda não replica todos os estados. |
| Pontuação sem pássaros | `adicionarPontuacaoDinos` (`0x449af0`; `ev_ec2a6cca711ef7f57f1b2d6eaebc0e6db38020345ae2ba65bf16a0f51dfc7c5b`). Usa a distância até a borda direita do próximo obstáculo, a distância horizontal até a comida e o limiar de energia `>50` para o prêmio de ultrapassagem. | Passa a considerar as duas distâncias e o prêmio de 1 ou 2 pontos. A avaliação nativa completa da geração e os desempates ainda não foram reproduzidos. |
| Ave atacante e sensores | `inicializarPassaro`, `dentroDaTela`, perseguição e `getValorSensores` (`0x450220`, `0x450190`, `0x450137`, `0x448580`; `ev_bc49548d36685549336cb3d78e5c1f9b96c74004f3fb2a697d5b2803056cc279`, `ev_befebb44b27397e04e261c4bb1ae38c9cb52175a6523cd0b48f2b20930a320fb`, `ev_11115495dbb33594fdc6c944238cf51df84663fef49bed16f953b99c01cc9395`). | Espera inicial de 50 passos, entrada pelas laterais ou pelo topo e perseguição direta a 5 px nativos/passo. A mira só recebe entradas quando a ave alvo está na área válida; a distância usa as origens e o ângulo usa os centros dos sprites. |
| Disparo | `atirar`, `inserirAcoes` e `aplicarColisaoPassaroProjetil` (`0x4407e0`, `0x447130`, `0x44c780`; `ev_1269c56f824c80e4ceb2f500f2bc0fbf9a387f7e3c2b4e1c882b09f0edddb980`, `ev_57ecacb90260796cfcc5616f066506c83a7767212213c134652f24222ab1a984`, `ev_54919c1ab3a90f1243c1130418d6bc1f4a09648e3944b18e5ba09a6769920d9f`). | Cinco slots, custo 5, intervalo de 10 passos, giro proporcional e pago, disparo do centro do dino com velocidade proporcional à saída da rede. Ao acertar, a munição volta a 20, a ave reaparece e o projétil segue. O controle manual usa os mesmos limites, sem equivalência de sensibilidade medida. |

**Validação do port:** `make test` passou com 14 testes JavaScript/Wasm mais as regressões de física (bônus por tipo, colisão sem recarga, reciclagem de obstáculos, comida, tiro e ave). O Chromium iniciou partidas nas duas edições sem erros de página (`ev_139d2dcc2d66c2f30cb1d67a3306a5d1ee8482f90f08cdcf4d12b830716e7a68`). Docker/Nginx serviu as duas rotas, os modelos legados e o módulo com `application/wasm`.

### Pontuação, evolução e sorteio

- **Pontuação com pássaros:** `getFitnessIndividuo` soma dez posições no dino (`0x449a40`, `ev_84f618fc532af980ef05dca703290262025a4cf73c5d53b8f161056cbeda329e`). O componente de acerto recebe `1 + distância horizontal/vertical combinada entre as origens ÷ largura da janela` (`0x44c8e8–0x44c923`, `ev_e74948b7d429546206c44dea96db5368e27077611f4c08be3fd100db0ab13a7b`). O port usa esse componente em vez dos antigos prêmios artificiais por corrida e moeda no disparo, usa X como critério secundário e preserva a média de duas partidas para evoluir. O treino **Só movimento** continua premiando desvio, pois não há mira nessa etapa. Não foi demonstrada equivalência dos dez acumuladores em todos os ramos; recordes criados pela fórmula antiga não são comparáveis diretamente com os novos.
- **Evolução:** `criarGeracao` (`0x44a530` com pássaros, `0x448190` sem pássaros; `ev_a93d7d62789185587a40507d86c4b7e818986e6bbc341fac768e5d4f738a1752`, `ev_2a141c02bb733cba71d01455e09f75929c3c19791fa5be169d8c2cd09dc75c38`) clona os pais ranqueados em ordem cíclica, sem cruzamento. A população inicial usa pesos inteiros divididos por 10.000 (`0x452780`/`0x44e930`). A mutação é gaussiana discreta (`0x440960`/`0x440150`; `ev_aba9a48f4e04fbcb4bdfffca280e70fb680d9df04253ad1a290c41698cab0055`, `ev_f47ed04713fc57b03764c89ba9c25bea7a2395d670ea4929dcfa975c9f8ab2a6`). No disparo, o primeiro clone fica intacto e cada outro tem um peso zerado depois da mutação; na edição sem pássaros, todos sofrem mutação sem zeragem deliberada. O port mantém a intensidade de mutação ajustável e a extensão de movimento semeado na edição com pássaros; não modifica a rede fixa do modo **Só disparo**.
- **Obstáculos:** o sorteio inteiro usa MT19937 com intervalo inclusivo nos PEs (`0x4029a0`/`0x402d20`; `ev_55bdd5f93ba36a2d546438183af83e2ee3e20c2fd8715a9af197c3f0f839e0`, `ev_45d8eae9ed04f2adf452ea65ac8ef593e6cd2312465e58bce7305df3f0f839e0`). A preparação (`0x451990`/`0x44dd20` e `0x451b30`; `ev_8dcff9a3693b8f34c7f3435e19e967a4d5a1ef25cad803026a2079cafb06738a`, `ev_441f4a833f88e098504f6ff7c8806c71bc67e3af881cbcd04b73c66130260990`, `ev_8f027467754211ef554b18b4426d6c2658428ea5756738db39b870e6b21d72bb`) usa 14 tipos com escala inicial 1, um só tipo de ave-obstáculo com altura sorteada, distância de 600–750 px e mais 100 px após a árvore. O port replica o sorteio e os intervalos, exceto a exclusão deliberada da ave na edição web **Sem pássaros**. As partidas deixam de reutilizar dois mapas fixos no treino com pássaros.

**Ordem dos sorteios.** O treino agora usa um único fluxo MT19937 contínuo para pesos, mutações, obstáculos, comida e aves, sem reiniciá-lo entre partidas ou gerações. `reiniciarPartida` (`0x448900`, `ev_b16e60702c896e34c2fc8716d5fee67bf11299a02b9387a33d4458d70495c526`) sorteia os obstáculos, depois a comida, depois a origem das aves por indivíduo. `passaro::atualizar` (`0x4504e0`, `ev_81a7c4f073696f79c509f620809b37f3cfb17b08a301da02844642207b5e441f`) apenas ativa as aves no fim da espera; não faz um segundo sorteio nessa ativação. O port testa essa ordem com uma semente fixa. O modo de teste do navegador usa seu próprio fluxo para não alterar um treino pausado.

**Recordes.** A seleção e a gravação agora usam a mesma avaliação concluída, sem promover um candidato por uma única partida isolada. Para a edição com pássaros, a média é calculada por indivíduo nas duas partidas antes de escolher o recordista (`SDL_main`, `0x4085ef–0x4086b2`). A posição X desempata apenas o líder vivo. A rede salva é a primeira com a maior média (`0x44af30`); a comparação usa a precisão integral do `double`, aceita igualdade e arredonda apenas o nome (`0x408710–0x4087bf`, `0x442c10`). O PE mantém ainda um recorde separado de distância global, que não é a aptidão da rede. No fim da partida o executável conserva três componentes (`0x4494c9–0x44952b`); só foi confirmado um incremento no componente de acerto (`+0xa0`). Não foram presumidos bônus para os outros dois (`+0x90` e `+0xb0`).

**Limites restantes:** persistem diferenças no consumo de `rand()` pelo executável, nas chamadas feitas pela inicialização nativa e no RNG isolado do teste web. Empates completos, todos os ramos da pontuação e o acabamento visual não estão certificados como idênticos. A validação determinística em `make test` é do port, não uma reprodução quadro a quadro do PE.

### Auditoria final: entrada, HUD e cenário

Duas execuções novas dos PEs em Wine 11.22, cada uma numa cópia isolada com prefixo próprio a 1920×1080, confirmaram `TA: 1.000x`, `0h:0m:0s`, três faixas de serra e a moeda pequena. Os hashes dos `.exe` foram comparados antes e depois e não mudaram.

| Item | Evidência | Aplicação |
|---|---|---|
| Formato do HUD | Strings `%.3fx` e `%dh:%dm:%ds` nos dois PEs (`0x4c5280`/`0x4c13e1`, `0x4c5201`/`0x4c1362`). Capturas novas confirmam `TA: 1.000x` e `0h:0m:0s`. | As duas edições passam a usar o mesmo formato compacto; antes a edição com pássaros mostrava `1.00` sem sufixo. |
| Chão | `experimento::desenhar`, seleção em `0x445b93`: índice 5 em múltiplos de 20, 4 em múltiplos de 10, senão o ciclo 0–3. BMPs `chao0..5` medem **60×12 px nativos**. | Ladrilhos desenhados em 60×12 convertidos por `MOVEMENT_SCALE` e com o mesmo padrão de seleção. Antes eram 1,6× maiores. |
| Serras | Capturas das duas edições e desenho em `0x443c40`/`0x44cc9b` mostram três faixas (`fundo0`/`fundo1`, 1200×90 nativos) com parallax distinto. | Três faixas nas duas edições, com tons escuro/médio/claro. |
| Moeda | PNGs e sprites nativos medem 40×40; `0x449383–0x44939f` aplica a oscilação vertical senoidal. | Moeda quadrada de 40 px nativos com oscilação de ±5 px nativos. Antes era esticada para 40×50. |
| Fumaça | Nove quadros `fumaca1..9` no atlas. | O índice agora alcança o nono quadro (antes parava no oitavo). |
| Ave | `passaro::desenhar` (`0x45038b–0x450456`): o ângulo de voo fica em `bird+0x18` (`0x450630`, saída de `anguloEntrePontos`). Ângulo em (90°,270°) usa `passaro3/4` com rotação `ângulo−180`; fora disso usa `passaro1/2` com rotação `ângulo`. O grupo anima a cada 10 atualizações (`0x450665–0x450683`, intervalo `10`). Velocidade 5 px/tick (`0x4c6fa4` em `0x450637`). | O sprite escolhe a família pelo sinal de `vx`, é **rotacionado pelo ângulo de voo** (espelhado para a esquerda) e alterna o quadro a cada 10 passos. |
| Mira | `Girar Cima`/`Girar Baixo` (`0x4c6029`, `0x4c6034`) são as saídas 0 e 1; ângulo positivo aponta para cima. | Corrigido o sinal do giro na rede e no teclado (`A` sobe, `D` desce) e o desenho da arma/projétil, que estava espelhado. |
| Dimensões do dino | Tabela de constantes em `0x4c6ff0`: (40,43) em pé, (55,25)/(55,26) abaixado, (36,37) pulo duplo, (48,47) queda, (51,44) dash (`0x43fc95–0x43feec`). | O port usava largura 56 no agachamento; corrigido para **55**. |
| Patinho e dash | Integrador comum (`0x446e60–0x446ec8` com `0x447340`): o agachar soma um deslocamento próprio sem substituir a gravidade; o dash volta a ficar disponível no chão, não ao soltar o botão. | `move_runner` aplica `-5` px nativos durante o agachamento e rearma o dash apenas no chão. |
| Modo inicial | O PE com pássaros começa com 500 indivíduos, 50 pais e 35 pesos de mira, mantendo `redeDesviando` fixa e fora do DNA. | A edição com pássaros abre em **Só disparo** com `redeDesviando`; o treino conjunto continua opcional na interface. |
| Camadas da árvore | `obstaculo::alocarSprites` (com pássaros, `0x451539–0x4518b2`) guarda `arvore2` em `manager+0x78`, `arvore3` em `+0x7c` e `arvore4` em `+0x80`. `obstaculo::desenhar` desenha `+0x7c` e `+0x80` na passada 1, atrás do dino (`0x4520a0` e `0x4521f3`), e só `+0x78` na passada 2, à frente (`0x451f92`). A base `arvore.png` usa o sprite da entrada e sai na passada 1. | Na edição com pássaros, a copa base e depois `arvore3`/`arvore4` são desenhadas atrás do dino, e `arvore2` na frente. A edição sem pássaros só tem `arvore.png` e `arvore2.png` no pacote e mantém duas camadas. Um teste novo confere a ordem das duas passadas nas duas edições. |

### Verificação final por subsistema

| Subsistema | Evidência conferida | Situação |
|---|---|---|
| Rolagem e velocidade | `5` a `10`, passo `±0,001`, inversão nos limites (`0x4471fa`, `0x4493d0`) | Igual |
| Ações e impulsos | Salto `9`, direita `3`, esquerda `−4,5`, gravidade `0,4`, dash `20×0,9`, agachar `−5`, duplo/super com custos 20/50 | Igual |
| Dimensões e poses | Tabela `0x4c6ff0` (40×43, 55×26, 36×37, 48×47, 51×44) | Igual (largura do agachamento corrigida) |
| Colisão | `0x449f50`/`0x44c400`: retângulo 10%–90% da pose, janela da árvore, caverna abaixada, círculo do Chrome | Igual |
| Obstáculos | 14 tipos, escala 1, espaçamento 600–750 +100 após árvore, 7 slots | Igual (sem ave na edição sem pássaros) |
| Comida | `rand(0,1880)`, oscilação senoidal, coleta sem bônus | Igual |
| Energia | `+0,01`/passo, `+20` supercacto, `+50` árvore/Chrome, `100` espinhos | Igual |
| Mira e tiro | 4 sensores, giro `12°`/saída com custo `0,167`, 5 projéteis, custo 5, intervalo 10 | Igual |
| Ave | Espera 50, velocidade 5, entrada 1–5, recarga de munição ao acertar | Igual (sprite agora rotacionado, quadro a cada 10) |
| Pontuação | Acerto `1 + distância/W`; `+0x90`/`+0xb0` sem escritor | Igual |
| Evolução | Clone cíclico, gaussiana discreta, 1 clone intacto, 1 gene zerado | Igual |
| Recordes | Média das 2 partidas, primeiro máximo, precisão integral, arredondamento só no nome | Igual |
| HUD | `%.3fx`, `%dh:%dm:%ds`, rótulos conferidos na string table | Igual |
| Cenário | 3 serras, chão 60×12 com chao4/5, moeda 40×40, 9 quadros de fumaça | Igual |
| Rede ampliada | Hit test do mouse (`0x4056d1–0x40576a`) | Igual |
| Modo inicial | 500/50, 35 pesos de mira, `redeDesviando` fixa | Igual |

**Divergências restantes (conhecidas):** os **valores** do RNG não coincidem com o PE (o port separa os fluxos para reprodutibilidade; a distribuição é a mesma); empates de ordenação seguem o primeiro índice, enquanto o `std::sort` nativo é instável; a auditoria da **inferência** das redes empacotadas (layout de pesos e ativação por camada) não foi concluída; o **minimapa** do port copia a tela, não um render próprio; a **fonte** fora do HUD usa o canvas (SDL_ttf no original) e o **pacing** avança por tempo real com teto de 96 passos por quadro.

**Ainda não certificado:** a auditoria das redes salvas não foi concluída nesta rodada por limite de uso do provedor; a comparação continua estática mais visual, não quadro a quadro com o PE. A ordem de `rand()` da inicialização nativa, os empates de ordenação e o acabamento de textos fora do HUD seguem sem prova de igualdade.

### Disco escuro do líder, pontuação e rede com cursor

| Item | Causa e correção |
|---|---|
| Disco escuro junto ao líder (só Com pássaros) | Não era um sprite novo: eram **as 500 armas** desenhadas no mesmo ponto, porque o port colocava todos os indivíduos em `x=132`. `dinossauro::prepararDinossauros` soma a cada um `rand() % (2*150) - 150` px nativos (`0x440791–0x4407bc`, com o alcance `0x96` em `0x44d432`). O port passou a sortear `(r%300 - 150) * MOVEMENT_SCALE` por indivíduo; as armas deixam de formar o círculo preto. |
| Dois acumuladores de aptidão | Varredura completa de `+0x88` a `+0xd0` nos dois PEs: só `+0xa0` recebe `fadd` (`0x44c91d`). `+0x90` e `+0xb0` são zerados no construtor (`0x4066ea`) e apenas lidos; não têm escritor incremental. A aptidão da partida equivale ao componente de acerto, que é o usado pelo port. **Não é divergência.** |
| Rede ampliada com todas as conexões | O nativo faz *hit test* com a posição do mouse (`0x4056d1–0x40576a`) e desenha só as conexões do neurônio sob o cursor. O port agora recebe `pointer(inside,x,y)`, seleciona o neurônio no mesmo raio do desenho e filtra as arestas; sem cursor, mantém todas, como nas capturas do HUD. |
| Deslocamento de `arvore3`/`arvore4` | O PE subtrai 40 e 2 px nativos de dois valores internos do desenho dessas camadas (`0x4520ae`, `0x4520b6`) e usa o escala/posição de cada imagem. As três imagens têm o mesmo tamanho e o port mantém a mesma origem; a diferença residual fica abaixo de 2 px nativos na área visível (raízes). |
