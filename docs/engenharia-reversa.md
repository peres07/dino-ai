# Descompilação com REA / Hopper

Alvo: `arquivos/Dino IA Hardmode.exe`, PE32 x86, SHA-256 `803541bcc8af279b957e69359dae4a3c9779da040669aa41c9aa7aeb5cc62383`. Os símbolos COFF permitiram nomear as rotinas analisadas. O [pseudocódigo selecionado](pseudocodigo-rea.txt) é saída do descompilador, não código-fonte recuperado. Os registros estáticos do REA foram exportados em `build/rea-dino.json` e `build/rea-dino-followup.json`.

| Rotina / endereço | Observação | Evidência REA |
| --- | --- | --- |
| `criarExperimentoDinoPlus` / `0x405d30` | População inicial de 500, 50 pais; rede de mira `4→4→3`, além de carregar `redeDesviando` separadamente. | `ev_109a436bdc7ef01dae93f3e659af0e5dc6bd65d385fc626e1df1568979929bd1` |
| `redeNeural::salvarEmArquivo` / `0x440b20` | Cabeçalho com sete inteiros de 32 bits, seguido por pesos de 64 bits, agrupados por camada e neurônio. A ordem e a contagem foram corroboradas pelos arquivos originais. | `ev_74cd7a04069742a38cbe4be7aff118b13e47767123d855ba6b02584f824682d9` |
| `redeNeural::alocarRedeNeural` / `0x440d70` | Camadas, viés e funções de ativação. Os índices `0…4` correspondem a ReLU, sigmoide, tanh, Leaky ReLU e linear; o despacho foi conferido na tabela do binário. | `ev_7bc19e080993c4a758b384e39c099a4677eaa15504a6604db1700b7f9ddc325b` |
| `experimentoDINO::getValorSensores` / `0x448580` | A rede de mira usa distância, stamina de tiro, cosseno e seno do ângulo relativo ao pássaro; a rede de desvio tem dez sensores. | `ev_11115495dbb33594fdc6c944238cf51df84663fef49bed16f953b99c01cc9395` |
| `experimentoDINO::getNomesOutputs` / `0x4476a0` e `inserirAcoes` / `0x447130` | Três saídas de mira/disparo, sete de movimento; disparo da rede quando a terceira saída supera `0,5`. | `ev_8922afc07dd7184479f2ae0533acd1926ac235369518292b42bfab828976f24a`, `ev_8c97551cf37e0d2bd9d6d3a9ff861dede2402c28fa525ae5e9517a1ddcea25be` |
| `estrategiaEvolutiva::criarGeracao` / `0x44a530` | Identificada a rotina de evolução; o port não replica a sequência aleatória nem toda a estratégia da DLL/biblioteca do original. | `ev_4acf6d6052362d895958380d2b4f9f793afa30a26ca54c33658d2fd3315215a1` |
| `experimento::desenhar` / `0x445100`, `desenharInformacoesGeraisTreino` / `0x443d70` e `desenharInformacoesGeraisTeste` / `0x443990` | Rotinas de desenho e divisão de informações por modo. O posicionamento visual foi reconstruído a partir de capturas. | `ev_50a7821828874e878c4429a49e513265cfcda05c4e5c45873ef4e5c710f096c7`, `ev_6cd28264c4cb41695b999de628cc9f453ebbf09580bfa3d8dae626c464791879`, `ev_9769b056facb7e5dd151618ca0aa72508925a58ebd1a94b2f044428e4998f2cf` |

## O que foi verificado

- **Referência visual observada:** o `.exe` foi executado em Wine 11.22, numa cópia isolada com Xvfb a 1920×1080. As capturas em `build/native-reference/shots/` mostram o treino, os gráficos ampliados, a lista de redes e o modo teste. Delas vieram as proporções, cores, disposição, texto e ordem visual; os sprites e a fonte vieram dos arquivos do pacote.
- **Treino no navegador:** `Instruções.txt` identifica `redeDesviando` como pré-treinada para o treino da rede que atira. O construtor decompilado corrobora a carga separada (`ev_469cac7d44d38063ac857277758e265da3982838c0b869ed147347785af3f8cc`). O treino completo solicitado para a web mantém uma cópia de referência e inicializa as demais com variações; seleção e mutação operam também sobre os pesos de movimento. Arquivos `.dino` incluem as duas redes legadas sob um cabeçalho `DNN1` (duas medidas de comprimento `uint32` little-endian seguidas pelos dois blocos).
- **Testes Wasm:** as 12 redes de mira e a rede de desvio carregam; no treino completo, as posições reais divergem. Um teste com rede de movimento zerada verifica que o modo teste usa o movimento do `.dino`, não a rede fixa. Foram verificados também os recordes, duas partidas por geração, as repetições do modo teste e os controles.
- **Chromium:** 21 passos passaram, incluindo abertura dos gráficos, teste de rede original e da rede atual de treino e persistência do `.dino` após recarregar; não houve eventos de erro (`ev_b4c4c0558d121190c1c3a5a87284664a8cc2df672d7d17da97545c0bb04fa705`).
- **Sem paridade comprovada:** a comparação com Wine foi visual e navegacional, não uma comparação quadro a quadro da simulação. Física, colisões, pontuação, dificuldade, sequência aleatória e seleção/mutação continuam reconstruídas. O diálogo de ajustes, a persistência no navegador e o treino completo são diferenças deliberadas.

Para extrair novamente o pseudocódigo de um pacote exportado pelo REA:

```sh
python3 scripts/rea_pseudocode.py build/rea-dino.json docs/pseudocodigo-rea.txt
```
