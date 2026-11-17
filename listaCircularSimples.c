#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM 7

/* Nó da lista circular simples */
typedef struct No {
    int chave;
    char nome[32];
    struct No *prox;
} No;

/* Cada posição da tabela guarda um ponteiro para o ÚLTIMO nó da lista circular.
   Assim: fim->prox é o primeiro nó, e inserir no final é O(1). */
typedef struct {
    No *fim[TAM];
} TabelaHash;

void iniciar(TabelaHash *t) {
    for (int i = 0; i < TAM; i++)
        t->fim[i] = NULL;
}

int hash(int chave) {
    return ((chave % TAM) + TAM) % TAM; /* funciona também para chaves negativas */
}

No *buscar(TabelaHash *t, int chave) {
    No *fim = t->fim[hash(chave)];
    if (fim == NULL)
        return NULL;

    No *p = fim->prox; /* primeiro nó */
    do {
        if (p->chave == chave)
            return p;
        p = p->prox;
    } while (p != fim->prox);

    return NULL;
}

void inserir(TabelaHash *t, int chave, const char *nome) {
    No *existente = buscar(t, chave);
    if (existente != NULL) { /* chave repetida: só atualiza o nome */
        strncpy(existente->nome, nome, sizeof(existente->nome) - 1);
        existente->nome[sizeof(existente->nome) - 1] = '\0';
        return;
    }

    No *novo = malloc(sizeof(No));
    if (novo == NULL) {
        printf("Erro de memoria!\n");
        return;
    }
    novo->chave = chave;
    strncpy(novo->nome, nome, sizeof(novo->nome) - 1);
    novo->nome[sizeof(novo->nome) - 1] = '\0';

    int i = hash(chave);
    if (t->fim[i] == NULL) {
        novo->prox = novo;              /* único nó aponta para si mesmo */
    } else {
        novo->prox = t->fim[i]->prox;   /* novo aponta para o primeiro */
        t->fim[i]->prox = novo;         /* antigo último aponta para o novo */
    }
    t->fim[i] = novo;                   /* novo passa a ser o último */
}

int remover(TabelaHash *t, int chave) {
    int i = hash(chave);
    No *fim = t->fim[i];
    if (fim == NULL)
        return 0;

    No *ant = fim;
    No *p = fim->prox;
    do {
        if (p->chave == chave) {
            if (p == ant) {             /* era o único nó da lista */
                t->fim[i] = NULL;
            } else {
                ant->prox = p->prox;
                if (p == fim)           /* removeu o último: ant vira o último */
                    t->fim[i] = ant;
            }
            free(p);
            return 1;
        }
        ant = p;
        p = p->prox;
    } while (ant != fim);

    return 0;
}

void imprimir(TabelaHash *t) {
    for (int i = 0; i < TAM; i++) {
        printf("[%d] ", i);
        No *fim = t->fim[i];
        if (fim == NULL) {
            printf("vazia\n");
            continue;
        }
        No *p = fim->prox;
        do {
            printf("(%d, %s) -> ", p->chave, p->nome);
            p = p->prox;
        } while (p != fim->prox);
        printf("(volta ao inicio)\n");
    }
}

void liberar(TabelaHash *t) {
    for (int i = 0; i < TAM; i++) {
        No *fim = t->fim[i];
        if (fim == NULL)
            continue;

        No *p = fim->prox;
        fim->prox = NULL;               /* quebra o círculo para poder percorrer até NULL */
        while (p != NULL) {
            No *prox = p->prox;
            free(p);
            p = prox;
        }
        t->fim[i] = NULL;
    }
}

int main(void) {
    TabelaHash t;
    iniciar(&t);

    inserir(&t, 10, "Ana");
    inserir(&t, 17, "Bruno");   /* 10 e 17 colidem na posição 3 */
    inserir(&t, 24, "Carla");   /* também posição 3 */
    inserir(&t, 5,  "Diego");
    inserir(&t, 12, "Elisa");   /* colide com o 5 na posição 5 */

    printf("--- Tabela inicial ---\n");
    imprimir(&t);

    No *r = buscar(&t, 17);
    printf("\nBuscar 17: %s\n", r ? r->nome : "nao encontrado");

    printf("\nRemovendo 17 e 5...\n");
    remover(&t, 17);
    remover(&t, 5);

    printf("\n--- Tabela final ---\n");
    imprimir(&t);

    liberar(&t);
    return 0;
}