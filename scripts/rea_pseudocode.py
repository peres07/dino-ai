"""Export the game routines from a REA evidence bundle as readable text."""
import argparse
import json
from pathlib import Path

ROUTINES = {
    '0x405d30': 'criarExperimentoDinoPlus()',
    '0x440b20': 'redeNeural::salvarEmArquivo(char*)',
    '0x440d70': 'redeNeural::alocarRedeNeural(int, int, int, int, int, int, int)',
    '0x4418e0': 'experimento::processarRedeNeural(int)',
    '0x441a90': 'experimento::prepararTreino()',
    '0x447130': 'experimentoDINO::inserirAcoes(int, double*, int)',
    '0x4476a0': 'experimentoDINO::getNomesOutputs(char**, int)',
    '0x448580': 'experimentoDINO::getValorSensores(int, double*, int)',
    '0x449a40': 'experimentoDINO::getFitnessIndividuo(int)',
    '0x44a530': 'estrategiaEvolutiva::criarGeracao()',
    '0x44c9f0': 'game::getDiferencaAnguloArmaPassaro(int)',
    '0x44caf0': 'game::game(int, int)',
}

BIRDLESS = {
    '0x401870': 'PIG_criarJanela(char*, int, int)',
    '0x4060b0': 'criarExperimentoDinoPlus()',
    '0x4456e0': 'experimentoDINO::inserirAcoes(int, double*)',
    '0x445b80': 'experimentoDINO::getNomesOutputs(char**)',
    '0x446630': 'experimentoDINO::getValorSensores(int, double*)',
    '0x4476b0': 'experimentoDINO::getFitnessIndividuo(int)',
    '0x448190': 'estrategiaEvolutiva::criarGeracao()',
    '0x449af0': 'game::adicionarPontuacaoDinos()',
    '0x44d4a0': 'obstaculo::alocarSprites()',
    '0x44dd20': 'obstaculo::prepararObstaculo(int, int)',
    '0x440310': 'redeNeural::salvarEmArquivo(char*)',
    '0x443c40': 'experimento::desenhar(int, int, double)',
}
EDITIONS = {
    '803541bcc8af279b957e69359dae4a3c9779da040669aa41c9aa7aeb5cc62383': ('com pássaros', ROUTINES),
    'a9e69b5637a2ccd889c61b18bd85a8daabf32a556a206c8ebbcb27f74e0db44d': ('sem pássaros', BIRDLESS),
}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('bundle', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()

records = json.loads(args.bundle.read_text(encoding='utf-8'))['records']
routines = {}
for record in records:
    if record['operation'] != 'analyze_function':
        continue
    sha = record['subject']['digest']['sha256']
    if sha not in EDITIONS:
        continue
    addr = record['parameters'].get('procedure')
    if addr in EDITIONS[sha][1]:
        if record['normalized_result']['pseudocode']:
            routines[sha,addr] = record

if not routines:
    parser.error('O pacote não contém pseudocódigo das rotinas reconhecidas.')

with args.output.open('w', encoding='utf-8') as output:
    output.write('Dino IA Hardmode — pseudocódigo de análise estática gerado pelo REA\n'
                 'Não é o código-fonte original nem um arquivo compilável.\n'
                 'Os nomes dos cabeçalhos vêm dos símbolos COFF do executável.\n')
    for (sha,addr), record in sorted(routines.items()):
        pseudo = record['normalized_result']['pseudocode']
        edition, names = EDITIONS[sha]
        output.write(f'\n{addr} {names[addr]} ({edition})\nSHA-256: {sha}\n'
                     f'Evidência: {record["evidence_id"]}\n'
                     f'Provedor: {record["provider"]["id"]}\n\n{pseudo}\n')
