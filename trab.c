/****************************************
 *      Nome: Ian Nery Bandeira         *
 *       Matricula: 17/0144739          *
 *                                      *
 *   Preferencialmente, executar em um  *
 *       terminal de cor preta.         *
 ****************************************/

// bibliotecas necessarias para rodar o programa
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

/**
 * Macros para cores do terminal, para ficar mais
 * facil de identificar as "entidades" presentes.
 */
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define RESET "\x1B[0m"


/**
 * Macros para cores do terminal, para ficar mais
 * facil de identificar as "entidades" presentes.
 */

#define NUMCLIENTE_N 4 // numero de threads de clientes comuns
#define NUMCLIENTE_P 5 // numero de threads de clientes preferenciais
#define NUMFUNC 4 // numero de threads de funcionarios
#define CAPACIDADE_ITENS 40 // capacidade de itens da prateleira
#define CAPACIDADE_CLIENTES 6 // capacidade de clientes presentes no mercadinhoinho
#define CAIXAS 3 // numero de caixas 

pthread_t cliente_n[NUMCLIENTE_N];
pthread_t cliente_p[NUMCLIENTE_P];
pthread_t funcionario[NUMFUNC];

int prateleira[CAPACIDADE_ITENS];
int item_pos = 0; 
int cliente_pos = 0;
int caixas_ocupados = 0;
int preferencial = 0; // clientees preferenciais na fila de caixa
int comum = 0; // clientes comuns na fila de caixa


// buffer para exclusao mutua entre ambas as threads de clientes e funcionarios 
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
// buffer para exclusao mutua entre as entidades dentro da fila do caixa
pthread_mutex_t buffer_mutex_caixa = PTHREAD_MUTEX_INITIALIZER;
// variaveis de condicao para criar a prioridade entre as entidades de clientes
pthread_cond_t fila_normal_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t fila_pref_cond = PTHREAD_COND_INITIALIZER;

// semaforos para as posicoes dos produtos no mercadinhoinho
sem_t posicoes_livres;
sem_t posicoes_ocupadas;
// semaforo para determinar a lotacao do mercadinhoinho
sem_t lotacao;

void *repor_prateleira(void *arg);
void *pegar_e_pagar_normal(void *arg);
void *pegar_e_pagar_preferencial(void *arg);

int main(int argc, char **argv)
{
    int i;
    srand48(time(NULL)); // inicializador randomico para numero de produto
    // inicializador dos semaforos declarados globalmente
    sem_init(&lotacao, 0, CAPACIDADE_CLIENTES);
    sem_init(&posicoes_livres, 0, CAPACIDADE_ITENS);
    sem_init(&posicoes_ocupadas, 0, 0);

    int *id;
    // CRIACAO DE THREADS
    for (i = 0; i < NUMCLIENTE_N; i++)
    {
        id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&(cliente_n[i]), NULL, pegar_e_pagar_normal, (void *)id);
    }

    for (i = 0; i < NUMCLIENTE_P; i++)
    {
        id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&(cliente_p[i]), NULL, pegar_e_pagar_preferencial, (void *)id);
    }

    for (i = 0; i < NUMFUNC; i++)
    {
        id = (int *)malloc(sizeof(int));
        *id = i;
        pthread_create(&(funcionario[i]), NULL, repor_prateleira, (void *)id);
    }
    // JOINS
    for (i = 0; i < NUMCLIENTE_N; i++)
    {
        pthread_join(cliente_n[i], NULL);
    }

    for (i = 0; i < NUMCLIENTE_P; i++)
    {
        pthread_join(cliente_p[i], NULL);
    }
    for (i = 0; i < NUMFUNC; i++)
    {
        pthread_join(funcionario[i], NULL);
    }
}
/**
 * Funcao que faz funcionarios reporem itens na prateleira,
 * quando existir espaco nela.
 */
void *repor_prateleira(void *arg)
{
    int id = *((int *)arg);
    int item;
    while (1)
    {
        item = (int)(drand48() * 1000.0); // "cria" o item
        sleep(2);

        sem_wait(&posicoes_livres); // espera ter posicoes livres na prateleira
        pthread_mutex_lock(&buffer_mutex); // exclusao mutua com outros funcionarios e outros clientes
            prateleira[item_pos] = item; // coloca item na prateleira
            item_pos = (item_pos + 1) % CAPACIDADE_ITENS;
            printf("Funcionario %d: Estou repondo o item %d na posicao %d da prateleira\n", id, item, item_pos);
        pthread_mutex_unlock(&buffer_mutex); // remove a exclusao mutua para outros funcionarios e clientes terem acesso a prateleira
        sem_post(&posicoes_ocupadas); // atualiza o semaforo de posicoes ocupadas na prateleira
    }
    pthread_exit(0);
}
/**
 * Funcao que faz clientes preferenciais pegarem um item na prateleira,
 * para em seguida levarem para a fila do caixa para o pagamento.
 * 
 * Essa funcao possui preferencia na fila do caixa, ja que sao clientes preferenciais.
 */
