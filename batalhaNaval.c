/* batalha_naval.c
   Jogo Batalha Naval - jogador vs computador
   Compile: gcc -o batalha_naval batalha_naval.c
   Execute: ./batalha_naval
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#define N 10

/* Códigos nas matrizes:
   0 = água
   1 = navio (não atingido)
   2 = tiro: água (erro)
   3 = tiro: navio (acerto)
*/

typedef struct {
    char nome[20];
    int tamanho;
    int partes_restantes;
} Navio;

/* Navios padrão */
Navio navios_padrao[] = {
    {"Porta-avioes", 5, 5},
    {"Navio-tanque", 4, 4}, /* "Battleship" */
    {"Cruiser", 3, 3},
    {"Submarino", 3, 3},
    {"Destroyer", 2, 2}
};
const int NUM_NAVIOS = sizeof(navios_padrao) / sizeof(navios_padrao[0]);

/* Funções utilitárias */
void inicializa_tabuleiro(int tab[N][N]) {
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            tab[i][j] = 0;
}

void imprime_coluna_indices() {
    printf("   ");
    for (int j = 0; j < N; ++j) printf(" %2d", j+1);
    printf("\n");
}

/* Mostra o tabuleiro do jogador (com navios visíveis) */
void imprime_tabuleiro_jogador(int tab[N][N]) {
    imprime_coluna_indices();
    for (int i = 0; i < N; ++i) {
        printf(" %c ", 'A' + i);
        for (int j = 0; j < N; ++j) {
            if (tab[i][j] == 0) printf("  ~");
            else if (tab[i][j] == 1) printf("  #"); /* navio não atingido */
            else if (tab[i][j] == 2) printf("  o"); /* tiro água */
            else if (tab[i][j] == 3) printf("  X"); /* acerto */
        }
        printf("\n");
    }
}

/* Mostra o tabuleiro do inimigo para o jogador (sem mostrar navios não atingidos) */
void imprime_tabuleiro_inimigo(int tab[N][N]) {
    imprime_coluna_indices();
    for (int i = 0; i < N; ++i) {
        printf(" %c ", 'A' + i);
        for (int j = 0; j < N; ++j) {
            if (tab[i][j] == 0 || tab[i][j] == 1) printf("  ~"); /* não revelado */
            else if (tab[i][j] == 2) printf("  o"); /* erro */
            else if (tab[i][j] == 3) printf("  X"); /* acerto */
        }
        printf("\n");
    }
}

/* Verifica se é possível colocar o navio sem colisão e dentro do tabuleiro */
int pode_colocar(int tab[N][N], int linha, int col, int tamanho, int horizontal) {
    if (horizontal) {
        if (col + tamanho > N) return 0;
        for (int j = col; j < col + tamanho; ++j)
            if (tab[linha][j] != 0) return 0;
    } else {
        if (linha + tamanho > N) return 0;
        for (int i = linha; i < linha + tamanho; ++i)
            if (tab[i][col] != 0) return 0;
    }
    return 1;
}

/* Coloca navio aleatoriamente */
void coloca_navio_aleatorio(int tab[N][N], int tamanho) {
    int linha, col, horizontal;
    do {
        linha = rand() % N;
        col = rand() % N;
        horizontal = rand() % 2;
    } while (!pode_colocar(tab, linha, col, tamanho, horizontal));

    if (horizontal) {
        for (int j = col; j < col + tamanho; ++j) tab[linha][j] = 1;
    } else {
        for (int i = linha; i < linha + tamanho; ++i) tab[i][col] = 1;
    }
}

/* Posiciona todos navios aleatoriamente */
void posiciona_frota_aleatoria(int tab[N][N], Navio fleet[]) {
    for (int k = 0; k < NUM_NAVIOS; ++k) {
        int tamanho = fleet[k].tamanho;
        coloca_navio_aleatorio(tab, tamanho);
        fleet[k].partes_restantes = tamanho;
    }
}

/* Converte entrada tipo "A5" ou "J10" para índices (linha,col). Retorna 1 se ok */
int parse_coord(const char *s, int *linha, int *col) {
    if (!s || strlen(s) < 2) return 0;
    char c = toupper(s[0]);
    if (c < 'A' || c > 'A' + N - 1) return 0;
    int n;
    if (sscanf(s+1, "%d", &n) != 1) return 0;
    if (n < 1 || n > N) return 0;
    *linha = c - 'A';
    *col = n - 1;
    return 1;
}

