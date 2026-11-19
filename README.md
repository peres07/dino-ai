# Dino IA Hardmode no navegador

O jogo inicia no treinamento. A simulação, as duas redes neurais, a evolução e a construção das visualizações rodam em **WebAssembly**. O JavaScript liga o módulo às APIs do navegador: teclado, temporização, Canvas, arquivos e IndexedDB. A versão web não executa o `.exe`, não precisa de Windows e não usa servidor de treinamento.

## Abrir

Se `dist/` já estiver compilada, sirva-a diretamente com Python 3:

```sh
python3 -m http.server 8000 --bind 127.0.0.1 --directory dist
```

Abra **http://127.0.0.1:8000/** para a edição **Com pássaros**, ou **http://127.0.0.1:8000/sem-passaros/** para **Sem pássaros**. O cabeçalho alterna entre as duas. Não abra `index.html` com `file://`, pois o navegador precisa buscar o Wasm, as imagens e as redes. Para publicar, envie somente `dist/` para uma hospedagem estática; as rotas também funcionam sob uma subpasta.

## Publicar na VPS

Pré-requisitos: Git, Docker com o plugin `docker compose` e acesso ao repositório privado `peres07/dino-ai`. Para clonar por SSH, a chave da VPS precisa ter acesso ao projeto no GitHub. Uma **deploy key somente leitura** é suficiente.

```sh
git clone git@github.com:peres07/dino-ai.git
cd dino-ai
docker compose up -d --build --wait
```

Abra `http://IP_DA_VPS:8080/` (com pássaros) ou `http://IP_DA_VPS:8080/sem-passaros/`. Para acessar diretamente de fora, libere a porta TCP 8080 no firewall da VPS e/ou nas regras do provedor, se necessário. O contêiner compila o Wasm e serve só os arquivos estáticos via Nginx; não exige Clang, Python ou Node.js instalados na VPS.

Para usar um domínio com HTTPS, configure um proxy reverso na VPS (por exemplo, Caddy ou Nginx) apontando para `127.0.0.1:8080`. Crie um `.env` ao lado do `compose.yaml` e execute `docker compose up -d` novamente:

```dotenv
DINO_BIND=127.0.0.1
DINO_PORT=8080
```

Para atualizar: `git pull --ff-only` e `docker compose up -d --build --wait`. Para ver o estado ou logs: `docker compose ps` e `docker compose logs -f dino`. A VPS não armazena os recordes: eles ficam no navegador, separados por edição e endereço do site. Exporte-os antes de trocar de domínio, protocolo ou porta.

### Compilar ou recompilar

Dependências: `make`, Clang com suporte a `wasm32`, LLVM `wasm-ld` no PATH (ou `WASM_LD=/caminho/wasm-ld`), Python 3 e Pillow. Se disponível, o Makefile também aceita o `rust-lld` do Rust como linker, sem usar Rust no código.

```sh
python3 -m pip install Pillow  # use um ambiente virtual se necessário
make serve                     # gera dist/ e inicia o servidor local
make test                      # testes do módulo compilado; precisa de Node.js
```

## Controles e redes

- **Com pássaros:** o padrão é **Só disparo** com a rede de movimento fixa `redeDesviando`, como no original. Para treinar a sua, selecione **1 · Só movimento** em **Ajustes → Objetivo**, aplique e use **Salvar melhor rede de movimento**. Depois escolha **2 · Só disparo** e indique a rede salva no campo **Rede de movimento fixa**. O modo conjunto continua disponível. Trocar de etapa ou de rede fixa reinicia a população e o gráfico, mas mantém os arquivos salvos.
- **Sem pássaros:** treino com a rede de movimento de 10 entradas, duas camadas de 15 neurônios e 7 saídas. Inicia com 1000 indivíduos e 100 pais. Não usa `redeDesviando`, mira, armas ou pássaros atacantes; os obstáculos do cenário usam os sprites dessa edição.
- **Treino:** em **Ajustes**, altere população, pais, mutação e limite por partida. Os ajustes entram na próxima geração.
- **Controles e visualizações:** `N`/`M` mudam a velocidade; `Esc` desliga a renderização do cenário para acelerar; `P` pausa. `W`/`A`/`S`/`D` movem a câmera, `Q`/`E` dão zoom e Espaço restaura a câmera. `R` amplia a rede de mira, `V` a de movimento, `F` alterna a média móvel e `G`/`H` ampliam o gráfico. `U` alterna as informações. O histórico guarda até 8.192 gerações.
- **Teste:** pressione Enter para abrir a lista. Escolha a melhor rede em treinamento ou uma rede salva. As setas controlam o dino ciano; `Z` dá pulo duplo, `X` superpulo, `C` dash; `A`/`D` miram, `S` dispara e `T` reinicia a partida. O treino não avança enquanto o teste estiver ativo. Enter volta ao treino.
- **Arquivos:** aparecem 12 redes originais de mira na edição **Com pássaros** e 19 de movimento em **Sem pássaros**. O modo **Só movimento** salva `.mov` (rede de movimento no formato legado); **Só disparo** com `redeDesviando` original salva a rede de mira no formato legado; **Só disparo** com rede escolhida ou o modo conjunto salva `.dino`, que inclui as duas redes. Também é possível importar para a edição com pássaros uma rede de movimento exportada pela edição sem pássaros. Cada edição tem seu próprio banco local. Recarregar preserva as redes salvas, não a população nem o gráfico.
- **Interface:** o HUD usa os mesmos formatos do original (`TA: 1.000x`, `0h:0m:0s`), três faixas de serra, moeda e chão nas proporções dos arquivos originais. A mira acompanha o sinal do original: `A` sobe e `D` desce.

## Relação com o original

Os pacotes contêm executáveis, DLLs, imagens e redes, **não o código C/C++**. As interfaces foram reconstruídas com imagens e fontes dos respectivos pacotes. Os binários originais não foram modificados. Veja a [revisão das duas edições](docs/revisao-das-edicoes.md), com as rotinas descompiladas, comparações e limites de fidelidade. Não é uma reprodução 1:1: uma rede compatível pode obter outra pontuação.
