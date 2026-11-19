# Revisão das duas edições

## Referências

- **Com pássaros:** `arquivos/Dino IA Hardmode.exe` (`803541bc…`). A análise original, as capturas e as diferenças da versão web estão em [engenharia-reversa.md](engenharia-reversa.md).
- **Sem pássaros:** `Dino sem pássaros/arquivos/Dino IA Hardmode.exe` (`a9e69b5637a2ccd889c61b18bd85a8daabf32a556a206c8ebbcb27f74e0db44d`). A descompilação direta no Hopper via REA funcionou, com ajuda dos símbolos COFF. O [pseudocódigo selecionado](pseudocodigo-sem-passaros-rea.txt) não é o código-fonte original.

| Sem pássaros — rotina | Evidência REA | Observado no binário |
|---|---|---|
| `criarExperimentoDinoPlus`, `0x4060b0` | `ev_7228eac7ef3623282c862f873e606465b3afa4b93a35c224f79cb0f326e6b950` | 1000 indivíduos e 100 pais; topologia `10→15→15→7`; teste com dois personagens. |
| `getNomesOutputs`, `0x445b80`; `getValorSensores`, `0x446630` | `ev_218a02486158e7ab4412014242a623b497f3378017a259409be5dfa1d2abe58d`, `ev_e29a7e0a8c6792f8f446d7b9d661506dc8349cf6f3c90c2642c6423f5618ce17` | Sete saídas de movimento, nenhuma de mira ou disparo. |
| `salvarEmArquivo`, `0x440310` | `ev_b8c4d44622723f608472ddab79f2fe24fd7af8acb3fd0e2743bcf74accd624bc` | Cabeçalho e pesos. As 19 redes empacotadas têm 4164 bytes, cada uma com 517 pesos. |
| `obstaculo::alocarSprites`, `0x44d4a0`; `prepararObstaculo`, `0x44dd20` | `ev_310e72256750cbce76ab4f0c74eef4bd4f9dfbf230dea7bc0f66cf79074294a2`, `ev_441f4a833f88e098504f6ff7c8806c71bc67e3af881cbcd04b73c66130260990` | Ainda há sprites de pássaros na inicialização de obstáculos. Isso não demonstra sua frequência em jogo. O port sem pássaros não gera aves; o catálogo completo de obstáculos nativos não foi reproduzido. |
| `adicionarPontuacaoDinos`, `0x449af0`; `criarGeracao`, `0x448190` | `ev_682047023c0ffe1055e6652af75c87eaa809afeb4f04bcb642ed096362eb7c59`, `ev_b46855aded030ed9ca459340029cbc9114b78e17faa2bcc81175e55fcef16e62` | Pontuação e estratégia evolutiva próprias, não apenas uma opção de ocultar o pássaro da outra edição. |
| `experimento::desenhar`, `0x443c40` | `ev_5bd426cc5b752d022a396ea0c60bd5a02d0235b11c1babd2578529c0990e25aa` | Desenho do ambiente e da interface. |

## Port nativo vs. execução do Windows

As rotas do port executam C recompilado para Wasm. Nenhum `.exe`, DLL ou emulador é distribuído no site. Como verificação adicional, tentei executar o segundo `.exe` inalterado no navegador com Boxedwine 26R1. A execução avançou até a abertura da janela, mas falhou por falta de suporte a OpenGL no runtime web (`No matching GL pixel format available`). `PIG_criarJanela` passa `0x6` a `SDL_CreateWindow`, que inclui `SDL_WINDOW_OPENGL` (`ev_aef898b423d71839dc6323d3bee0afbb03254bdc5ea84356206851323257ba78`). O teste não é prova de que toda abordagem de emulação falharia; essa configuração não fornece uma versão idêntica utilizável.

## O que foi conferido e corrigido