/* Tenta disparar no tabuleiro alvo; retorna:
   0 = já atirado antes naquela casa,
   1 = erro (água),
   2 = acerto
*/
int disparar(int alvo[N][N], int linha, int col) {
    if (alvo[linha][col] == 2 || alvo[linha][col] == 3) return 0; /* já atirado */
    if (alvo[linha][col] == 1) {
        alvo[linha][col] = 3; /* acerto */
        return 2;
    } else {
        alvo[linha][col] = 2; /* água */
        return 1;
    }
}

/* Atualiza contagem de partes restantes dos navios depois de um acerto:
   Procura por navio afundado verificando contagem de partes 1 (restantes) presentes no tabuleiro.
   Simples: recalcula para cada navio quantas células desse tamanho ainda existem — porém
   isso pode confundir quando há múltiplos navios do mesmo tamanho. Então, em vez disso,
   vamos detectar afundamento por procurar sequências contínuas atingidas. Para simplicidade,
   aqui atualizaremos a contagem de partes restantes a partir do tabuleiro: contamos células
   originais de navio (valor 3) que correspondem a cada navio tamanho. Como navios são únicos
   por colocação (mesmo tamanho pode se repetir—somente Submarino/Cruiser ambos 3: código lida
   sem fins perfeitos, mas para feedback de afundado simplificamos notificando quando não há
   mais células '1' ou '3' que pertençam a um navio).
*/
int conta_partes_restantes_no_tab(int tab[N][N]) {
    int cont = 0;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            if (tab[i][j] == 1) cont++;
    return cont;
}

/* Conta acertos no tabuleiro (valor 3) */
int conta_acertos_no_tab(int tab[N][N]) {
    int cont = 0;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            if (tab[i][j] == 3) cont++;
    return cont;
}

/* Verifica se toda a frota foi afundada (não há mais células 1) */
int frota_afundada(int tab[N][N]) {
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            if (tab[i][j] == 1) return 0;
    return 1;
}

/* IA simples: gera tiro aleatório não repetido */
void ia_dispara_aleatorio(int alvo[N][N], int *linha, int *col) {
    int l, c;
    do {
        l = rand() % N;
        c = rand() % N;
    } while (alvo[l][c] == 2 || alvo[l][c] == 3); /* já atirado */
    *linha = l;
    *col = c;
}

/* Mensagem de instruções */
void exibe_instrucoes() {
    printf("Bem-vindo(a) ao Batalha Naval (vs computador)!\n");
    printf("Tabuleiro: linhas A-J e colunas 1-10.\n");
    printf("Digite coordenadas no formato A5, J10, etc. Sem espaço.\n");
    printf("Seu tabuleiro mostra seus navios (#), seus acertos (X) e erros (o).\n");
    printf("O tabuleiro inimigo mostra apenas seus acertos (X) e erros (o).\n\n");
}

