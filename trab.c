/****************************************
 *      Nome: Ian Nery Bandeira         *
 *       Matricula: 17/0144739          *
 ****************************************/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

#define NUMCLIENTE_N 4
#define NUMCLIENTE_P 5
#define NUMFUNC 4
#define CAPACIDADE_ITENS 40
#define CAPACIDADE_CLIENTES 6
#define CAIXAS 3

pthread_t cliente_n[NUMCLIENTE_N];
pthread_t cliente_p[NUMCLIENTE_P];
pthread_t funcionario[NUMFUNC];

int prateleira[CAPACIDADE_ITENS];
int item_pos = 0;
int cliente_pos = 0;
int caixas_ocupados = 0;
int preferencial = 0;
int comum = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buffer_mutex_caixa = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fila_normal_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t fila_pref_cond = PTHREAD_COND_INITIALIZER;
sem_t posicoes_livres;
sem_t posicoes_ocupadas;
sem_t lotacao;

void *repor_prateleira(void *arg);
void *pegar_e_pagar_normal(void *arg);
void *pegar_e_pagar_preferencial(void *arg);

int main(int argc, char **argv)
{
    int i;
    srand48(time(NULL));
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

void *repor_prateleira(void *arg)
{
    int id = *((int *)arg);
    int item;
    while (1)
    {
        item = (int)(drand48() * 1000.0);
        sleep(2);

        sem_wait(&posicoes_livres);
        pthread_mutex_lock(&buffer_mutex);
            prateleira[item_pos] = item;
            item_pos = (item_pos + 1) % CAPACIDADE_ITENS;
            printf("Funcionario %d: Estou repondo o item %d na posicao %d da prateleira\n", id, item, item_pos);
        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&posicoes_ocupadas);
    }
    pthread_exit(0);
}

void *pegar_e_pagar_preferencial(void *arg)
{
    int id = *((int *)arg);
    int item;
    int qtd_clientes;
    while (1)
    {
        if(sem_trywait(&lotacao) == 0)
        {
            sem_getvalue(&lotacao, &qtd_clientes);
            printf(GRN"\t\tCliente preferencial %d: Entrei! Tem %d pessoas no mercadinhoinho.\n"RESET, id, CAPACIDADE_CLIENTES - qtd_clientes);
            sem_wait(&posicoes_ocupadas);
            pthread_mutex_lock(&buffer_mutex);
                item = prateleira[cliente_pos];
                cliente_pos = (cliente_pos + 1) % CAPACIDADE_ITENS;
                printf(GRN"\t\tCliente preferencial %d, Pegando o item %d na posicao %d da prateleira\n"RESET, id, item, cliente_pos);
            pthread_mutex_unlock(&buffer_mutex);
            sem_post(&posicoes_livres);
            pthread_mutex_lock(&buffer_mutex_caixa);
                printf(GRN"\t\tCliente preferencial %d: Vou pagar meu produto! \n"RESET,id);
                preferencial++;
                while (caixas_ocupados == CAIXAS)
                {
                    printf(GRN"\t\tCliente preferencial %d: Os caixas estão cheios, terei que esperar.\n"RESET,id);
                    printf(RED"\t\t\tPessoas nos caixas = %d, Preferenciais na fila = %d, Comuns na fila = %d\n"RESET,caixas_ocupados, preferencial, comum);
                    pthread_cond_wait(&fila_pref_cond, &buffer_mutex_caixa);
                }
                preferencial--;
                caixas_ocupados++;
                printf(GRN"\t\tCliente preferencial %d: Estou pagando meu produto\n"RESET, id);
                printf(RED"\t\t\tPessoas nos caixas = %d, Preferenciais na fila = %d, Comuns na fila = %d\n"RESET,caixas_ocupados, preferencial, comum);
            pthread_mutex_unlock(&buffer_mutex_caixa);
            sleep(5);
            pthread_mutex_lock(&buffer_mutex_caixa);
                caixas_ocupados--;
                printf(GRN"\t\tCliente preferencial %d: Paguei e estou saindo do mercadinhoinho.\n"RESET, id);
                pthread_cond_signal(&fila_pref_cond);
                pthread_cond_signal(&fila_normal_cond);
                sem_post(&lotacao);
            pthread_mutex_unlock(&buffer_mutex_caixa);
            sleep(10);
        }
    }
    pthread_exit(0);
}

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
