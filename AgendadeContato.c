
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define CAPACIDADE_INICIAL 11
#define TEL_MIN  10
#define TEL_MAX  11
#define BASE     257ULL
#define MOD      1000000007ULL

enum { OK = 0, ERRO_TELEFONE_INVALIDO, ERRO_DUPLICADA, ERRO_NAO_ENCONTRADA, ERRO_MEMORIA };

/* ------------------------------------------------------------------ */
/*  Tipos                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    char nome[64];
    char email[64];
    char obs[128];
} Contato;

typedef struct No {
    char chave[TEL_MAX + 1];    /* telefone normalizado */
    Contato valor;
    struct No *prox;
} No;

/* Ponteiro para o ULTIMO no (NULL = lista vazia) */
typedef No *ListaCircular;

typedef struct {
    ListaCircular *lista;       /* vetor de listas circulares */
    int capacidade;
    int qtd;                    /* contatos armazenados */
    int colisoes;               /* insercoes em posicao ja ocupada */
} Agenda;

/* ------------------------------------------------------------------ */
/*  Utilitarios                                                       */
/* ------------------------------------------------------------------ */

static void copiar(char *dst, size_t tam, const char *src) {
    strncpy(dst, src, tam - 1);
    dst[tam - 1] = '\0';
}

/* Gera a chave normalizada. Retorna 1 se valida, 0 caso contrario.
   "saida" precisa ter pelo menos TEL_MAX + 1 bytes. */
int normalizar_telefone(const char *entrada, char *saida) {
    char dig[32];
    int n = 0;

    for (const char *p = entrada; *p != '\0'; p++) {
        if (isdigit((unsigned char)*p)) {
            if (n >= (int)sizeof(dig) - 1)
                return 0;
            dig[n++] = *p;
        } else if (strchr(" ()-+.", *p) == NULL) {
            return 0;               /* caractere que nao e separador */
        }
    }
    dig[n] = '\0';

    const char *ini = dig;
    if ((n == 12 || n == 13) && dig[0] == '5' && dig[1] == '5') {
        ini += 2;                   /* remove o codigo do pais */
        n -= 2;
    }
    if (n < TEL_MIN || n > TEL_MAX)
        return 0;

    memcpy(saida, ini, n);
    saida[n] = '\0';
    return 1;
}

/* Formata a chave para exibicao: (84) 99999-1234 */
static void formatar_telefone(const char *chave, char *out) {
    if (strlen(chave) == 11)
        snprintf(out, 20, "(%.2s) %.5s-%s", chave, chave + 2, chave + 7);
    else
        snprintf(out, 20, "(%.2s) %.4s-%s", chave, chave + 2, chave + 6);
}

/* ------------------------------------------------------------------ */
/*  Hash polinomial                                                   */
/* ------------------------------------------------------------------ */

uint64_t hash_polinomial(const char *s) {
    uint64_t h = 0;
    for (; *s != '\0'; s++)
        h = (h * BASE + (unsigned char)*s) % MOD;
    return h;
}

unsigned int indice(const Agenda *ag, const char *chave) {
    return (unsigned int)(hash_polinomial(chave) % (uint64_t)ag->capacidade);
}

/* ------------------------------------------------------------------ */
/*  Lista circular simples                                            */
/* ------------------------------------------------------------------ */

