#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TAM       7             /* tamanho da tabela (de preferência primo) */
#define MAX_CHAVE 32
#define BASE      257ULL
#define MOD       1000000007ULL

/* ------------------------------------------------------------------ */
/*  Nó e lista circular simples                                       */
/* ------------------------------------------------------------------ */

typedef struct No {
    char chave[MAX_CHAVE];
    int valor;
    struct No *prox;
} No;

/* A lista circular é representada por um ponteiro para o ÚLTIMO nó.
   fim == NULL      -> lista vazia
   fim->prox        -> primeiro nó
   último->prox     -> primeiro nó (fecha o círculo) */
typedef No *ListaCircular;

No *lc_buscar(ListaCircular fim, const char *chave) {
    if (fim == NULL)
        return NULL;

    No *p = fim->prox;
    do {
        if (strcmp(p->chave, chave) == 0)
            return p;
        p = p->prox;
    } while (p != fim->prox);

    return NULL;
}

int lc_inserir_fim(ListaCircular *fim, const char *chave, int valor) {
    No *novo = malloc(sizeof(No));
    if (novo == NULL)
        return 0;

    strncpy(novo->chave, chave, MAX_CHAVE - 1);
    novo->chave[MAX_CHAVE - 1] = '\0';
    novo->valor = valor;

    if (*fim == NULL) {
        novo->prox = novo;              /* único nó aponta para si mesmo */
    } else {
        novo->prox = (*fim)->prox;      /* novo -> primeiro */
        (*fim)->prox = novo;            /* antigo último -> novo */
    }
    *fim = novo;                        /* novo é o último */
    return 1;
}

int lc_remover(ListaCircular *fim, const char *chave) {
    if (*fim == NULL)
        return 0;

    No *ant = *fim;
    No *p = (*fim)->prox;
    do {
        if (strcmp(p->chave, chave) == 0) {
            if (p == ant) {             /* único nó */
                *fim = NULL;
            } else {
                ant->prox = p->prox;
                if (p == *fim)          /* removeu o último */
                    *fim = ant;
            }
            free(p);
            return 1;
        }
        ant = p;
        p = p->prox;
    } while (ant != *fim);

    return 0;
}

int lc_tamanho(ListaCircular fim) {
    if (fim == NULL)
        return 0;

    int n = 0;
    No *p = fim->prox;
    do {
        n++;
        p = p->prox;
    } while (p != fim->prox);
    return n;
}

void lc_imprimir(ListaCircular fim) {
    if (fim == NULL) {
        printf("vazia\n");
        return;
    }
    No *p = fim->prox;
    do {
        printf("%s:%d -> ", p->chave, p->valor);
        p = p->prox;
    } while (p != fim->prox);
    printf("(volta ao inicio)\n");
}

void lc_liberar(ListaCircular *fim) {
    if (*fim == NULL)
        return;

    No *p = (*fim)->prox;
    (*fim)->prox = NULL;                /* quebra o círculo */
    while (p != NULL) {
        No *prox = p->prox;
        free(p);
        p = prox;
    }
    *fim = NULL;
}

/* ------------------------------------------------------------------ */
/*  Hash polinomial (método de Horner)                                */
/* ------------------------------------------------------------------ */

uint64_t hash_polinomial(const char *s) {
    uint64_t h = 0;
    for (; *s != '\0'; s++)
        h = (h * BASE + (unsigned char)*s) % MOD;
    return h;
}

unsigned int indice(const char *chave) {
    return (unsigned int)(hash_polinomial(chave) % TAM);
}

/* ------------------------------------------------------------------ */
/*  Tabela hash com ENCADEAMENTO SEPARADO                             */
/*  Cada posição do vetor guarda uma lista circular independente;     */
/*  chaves que colidem ficam todas na mesma lista.                    */
/* ------------------------------------------------------------------ */

typedef struct {
    ListaCircular lista[TAM];
    int qtd;
} TabelaHash;

void th_iniciar(TabelaHash *t) {
    for (int i = 0; i < TAM; i++)
        t->lista[i] = NULL;
    t->qtd = 0;
}

/* Retorna 1 se inseriu, 2 se só atualizou o valor, 0 se deu erro */
int th_inserir(TabelaHash *t, const char *chave, int valor) {
    int i = indice(chave);

    No *existente = lc_buscar(t->lista[i], chave);
    if (existente != NULL) {
        existente->valor = valor;
        return 2;
    }
    if (!lc_inserir_fim(&t->lista[i], chave, valor))
        return 0;

    t->qtd++;
    return 1;
}

No *th_buscar(TabelaHash *t, const char *chave) {
    return lc_buscar(t->lista[indice(chave)], chave);
}

int th_remover(TabelaHash *t, const char *chave) {
    if (lc_remover(&t->lista[indice(chave)], chave)) {
        t->qtd--;
        return 1;
    }
    return 0;
}

void th_imprimir(TabelaHash *t) {
    for (int i = 0; i < TAM; i++) {
        printf("[%d] (n=%d) ", i, lc_tamanho(t->lista[i]));
        lc_imprimir(t->lista[i]);
    }
    printf("elementos: %d | fator de carga: %.2f\n", t->qtd, (double)t->qtd / TAM);
}

void th_liberar(TabelaHash *t) {
    for (int i = 0; i < TAM; i++)
        lc_liberar(&t->lista[i]);
    t->qtd = 0;
}

/* ------------------------------------------------------------------ */

int main(void) {
    TabelaHash t;
    th_iniciar(&t);

    const char *chaves[] = { "cba", "bac", "A1", "user@01", "123", "user@10", "321", "1A", "#9" };
    int n = sizeof(chaves) / sizeof(chaves[0]);

    for (int i = 0; i < n; i++)
        th_inserir(&t, chaves[i], (i + 1) * 10);

    printf("--- Tabela inicial ---\n");
    th_imprimir(&t);

    th_inserir(&t, "A1", 999);          /* chave repetida: atualiza o valor */
    No *r = th_buscar(&t, "A1");
    printf("\nBuscar A1: %d\n", r ? r->valor : -1);
    printf("Buscar XYZ: %s\n", th_buscar(&t, "XYZ") ? "achou" : "nao encontrado");

    printf("\nRemovendo user@01 (ultimo da lista), bac (meio), 123 e XYZ (inexistente)...\n");
    printf("user@01: %d | bac: %d | 123: %d | XYZ: %d\n",
           th_remover(&t, "user@01"), th_remover(&t, "bac"),
           th_remover(&t, "123"), th_remover(&t, "XYZ"));

    printf("\n--- Tabela final ---\n");
    th_imprimir(&t);

    th_liberar(&t);
    return 0;
}