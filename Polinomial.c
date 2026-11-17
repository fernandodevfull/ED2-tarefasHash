#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TAM  7             
#define BASE 257ULL         
#define MOD  1000000007ULL   

/*
 * Hash polinomial:
 *
 *
 * Cada símbolo/dígito recebe um peso B^k que depende da POSIÇÃO dele,
 * então "abc" e "cba" geram hashes diferentes (uma soma simples de
 * caracteres daria o mesmo valor).
 *
 * Em vez de calcular as potências, usa-se o método de Horner:
 *   h = h * B + s[i]     para cada caractere, da esquerda para a direita.
 */
uint64_t hash_polinomial(const char *s) {
    uint64_t h = 0;
    for (; *s != '\0'; s++)
        h = (h * BASE + (unsigned char)*s) % MOD;
    return h;
}

/* Índice final na tabela */
unsigned int indice(const char *s) {
    return (unsigned int)(hash_polinomial(s) % TAM);
}

/* Versão "explícita", com as potências, só para conferir o Horner */
uint64_t hash_explicito(const char *s) {
    size_t n = strlen(s);
    uint64_t h = 0;
    for (size_t i = 0; i < n; i++) {
        uint64_t peso = 1;                       /* peso = BASE^(n-1-i) mod MOD */
        for (size_t k = 0; k < n - 1 - i; k++)
            peso = (peso * BASE) % MOD;
        h = (h + (uint64_t)(unsigned char)s[i] * peso) % MOD;
    }
    return h;
}

int main(void) {
    const char *chaves[] = {
        "abc", "cba", "bac",      /* mesmas letras, ordem diferente */
        "123", "321", "A1", "1A", /* dígitos e símbolos misturados */
        "user@01", "user@10", "#9", "9#"
    };
    int n = sizeof(chaves) / sizeof(chaves[0]);

    printf("%-10s %-12s %-8s %s\n", "chave", "hash", "indice", "confere");
    for (int i = 0; i < n; i++) {
        uint64_t h  = hash_polinomial(chaves[i]);
        uint64_t he = hash_explicito(chaves[i]);
        printf("%-10s %-12llu %-8u %s\n",
               chaves[i], (unsigned long long)h, indice(chaves[i]),
               h == he ? "ok" : "DIFERENTE");
    }
    return 0;
}