static No *lc_buscar(ListaCircular fim, const char *chave) {
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

static int lc_inserir_fim(ListaCircular *fim, const char *chave, const Contato *valor) {
    No *novo = malloc(sizeof(No));
    if (novo == NULL)
        return 0;

    copiar(novo->chave, sizeof(novo->chave), chave);
    novo->valor = *valor;

    if (*fim == NULL) {
        novo->prox = novo;              /* unico no aponta para si mesmo */
    } else {
        novo->prox = (*fim)->prox;      /* novo -> primeiro */
        (*fim)->prox = novo;            /* antigo ultimo -> novo */
    }
    *fim = novo;
    return 1;
}

static int lc_remover(ListaCircular *fim, const char *chave) {
    if (*fim == NULL)
        return 0;

    No *ant = *fim;
    No *p = (*fim)->prox;
    do {
        if (strcmp(p->chave, chave) == 0) {
            if (p == ant) {             /* era o unico no */
                *fim = NULL;
            } else {
                ant->prox = p->prox;
                if (p == *fim)          /* removeu o ultimo */
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

static int lc_tamanho(ListaCircular fim) {
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

static void lc_liberar(ListaCircular *fim) {
    if (*fim == NULL)
        return;

    No *p = (*fim)->prox;
    (*fim)->prox = NULL;                /* quebra o circulo */
    while (p != NULL) {
        No *prox = p->prox;
        free(p);
        p = prox;
    }
    *fim = NULL;
}

/* ------------------------------------------------------------------ */
/*  Agenda (tabela hash com encadeamento separado)                    */
/* ------------------------------------------------------------------ */

Agenda *agenda_criar(int capacidade) {
    if (capacidade <= 0)
        return NULL;

    Agenda *ag = malloc(sizeof(Agenda));
    if (ag == NULL)
        return NULL;

    ag->lista = calloc((size_t)capacidade, sizeof(ListaCircular));
    if (ag->lista == NULL) {
        free(ag);
        return NULL;
    }
    ag->capacidade = capacidade;
    ag->qtd = 0;
    ag->colisoes = 0;
    return ag;
}

void agenda_destruir(Agenda *ag) {
    if (ag == NULL)
        return;
    for (int i = 0; i < ag->capacidade; i++)
        lc_liberar(&ag->lista[i]);
    free(ag->lista);
    free(ag);
}

int inserir(Agenda *ag, const char *telefone, const Contato *valor) {
    char chave[TEL_MAX + 1];
    if (!normalizar_telefone(telefone, chave))
        return ERRO_TELEFONE_INVALIDO;

    unsigned int i = indice(ag, chave);
    if (lc_buscar(ag->lista[i], chave) != NULL)
        return ERRO_DUPLICADA;

    int ocupada = (ag->lista[i] != NULL);
    if (!lc_inserir_fim(&ag->lista[i], chave, valor))
        return ERRO_MEMORIA;

    ag->qtd++;
    if (ocupada)
        ag->colisoes++;
    return OK;
}

Contato *buscar(Agenda *ag, const char *telefone) {
    char chave[TEL_MAX + 1];
    if (!normalizar_telefone(telefone, chave))
        return NULL;

    No *no = lc_buscar(ag->lista[indice(ag, chave)], chave);
    return no ? &no->valor : NULL;
}

int atualizar(Agenda *ag, const char *telefone, const Contato *valor) {
    char chave[TEL_MAX + 1];
    if (!normalizar_telefone(telefone, chave))
        return ERRO_TELEFONE_INVALIDO;

    No *no = lc_buscar(ag->lista[indice(ag, chave)], chave);
    if (no == NULL)
        return ERRO_NAO_ENCONTRADA;

    no->valor = *valor;
    return OK;
}

int remover(Agenda *ag, const char *telefone) {
    char chave[TEL_MAX + 1];
    if (!normalizar_telefone(telefone, chave))
        return ERRO_TELEFONE_INVALIDO;

    if (!lc_remover(&ag->lista[indice(ag, chave)], chave))
        return ERRO_NAO_ENCONTRADA;

    ag->qtd--;
    return OK;
}

static void imprimir_contato(const char *chave, const Contato *c) {
    char tel[20];
    formatar_telefone(chave, tel);
    printf("%s | %s | %s | %s\n", tel, c->nome, c->email, c->obs);
}

/* Lista todos os contatos (ordem das posicoes da tabela, nao alfabetica) */
void listar(const Agenda *ag) {
    printf("--- Contatos (%d) ---\n", ag->qtd);
    if (ag->qtd == 0) {
        printf("(agenda vazia)\n");
        return;
    }
    for (int i = 0; i < ag->capacidade; i++) {
        ListaCircular fim = ag->lista[i];
        if (fim == NULL)
            continue;
        No *p = fim->prox;
        do {
            imprimir_contato(p->chave, &p->valor);
            p = p->prox;
        } while (p != fim->prox);
    }
}

/* Estado interno da tabela, para depuracao */
void exibirEstrutura(const Agenda *ag) {
    int ocupadas = 0, maior = 0;
    for (int i = 0; i < ag->capacidade; i++) {
        int n = lc_tamanho(ag->lista[i]);
        if (n > 0)
            ocupadas++;
        if (n > maior)
            maior = n;
    }

    printf("=== Estrutura da tabela ===\n");
    printf("capacidade: %d | contatos: %d | fator de carga: %.2f\n",
           ag->capacidade, ag->qtd, (double)ag->qtd / ag->capacidade);
    printf("posicoes ocupadas: %d/%d | maior lista: %d | colisoes: %d\n",
           ocupadas, ag->capacidade, maior, ag->colisoes);

    for (int i = 0; i < ag->capacidade; i++) {
        ListaCircular fim = ag->lista[i];
        printf("[%2d] (n=%d) ", i, lc_tamanho(fim));
        if (fim == NULL) {
            printf("vazia\n");
            continue;
        }
        No *p = fim->prox;
        do {
            printf("%s -> ", p->chave);
            p = p->prox;
        } while (p != fim->prox);
        printf("(volta ao inicio)\n");
    }
}

/* ------------------------------------------------------------------ */
/*  Testes de corretude                                               */
/* ------------------------------------------------------------------ */

static int total_testes = 0, falhas = 0;

#define CHECK(cond, msg) do { \
    total_testes++; \
    if (cond) printf("  [ok] %s\n", msg); \
    else { falhas++; printf("  [FALHOU] %s\n", msg); } \
} while (0)

/* Gera "quantidade" telefones validos e distintos que caem na MESMA posicao */
static int gerar_colisoes(const Agenda *ag, int quantidade, char out[][TEL_MAX + 1]) {
    unsigned int alvo = 0;
    int achados = 0;

    for (int k = 0; k < 100000 && achados < quantidade; k++) {
        char tel[32];
        snprintf(tel, sizeof tel, "849%08d", k);    /* 11 digitos */
        if (achados == 0)
            alvo = indice(ag, tel);
        if (indice(ag, tel) == alvo)
            copiar(out[achados++], TEL_MAX + 1, tel);
    }
    return achados;
}

static int acessivel(Agenda *ag, const char *tel, const char *nome) {
    Contato *c = buscar(ag, tel);
    return c != NULL && strcmp(c->nome, nome) == 0;
}

int testes(void) {
    char k1[16], k2[16], k3[16];

    printf("== Teste 1: normalizacao do telefone ==\n");
    CHECK(normalizar_telefone("(84) 99999-1234", k1) && strcmp(k1, "84999991234") == 0,
          "(84) 99999-1234 -> 84999991234");
    CHECK(normalizar_telefone("+55 84 99999-1234", k2) && strcmp(k1, k2) == 0,
          "+55 84 99999-1234 gera a mesma chave");
    CHECK(normalizar_telefone("84 9999-1234", k3) && strcmp(k3, "8499991234") == 0,
          "telefone fixo de 10 digitos");
    CHECK(!normalizar_telefone("12345", k3), "poucos digitos e rejeitado");
    CHECK(!normalizar_telefone("84-9999a-1234", k3), "letra no telefone e rejeitada");
    CHECK(!normalizar_telefone("", k3), "string vazia e rejeitada");

    printf("\n== Teste 2: inserir, buscar, atualizar, remover ==\n");
    Agenda *ag = agenda_criar(CAPACIDADE_INICIAL);
    Contato ana  = { "Ana Souza", "ana@email.com", "Colega de turma" };
    Contato ana2 = { "Ana S. Lima", "ana.lima@email.com", "Mudou de sobrenome" };

    CHECK(inserir(ag, "(84) 99999-1234", &ana) == OK, "inserir contato novo");
    CHECK(inserir(ag, "84999991234", &ana) == ERRO_DUPLICADA,
          "duplicata rejeitada (mesma chave normalizada)");
    CHECK(inserir(ag, "123", &ana) == ERRO_TELEFONE_INVALIDO, "telefone invalido rejeitado");
    CHECK(ag->qtd == 1, "apenas 1 contato armazenado");
    CHECK(acessivel(ag, "+55 84 99999-1234", "Ana Souza"), "buscar com outro formato do mesmo numero");
    CHECK(buscar(ag, "84988887777") == NULL, "busca sem resultado devolve NULL");
    CHECK(atualizar(ag, "84999991234", &ana2) == OK && acessivel(ag, "84999991234", "Ana S. Lima"),
          "atualizar altera os dados");
    CHECK(atualizar(ag, "84988887777", &ana2) == ERRO_NAO_ENCONTRADA, "atualizar inexistente");
    CHECK(remover(ag, "84988887777") == ERRO_NAO_ENCONTRADA, "remover inexistente");
    CHECK(remover(ag, "84999991234") == OK && buscar(ag, "84999991234") == NULL && ag->qtd == 0,
          "remover contato existente");
    agenda_destruir(ag);

    printf("\n== Teste 3 (indispensavel): chaves que colidem continuam acessiveis apos remocoes ==\n");
    ag = agenda_criar(CAPACIDADE_INICIAL);
    char tel[4][TEL_MAX + 1];
    CHECK(gerar_colisoes(ag, 4, tel) == 4, "gerados 4 telefones distintos na mesma posicao");

    unsigned int idx = indice(ag, tel[0]);
    int mesma_posicao = 1;
    for (int i = 1; i < 4; i++)
        if (indice(ag, tel[i]) != idx)
            mesma_posicao = 0;
    CHECK(mesma_posicao, "os 4 telefones tem o mesmo indice");

    char nome[4][16];
    for (int i = 0; i < 4; i++) {
        Contato c;
        snprintf(nome[i], sizeof nome[i], "Contato %d", i);
        snprintf(c.nome, sizeof c.nome, "%s", nome[i]);
        snprintf(c.email, sizeof c.email, "c%d@email.com", i);
        snprintf(c.obs, sizeof c.obs, "colide na posicao %u", idx);
        inserir(ag, tel[i], &c);
    }
    CHECK(ag->colisoes == 3, "contador de colisoes = 3");
    CHECK(lc_tamanho(ag->lista[idx]) == 4, "lista circular da posicao tem 4 nos");
    printf("\n");
    exibirEstrutura(ag);
    printf("\n");
    listar(ag);
    printf("\n");

    CHECK(remover(ag, tel[1]) == OK, "remove no do MEIO");
    CHECK(acessivel(ag, tel[0], nome[0]) && acessivel(ag, tel[2], nome[2]) && acessivel(ag, tel[3], nome[3]),
          "os outros 3 continuam acessiveis");
    CHECK(buscar(ag, tel[1]) == NULL, "o removido nao e mais encontrado");

    CHECK(remover(ag, tel[3]) == OK, "remove o ULTIMO no (fim da lista)");
    CHECK(acessivel(ag, tel[0], nome[0]) && acessivel(ag, tel[2], nome[2]),
          "os outros 2 continuam acessiveis");

    CHECK(remover(ag, tel[0]) == OK, "remove o PRIMEIRO no");
    CHECK(acessivel(ag, tel[2], nome[2]), "o que sobrou continua acessivel");

    CHECK(remover(ag, tel[2]) == OK, "remove o UNICO no restante");
    CHECK(ag->lista[idx] == NULL && ag->qtd == 0, "posicao volta a ficar vazia");
    agenda_destruir(ag);

    printf("\n== Teste 4: varios contatos (inserir, buscar, remover metade) ==\n");
    ag = agenda_criar(CAPACIDADE_INICIAL);
    int ok_ins = 1, ok_busca = 1, ok_resto = 1, ok_rem = 1;
    for (int k = 0; k < 40; k++) {
        char t[32];
        Contato c;
        snprintf(t, sizeof t, "8498%07d", k);
        snprintf(c.nome, sizeof c.nome, "Pessoa %d", k);
        snprintf(c.email, sizeof c.email, "p%d@email.com", k);
        c.obs[0] = '\0';
        if (inserir(ag, t, &c) != OK)
            ok_ins = 0;
    }
    CHECK(ok_ins && ag->qtd == 40, "40 contatos inseridos");
    for (int k = 0; k < 40; k++) {
        char t[32], n[32];
        snprintf(t, sizeof t, "8498%07d", k);
        snprintf(n, sizeof n, "Pessoa %d", k);
        if (!acessivel(ag, t, n))
            ok_busca = 0;
    }
    CHECK(ok_busca, "todos os 40 sao encontrados");
    for (int k = 0; k < 40; k += 2) {
        char t[32];
        snprintf(t, sizeof t, "8498%07d", k);
        if (remover(ag, t) != OK)
            ok_rem = 0;
    }
    for (int k = 1; k < 40; k += 2) {
        char t[32], n[32];
        snprintf(t, sizeof t, "8498%07d", k);
        snprintf(n, sizeof n, "Pessoa %d", k);
        if (!acessivel(ag, t, n))
            ok_resto = 0;
    }
    CHECK(ok_rem && ag->qtd == 20, "20 contatos removidos, 20 restantes");
    CHECK(ok_resto, "os 20 restantes continuam acessiveis");
    printf("\n");
    exibirEstrutura(ag);
    agenda_destruir(ag);

    printf("\nResultado: %d/%d verificacoes passaram%s\n",
           total_testes - falhas, total_testes, falhas ? " (HA FALHAS)" : "");
    return falhas ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/*  Menu interativo                                                   */
/* ------------------------------------------------------------------ */

static int ler(const char *prompt, char *buf, size_t n) {
    printf("%s", prompt);
    if (fgets(buf, (int)n, stdin) == NULL) {
        buf[0] = '\0';
        return 0;
    }
    buf[strcspn(buf, "\n")] = '\0';
    return 1;
}

static void ler_contato(Contato *c) {
    ler("Nome: ", c->nome, sizeof c->nome);
    ler("E-mail: ", c->email, sizeof c->email);
    ler("Observacoes: ", c->obs, sizeof c->obs);
}

static const char *mensagem(int cod) {
    switch (cod) {
        case OK:                  return "Operacao realizada.";
        case ERRO_TELEFONE_INVALIDO: return "Telefone invalido (use DDD + 8 ou 9 digitos).";
        case ERRO_DUPLICADA:      return "Ja existe contato com esse telefone (use atualizar).";
        case ERRO_NAO_ENCONTRADA: return "Contato nao encontrado.";
        default:                  return "Erro de memoria.";
    }
}

static void menu(void) {
    Agenda *ag = agenda_criar(CAPACIDADE_INICIAL);
    if (ag == NULL) {
        printf("Erro de memoria!\n");
        return;
    }

    char op[16], tel[64];
    Contato c;

    for (;;) {
        printf("\n=== AGENDA DE CONTATOS ===\n"
               "1 - Inserir\n2 - Consultar\n3 - Atualizar\n4 - Remover\n"
               "5 - Listar\n6 - Exibir estrutura da tabela\n0 - Sair\n");
        if (!ler("Opcao: ", op, sizeof op))
            break;

        int opcao = atoi(op);
        if (opcao == 0)
            break;

        switch (opcao) {
            case 1:
                ler("Telefone: ", tel, sizeof tel);
                ler_contato(&c);
                printf("%s\n", mensagem(inserir(ag, tel, &c)));
                break;
            case 2: {
                ler("Telefone: ", tel, sizeof tel);
                Contato *r = buscar(ag, tel);
                if (r == NULL) {
                    printf("Contato nao encontrado.\n");
                } else {
                    char chave[TEL_MAX + 1];
                    normalizar_telefone(tel, chave);
                    imprimir_contato(chave, r);
                }
                break;
            }
            case 3:
                ler("Telefone: ", tel, sizeof tel);
                ler_contato(&c);
                printf("%s\n", mensagem(atualizar(ag, tel, &c)));
                break;
            case 4:
                ler("Telefone: ", tel, sizeof tel);
                printf("%s\n", mensagem(remover(ag, tel)));
                break;
            case 5:
                listar(ag);
                break;
            case 6:
                exibirEstrutura(ag);
                break;
            default:
                printf("Opcao invalida.\n");
        }
    }

    agenda_destruir(ag);
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "teste") == 0)
        return testes();

    menu();
    return 0;
}