- **Interface:** comparação com capturas de Wine/Xvfb a 1920×1080, em `build/native-reference/shots/` e `build/birdless-reference/shots/`. A edição sem pássaros mostra o painel de uma partida por geração e a rede de movimento; as ampliações e o histograma seguem as capturas. Os sprites de neurônio dessa edição são `neuronio2.png`, `neuronio4.png`, `luz.png` e `entrada.png` (`ev_644e753541be41c82e81441db1e730d4de6c3f58964f932e1bcd2064a7dfcfbe`).
- **Simulação:** na edição sem pássaros, as sete ações são evoluídas e os arquivos exportados são redes de movimento legadas. Na edição com pássaros, os testes mantêm os dois modos de treino e a leitura dos arquivos `.dino` e legados. A revisão restaurou a câmera em teste na edição sem pássaros, a redução de velocidade abaixo de `1×`, a rolagem do gráfico livre e a proteção contra seleção sem pais válidos.
- **Distribuição e armazenamento:** `make test` passou nos 13 testes do Wasm e da distribuição. Eles conferem também os 31 arquivos legados, a origem da fonte e dos sprites e os links das duas rotas quando hospedadas sob uma subpasta. Em Chromium, troca de versão, menu, atalhos de gráficos e teste passaram em 23 passos (`ev_4556894790d80ec448317cdd7092bdcad1fabbf2a2a520f8758b728e32128480`). Um segundo cenário validou a persistência do recorde sem pássaros e que ele não aparece na outra rota (`ev_915b46da4f3b059e4f49b909e75fd7792ae5598c6f99f6fe9d3e5e49e61e0cc8`).

## Critério de paridade

Os manuais, as rotinas descompiladas, os arquivos legados e as capturas servem como referência. Compatibilidade de arquivos não prova que a IA toma a mesma sequência de decisões, que o mesmo recorde é obtido ou que a imagem é idêntica. O seletor de edição, os ajustes web, a persistência no navegador e a evolução conjunta opcional na edição com pássaros são diferenças deliberadas. Paridade integral de física, RNG, estratégia de evolução e desenho exigiria uma comparação determinística, quadro a quadro, ou os projetos-fonte completos; não está certificada.

## Movimento, colisão e treino em duas etapas

| Evidência | Aplicação no port |
|---|---|
| Ambos os `processarAmbiente` (`0x446a20` sem pássaros, `0x448b50` com pássaros), `ev_e0a5d3d5abf7d89699d18ef968f5d772ca7f31ac498270cf773d9cde9eea8e29` / `ev_d95e7789eb04935a349112d41e493bbd355c5720e4d4b62651324d456dad206c`. Clamp e inversão observados nas sequências `0x4471fa` e `0x4493d0`. | Rolagem começa em 5 pixels nativos/tick, oscila entre 5 e 10 e muda 0,001 por tick; conversão única para o canvas. |
| Ações e constantes nos dois PEs (`0x4456e0` / `0x447130`), incluindo as inicializações de salto `9` e movimento à esquerda `−4,5`. | Um integrador compartilhado integra os comandos de movimento, com impulso, gravidade e desaceleração do dash separados da mira. Render e testes de colisão usam a mesma pose. |
| Obstáculos e colisão (`0x44dd20`, `0x449f50`, `0x44c400`), `ev_441f4a833f88e098504f6ff7c8806c71bc67e3af881cbcd04b73c66130260990`, `ev_85769594f6da93eb6289224100109df6461dcb973909d4f770f8aa382ce79edf` / `ev_f8f042fa2acd5e38cafd85a8bcf5c1f9f419b0f75920901654e61a1f4d3a1bb7`. | A árvore e a caverna são obstáculos, com dimensões lógicas menores do que a imagem e camadas diante/atrás do dino. A caverna permite passar abaixado; a árvore tem uma janela vertical. As serras de `fundo0.bmp` e `fundo1.bmp` continuam decorativas. A fila agora usa as larguras lógicas para espaçar obstáculos. Partes decorativas da copa não são hitbox. |

O modo **Só movimento** da edição com pássaros exporta exclusivamente os pesos de movimento. **Só disparo** usa uma cópia imutável da rede escolhida e evolui apenas os pesos de mira/disparo. O teste de regressão compara os bytes da rede fixa antes e depois da evolução. `make test` também exercita salto, dash, limites de velocidade e colisões de árvore e caverna. A distribuição dos obstáculos, as regras detalhadas de stamina, o tempo real por tick e as colisões do pássaro atacante/projéteis não têm paridade integral medida com os PEs.