void *pegar_e_pagar_preferencial(void *arg)
{
    int id = *((int *)arg);
    int item;
    // variavel que recebe quantos clientes existem no mercado retirado pela funcao sem_getvalue. 
    int qtd_clientes; 
    while (1)
    {
        if(sem_trywait(&lotacao) == 0)
        {
            sem_getvalue(&lotacao, &qtd_clientes);
            printf(GRN"\t\tCliente preferencial %d: Entrei! Tem %d pessoas no mercadinhoinho.\n"RESET, id, CAPACIDADE_CLIENTES - qtd_clientes);
            sem_wait(&posicoes_ocupadas); // espera existirem itens na prateleira
            pthread_mutex_lock(&buffer_mutex); // introduz exclusao mutua na prateleira, para somente um cliente poder retirar um item por vez
                item = prateleira[cliente_pos];
                cliente_pos = (cliente_pos + 1) % CAPACIDADE_ITENS;
                printf(GRN"\t\tCliente preferencial %d, Pegando o item %d na posicao %d da prateleira\n"RESET, id, item, cliente_pos);
            pthread_mutex_unlock(&buffer_mutex); // remove a exclusao mutua para outros funcionarios e clientes terem acesso a prateleira
            sem_post(&posicoes_livres); // atualiza o semaforo para funcionarios poderem preencher o espaco vazio que ficou na prateleira
            pthread_mutex_lock(&buffer_mutex_caixa); // introduz exclusao mutua sob os caixas de pagamento.
                printf(GRN"\t\tCliente preferencial %d: Vou pagar meu produto! \n"RESET,id);
                preferencial++; // coloca um cliente preferencial na "fila"
                while (caixas_ocupados == CAIXAS) // se todos os caixas estiverem ocupados, fica esperando na fila
                {
                    printf(GRN"\t\tCliente preferencial %d: Os caixas estão cheios, terei que esperar.\n"RESET,id);
                    printf(RED"\t\t\tPessoas nos caixas = %d, Preferenciais na fila = %d, Comuns na fila = %d\n"RESET,caixas_ocupados, preferencial, comum);
                    pthread_cond_wait(&fila_pref_cond, &buffer_mutex_caixa);
                }
                preferencial--; // sai da fila se tiverem caixas desocupados
                caixas_ocupados++; // ocupa um caixa para pagar o produto
                printf(GRN"\t\tCliente preferencial %d: Estou pagando meu produto\n"RESET, id);
                printf(RED"\t\t\tPessoas nos caixas = %d, Preferenciais na fila = %d, Comuns na fila = %d\n"RESET,caixas_ocupados, preferencial, comum);
            pthread_mutex_unlock(&buffer_mutex_caixa);
            sleep(5); // tempo para pagar o item no caixa
            pthread_mutex_lock(&buffer_mutex_caixa); // coloca novamente exclusao mutua para retirar um cliente do caixa
                caixas_ocupados--; // libera um caixa para uso
                printf(GRN"\t\tCliente preferencial %d: Paguei e estou saindo do mercadinhoinho.\n"RESET, id);
                pthread_cond_signal(&fila_pref_cond); // da sinal para a fila preferencial
                pthread_cond_signal(&fila_normal_cond); // da sinal para a fila comum
                sem_post(&lotacao); // libera um lugar no mercadinhoinho
            pthread_mutex_unlock(&buffer_mutex_caixa);
            sleep(10); // tempo para o cliente tentar entrar novamente no mercado
        }
    }
    pthread_exit(0);
}

/**
 * Funcao que faz clientes preferenciais pegarem um item na prateleira,
 * para em seguida levarem para a fila do caixa para o pagamento.
 * 
 * A funcao funciona de maneira analoga a pagar_e_pegar_preferencial, porem
 * essa funcao nao tem preferencia na fila do caixa, ja que sao clientes comuns.
 */

void *pegar_e_pagar_normal(void *arg)
{
    int id = *((int *)arg);
    int item;
    int qtd_clientes;
    while (1)
    {
        if(sem_trywait(&lotacao) == 0)
        {
            sem_getvalue(&lotacao, &qtd_clientes);
            printf(YEL"\t\tCliente normal %d: Entrei! Tem %d pessoas no mercadinhoinho.\n"RESET, id, CAPACIDADE_CLIENTES - qtd_clientes);
            sem_wait(&posicoes_ocupadas);
            pthread_mutex_lock(&buffer_mutex);
                item = prateleira[cliente_pos];
                cliente_pos = (cliente_pos + 1) % CAPACIDADE_ITENS;
                printf(YEL"\t\tCliente normal %d: Pegando o item %d na posicao %d da prateleira\n"RESET, id, item, cliente_pos);
            pthread_mutex_unlock(&buffer_mutex);
            sem_post(&posicoes_livres);
            pthread_mutex_lock(&buffer_mutex_caixa);
                comum++;
                printf(YEL"\t\tCliente normal %d: Vou pagar meu produto! \n"RESET,id);
                // se todos os caixas estiverem ocupados ou se existem clientes preferenciais na fila, fica esperando na fila
                while (caixas_ocupados == CAIXAS || preferencial > 0) 
                {
                    printf(YEL"\t\tCliente normal %d: Os caixas estão cheios, terei que esperar.\n"RESET,id);
                    printf(RED"\t\t\tPessoas nos caixas = %d, Preferenciais na fila = %d, Comuns na fila = %d\n"RESET, caixas_ocupados, preferencial, comum);
                    pthread_cond_wait(&fila_normal_cond, &buffer_mutex_caixa);
                }
                comum--;
                caixas_ocupados++;
                printf(YEL"\t\tCliente normal %d: Estou pagando meu produto\n", id);
                printf(RED"\t\t\tPessoas nos caixas = %d, Preferenciais na fila = %d, Comuns na fila = %d\n"RESET, caixas_ocupados, preferencial, comum);
            pthread_mutex_unlock(&buffer_mutex_caixa);
            sleep(5);
            pthread_mutex_lock(&buffer_mutex_caixa);
                caixas_ocupados--;
                printf(YEL"\t\tCliente normal %d: Paguei e estou saindo do mercadinhoinho.\n"RESET, id);
                pthread_cond_signal(&fila_pref_cond);
                pthread_cond_signal(&fila_normal_cond);
                sem_post(&lotacao);
            pthread_mutex_unlock(&buffer_mutex_caixa);
            sleep(10);
        }
    }
    pthread_exit(0);
}