int main() {
    srand((unsigned) time(NULL));
    int tab_jog[N][N], tab_cpu[N][N];
    inicializa_tabuleiro(tab_jog);
    inicializa_tabuleiro(tab_cpu);

    /* Copiamos navios padrao para instância local (para partes_restantes) */
    Navio fleet_jog[NUM_NAVIOS];
    Navio fleet_cpu[NUM_NAVIOS];
    for (int i = 0; i < NUM_NAVIOS; ++i) {
        fleet_jog[i] = navios_padrao[i];
        fleet_cpu[i] = navios_padrao[i];
    }

    /* Posicionamento aleatório para os dois */
    posiciona_frota_aleatoria(tab_jog, fleet_jog);
    posiciona_frota_aleatoria(tab_cpu, fleet_cpu);

    exibe_instrucoes();

    int vez_jogador = 1;
    char entrada[20];
    int linha, col;
    int tiros_jog = 0, tiros_cpu = 0;

    while (1) {
        /* Verificar vitória antes do turno (caso) */
        if (frota_afundada(tab_cpu)) {
            printf("\nParabéns — você afundou toda a frota inimiga! Vitória!!!\n");
            break;
        }
        if (frota_afundada(tab_jog)) {
            printf("\nSuas embarcações foram todas afundadas. Você perdeu.\n");
            break;
        }

        if (vez_jogador) {
            printf("\n=== Seu tabuleiro ===\n");
            imprime_tabuleiro_jogador(tab_jog);
            printf("\n=== Tabuleiro inimigo ===\n");
            imprime_tabuleiro_inimigo(tab_cpu);

            printf("\nSua vez — digite coordenada (ex: B7) ou 'sair': ");
            if (!fgets(entrada, sizeof(entrada), stdin)) {
                printf("Erro de leitura. Saindo.\n");
                break;
            }
            entrada[strcspn(entrada, "\r\n")] = 0; /* remove newline */
            if (strcasecmp(entrada, "sair") == 0) {
                printf("Jogo terminado pelo jogador.\n");
                break;
            }
            if (!parse_coord(entrada, &linha, &col)) {
                printf("Entrada inválida. Tente novamente.\n");
                continue;
            }
            int res = disparar(tab_cpu, linha, col);
            if (res == 0) {
                printf("Você já atirou nessa casa. Escolha outra.\n");
                continue;
            } else if (res == 1) {
                printf("Você acertou a água em %s — erro.\n", entrada);
                tiros_jog++;
                vez_jogador = 0; /* passa turno para IA */
            } else if (res == 2) {
                printf("Acertou! Você atingiu um navio em %s!\n", entrada);
                tiros_jog++;
                /* não muda vez: jogador pode voltar a atirar (opção de regra) */
                /* Aqui decidimos: jogador NÃO tem tiro extra — alterna turno */
                vez_jogador = 0;
            }
        } else {
            /* turno do computador (IA) */
            printf("\n--- Vez do computador ---\n");
            int l, c;
            ia_dispara_aleatorio(tab_jog, &l, &c);
            int res = disparar(tab_jog, l, c);
            char coord[6];
            sprintf(coord, "%c%d", 'A' + l, c + 1);
            if (res == 1) {
                printf("Computador errou em %s.\n", coord);
                tiros_cpu++;
                vez_jogador = 1;
            } else if (res == 2) {
                printf("Computador acertou em %s!\n", coord);
                tiros_cpu++;
                vez_jogador = 1; /* alterna turno */
            } else {
                /* não deveria acontecer por ia_dispara_aleatorio, mas só por segurança */
                continue;
            }
        }
    }

    printf("\nEstatísticas: Seu tiros = %d | Tiros do computador = %d\n", tiros_jog, tiros_cpu);
    printf("Obrigado por jogar!\n");
    return 0;
}


// Desafio Batalha Naval - MateCheck
// Este código inicial serve como base para o desenvolvimento do sistema de Batalha Naval.
// Siga os comentários para implementar cada parte do desafio.


    // Nível Novato - Posicionamento dos Navios
    // Sugestão: Declare uma matriz bidimensional para representar o tabuleiro (Ex: int tabuleiro[5][5];).
    // Sugestão: Posicione dois navios no tabuleiro, um verticalmente e outro horizontalmente.
    // Sugestão: Utilize `printf` para exibir as coordenadas de cada parte dos navios.

    // Nível Aventureiro - Expansão do Tabuleiro e Posicionamento Diagonal
    // Sugestão: Expanda o tabuleiro para uma matriz 10x10.
    // Sugestão: Posicione quatro navios no tabuleiro, incluindo dois na diagonal.
    // Sugestão: Exiba o tabuleiro completo no console, mostrando 0 para posições vazias e 3 para posições ocupadas.

    // Nível Mestre - Habilidades Especiais com Matrizes
    // Sugestão: Crie matrizes para representar habilidades especiais como cone, cruz, e octaedro.
    // Sugestão: Utilize estruturas de repetição aninhadas para preencher as áreas afetadas por essas habilidades no tabuleiro.
    // Sugestão: Exiba o tabuleiro com as áreas afetadas, utilizando 0 para áreas não afetadas e 1 para áreas atingidas.

    // Exemplos de exibição das habilidades:
    // Exemplo para habilidade em cone:
    // 0 0 1 0 0
    // 0 1 1 1 0
    // 1 1 1 1 1
    
    // Exemplo para habilidade em octaedro:
    // 0 0 1 0 0
    // 0 1 1 1 0
    // 0 0 1 0 0

    // Exemplo para habilidade em cruz:
    // 0 0 1 0 0
    // 1 1 1 1 1
    // 0 0 1 0 0

 

