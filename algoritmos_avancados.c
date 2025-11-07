#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* =========================
   Utilidades de strings
   ========================= */
static char* str_clone(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (!p) { perror("malloc"); exit(1); }
    memcpy(p, s, n);
    return p;
}

/* =========================
   BST de Pistas (ordenadas)
   ========================= */
typedef struct NoPista {
    char* texto;
    struct NoPista* esq;
    struct NoPista* dir;
} NoPista;

static NoPista* bst_inserir(NoPista* r, const char* texto) {
    if (!r) {
        r = (NoPista*)calloc(1, sizeof(NoPista));
        if (!r) { perror("calloc"); exit(1); }
        r->texto = str_clone(texto);
        return r;
    }
    int cmp = strcmp(texto, r->texto);
    if (cmp < 0) r->esq = bst_inserir(r->esq, texto);
    else if (cmp > 0) r->dir = bst_inserir(r->dir, texto);
    /* se igual, não insere duplicado */
    return r;
}

static void bst_em_ordem(NoPista* r) {
    if (!r) return;
    bst_em_ordem(r->esq);
    printf(" - %s\n", r->texto);
    bst_em_ordem(r->dir);
}

static void bst_destruir(NoPista* r) {
    if (!r) return;
    bst_destruir(r->esq);
    bst_destruir(r->dir);
    free(r->texto);
    free(r);
}

/* =======================================
   Tabela Hash de Suspeitos e suas Pistas
   ======================================= */
typedef struct ListaStr {
    char* s;
    struct ListaStr* prox;
} ListaStr;

typedef struct Suspeito {
    char* nome;
    int contador;          /* nº de associações (pistas citando) */
    ListaStr* pistas;      /* lista de pistas associadas (únicas) */
    struct Suspeito* prox; /* encadeamento do bucket */
} Suspeito;

#define HASH_TAM 31
static Suspeito* HT[HASH_TAM];

static unsigned hash_nome(const char* k) {
    /* soma simples dos ASCII com pequena mistura */
    unsigned h = 0;
    while (*k) {
        h = h * 131 + (unsigned char)(*k++);
    }
    return h % HASH_TAM;
}

static int lista_contem(ListaStr* L, const char* s) {
    for (; L; L = L->prox) if (strcmp(L->s, s) == 0) return 1;
    return 0;
}

static Suspeito* hash_buscar_ou_criar(const char* nome) {
    unsigned idx = hash_nome(nome);
    Suspeito* cur = HT[idx];
    while (cur) {
        if (strcmp(cur->nome, nome) == 0) return cur;
        cur = cur->prox;
    }
    /* criar */
    Suspeito* novo = (Suspeito*)calloc(1, sizeof(Suspeito));
    if (!novo) { perror("calloc"); exit(1); }
    novo->nome = str_clone(nome);
    novo->contador = 0;
    novo->pistas = NULL;
    novo->prox = HT[idx];
    HT[idx] = novo;
    return novo;
}

static void hash_associar_pista(const char* pista, const char* nomeSuspeito) {
    Suspeito* s = hash_buscar_ou_criar(nomeSuspeito);
    /* evitar duplicar a mesma pista no mesmo suspeito */
    if (!lista_contem(s->pistas, pista)) {
        ListaStr* n = (ListaStr*)calloc(1, sizeof(ListaStr));
        if (!n) { perror("calloc"); exit(1); }
        n->s = str_clone(pista);
        n->prox = s->pistas;
        s->pistas = n;
        s->contador++; /* conta citação/associação */
    }
}

static void hash_listar_associacoes(void) {
    printf("=== Suspeitos e Pistas Associadas ===\n");
    for (int i = 0; i < HASH_TAM; ++i) {
        for (Suspeito* s = HT[i]; s; s = s->prox) {
            printf("* %s (citacoes: %d)\n", s->nome, s->contador);
            for (ListaStr* L = s->pistas; L; L = L->prox) {
                printf("   - %s\n", L->s);
            }
        }
    }
}

static Suspeito* hash_suspeito_top(void) {
    Suspeito* best = NULL;
    for (int i = 0; i < HASH_TAM; ++i) {
        for (Suspeito* s = HT[i]; s; s = s->prox) {
            if (!best || s->contador > best->contador) best = s;
        }
    }
    return best;
}

static void hash_destruir(void) {
    for (int i = 0; i < HASH_TAM; ++i) {
        Suspeito* s = HT[i];
        while (s) {
            Suspeito* nxt = s->prox;
            free(s->nome);
            ListaStr* L = s->pistas;
            while (L) {
                ListaStr* ln = L->prox;
                free(L->s);
                free(L);
                L = ln;
            }
            free(s);
            s = nxt;
        }
        HT[i] = NULL;
    }
}

/* =========================
   Árvore Binária: Salas
   ========================= */
typedef struct Sala {
    char* nome;
    struct Sala* esq;
    struct Sala* dir;

    /* mecânica de pistas: opcional por sala */
    const char* pista;         /* pista coletada ao entrar (uma por simplicidade) */
    const char** suspeitos;    /* nomes de suspeitos a associar com esta pista */
    int qtdSuspeitos;

    int pistaColetada;         /* evita coletar repetidamente */
} Sala;

/* criação de sala */
static Sala* criar_sala(const char* nome,
                        const char* pista,
                        const char** suspeitos,
                        int qtdSuspeitos) {
    Sala* s = (Sala*)calloc(1, sizeof(Sala));
    if (!s) { perror("calloc"); exit(1); }
    s->nome = str_clone(nome);
    s->pista = pista;
    s->suspeitos = suspeitos;
    s->qtdSuspeitos = qtdSuspeitos;
    s->pistaColetada = 0;
    return s;
}

static void conectar(Sala* pai, Sala* esq, Sala* dir) {
    if (pai) { pai->esq = esq; pai->dir = dir; }
}

static void destruir_mapa(Sala* r) {
    if (!r) return;
    destruir_mapa(r->esq);
    destruir_mapa(r->dir);
    free(r->nome);
    /* suspeitos/pistas são literais/constantes, não free */
    free(r);
}

/* =========================
   Jogo / Exploração
   ========================= */
static void coletar_da_sala(Sala* s, NoPista** arvPistas) {
    if (!s || s->pistaColetada) return;
    if (s->pista && s->pista[0]) {
        *arvPistas = bst_inserir(*arvPistas, s->pista);
        for (int i = 0; i < s->qtdSuspeitos; ++i) {
            hash_associar_pista(s->pista, s->suspeitos[i]);
        }
        s->pistaColetada = 1;
        printf("[Pista coletada] %s\n", s->pista);
    }
}

static void ajuda(void) {
    printf("Comandos:\n");
    printf("  e - ir para a sala à esquerda\n");
    printf("  d - ir para a sala à direita\n");
    printf("  p - listar pistas coletadas (ordem alfabetica)\n");
    printf("  a - listar suspeitos e suas pistas\n");
    printf("  m - mostrar suspeito mais provavel\n");
    printf("  h - ajuda\n");
    printf("  s - sair da mansao\n");
}

static void explorar(Sala* raiz, NoPista** arvPistas) {
    Sala* atual = raiz;
    ajuda();
    while (1) {
        if (!atual) {
            printf("\nVoce esta em um corredor sem saida. Use 's' para sair ou volte com 'h' para ajuda.\n");
        } else {
            printf("\n[Local] %s\n", atual->nome);
            coletar_da_sala(atual, arvPistas);
            printf("Escolha: (e) esquerda  (d) direita  (p) pistas  (a) associacoes  (m) mais provavel  (h) ajuda  (s) sair\n");
        }

        int c = 0;
        /* consumir espaços/linhas */
        do { c = getchar(); } while (c != EOF && isspace(c));

        if (c == EOF) break;
        switch (tolower(c)) {
            case 'e':
                if (atual && atual->esq) atual = atual->esq;
                else printf("Nao ha sala à esquerda.\n");
                break;
            case 'd':
                if (atual && atual->dir) atual = atual->dir;
                else printf("Nao ha sala à direita.\n");
                break;
            case 'p':
                if (*arvPistas) {
                    printf("=== Pistas em ordem alfabetica ===\n");
                    bst_em_ordem(*arvPistas);
                } else {
                    printf("(Nenhuma pista coletada ainda)\n");
                }
                break;
            case 'a':
                hash_listar_associacoes();
                break;
            case 'm': {
                Suspeito* top = hash_suspeito_top();
                if (top && top->contador > 0) {
                    printf("Suspeito mais provavel: %s (citacoes: %d)\n", top->nome, top->contador);
                } else {
                    printf("Ainda nao ha suspeito predominante.\n");
                }
            } break;
            case 'h':
                ajuda();
                break;
            case 's':
                return;
            default:
                printf("Comando invalido. Use 'h' para ajuda.\n");
                break;
        }
    }
}

/* =========================
   Montagem do Cenário
   ========================= */
/*
   Mapa fixo:

                 [Hall de Entrada]
                   /            \
            [Biblioteca]       [Cozinha]
              /     \            /     \
       [Escritorio][Sotao] [Despensa] [Porão]

*/

int main(void) {
    NoPista* arvPistas = NULL;
    memset(HT, 0,
