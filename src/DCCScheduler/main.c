/*
 * DCCScheduler.c
 * Código base para la implementación del scheduler MLFQ adaptado a file_manager.
 *
 * Compilar:
 *     gcc -Wall -Wextra -std=c99 -o DCCScheduler DCCScheduler.c
 *
 * Uso:
 *     ./DCCScheduler <input_file> <output_file> <q> <n>
 *
 * Donde:
 *   - input_file: Archivo con la descripción de los procesos.
 *   - output_filmme: Archivo CSV en donde se imprimen las estadísticas.
 *   - q: Parámetro para calcular el quantum (cola High: 2*q, Medium: q).
 *   - n: Cantidad de ticks para el aging (cambio de prioridad).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../file_manager/manager.h"

/* Definición de estados del proceso */
typedef enum
{
    RUNNING,
    READY,
    WAITING,
    FINISHED
} ProcessState;

typedef enum
{
    LOW,
    MEDIUM,
    HIGH
} QueueType;

QueueType DecrementQueueType(QueueType q)
{
    if (q > LOW)
    {
        return q - 1;
    }
    return q; // Stay at LOW
}

/* Estructura que modela un proceso */
typedef struct Process
{
    char name[64];
    unsigned int pid;
    ProcessState state;
    unsigned int t_inicio; // Tick en el que el proceso ingresa al sistema
    unsigned int burst;    // Duración de la ráfaga de CPU
    int n_bursts; // Número total de ráfagas
    unsigned int io_wait;  // Tiempo de espera para I/O entre ráfagas
    unsigned int priority; // Prioridad original (1 a 30)

    /* Variables para la simulación */
    unsigned int remaining_burst; // Tiempo restante de la ráfaga actual
    unsigned int quantum;         // Quantum asignado (según la cola)
    unsigned int turnaround;      // Tiempo total de ejecución (fin - inicio)
    unsigned int response;        // Tiempo hasta la primera asignación a la CPU
    unsigned int waiting_time;    // Suma de intervalos en READY o WAITING
    unsigned int num_cambios;     // Número de veces que cambia de cola
    unsigned int io_finish;       // Tick en el que termina la operación de I/O
    unsigned int total_runtime;   // para calcular waiting time total
    int started;                  // Flag para indicar si ya se ejecutó (para calcular response)
    struct Process *next;
    QueueType queue_type;
} Process;

/* Estructura para las colas de procesos */
typedef struct
{
    Process *head;
    Process *tail;
    int type; // 0 Low, 1 Medium, 2 High
} Queue;

/* Funciones para manejo de cola */
Queue *createQueue();
void enqueue(Queue *queue, Process *p, bool reset);
Process *dequeue(Queue *queue);
void freeQueue(Queue *queue);

/* Funciones para creación de procesos a partir de tokens (ya leídos por manager) */
Process createProcessFromTokens(char **tokens);

/* Funciones para la simulación del scheduler */
void simulateScheduler(Process *processes, int num_processes, int q, int n, const char *output_filename);
void aging(Queue *queue);
void printStatistics(Process *processes, int num_processes, const char *output_filename);

int q;
int num_processes;

Queue *highQueue;
Queue *mediumQueue;
Queue *lowQueue;

Process *running_process;

int main(int argc, char const *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Uso: %s <input_file> <output_file> <q> <n>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Se leen los argumentos de entrada */
    char *input_filename = (char *)argv[1];
    char *output_filename = (char *)argv[2];
    q = atoi(argv[3]);
    int n = atoi(argv[4]);

    /* Lectura del input mediante file_manager */
    InputFile *input_file = read_file(input_filename);

    num_processes = input_file->len;
    printf("Cantidad de procesos: %d\n", num_processes);
    printf("Procesos:\n");

    /* Se crea un arreglo de procesos basado en las líneas leídas.
       Se asume que cada línea tiene 7 tokens:
       NOMBRE_PROCESO, PID, T_INICIO, T_CPU_BURST, N_BURSTS, IO_WAIT, PRIORITY
    */
    Process *processes = (Process *)malloc(input_file->len * sizeof(Process));
    if (!processes)
    {
        perror("Error al asignar memoria para procesos");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < input_file->len; ++i)
    {
        // Imprimir en consola (opcional, para verificar)
        printf("  NOMBRE_PROCESO: %s\n", input_file->lines[i][0]);
        printf("  PID: %s\n", input_file->lines[i][1]);
        printf("  T_INICIO: %s\n", input_file->lines[i][2]);
        printf("  T_CPU_BURST: %s\n", input_file->lines[i][3]);
        printf("  N_BURSTS: %s\n", input_file->lines[i][4]);
        printf("  IO_WAIT: %s\n", input_file->lines[i][5]);
        printf("  PRIORITY: %s\n", input_file->lines[i][6]);
        printf("\n");

        processes[i] = createProcessFromTokens(input_file->lines[i]);
    }
    sleep(3);

    /* Inicia la simulación del scheduler */
    simulateScheduler(processes, input_file->len, q, n, output_filename);

    input_file_destroy(input_file);
    free(processes);
    return EXIT_SUCCESS;
}

/* Implementación de funciones para cola */
Queue *createQueue(int type)
{
    Queue *queue = (Queue *)calloc(1, sizeof(Queue));
    queue->type = type;
    queue->head = NULL;
    queue->tail = NULL;
    if (!queue)
    {
        perror("Error al asignar memoria para la cola");
        exit(EXIT_FAILURE);
    }
    return queue;
}

void enqueue(Queue *queue, Process *p, bool reset)
{
    // no duplicados
    Process *current = queue->head;
    while (current != NULL)
    {
        if (current->pid == p->pid) {
            return;
        }
        current = current->next;
    }
    p->next = NULL;
    
    if (reset) {
        p->num_cambios++;
    }

    if ((p->quantum == 0 && p->queue_type == queue->type) | (p->queue_type != queue->type) | (reset == true)) {
    // reajustar quantums
        if (queue->type == HIGH) {
            p->quantum = 2*q;
            p->queue_type = HIGH;
        }
        else if (queue->type == MEDIUM) {
            p->quantum = q;
            p->queue_type = MEDIUM;
        }
        else {
            p->quantum = -1;    // LOW NO TIENE QUANTUM
            p->queue_type = LOW;
        }
        // printf("%s insertado en %d\n", p->name, queue->type);
        // printf("%s: quantum reajustado a %d\n", p->name, p->quantum);
    }

    if (queue->head == NULL)
    {
        queue->head = p;
        return;
    }

    if (queue->type != 0)
    {

        Process *current = queue->head;
        Process *prev = NULL;

        while (current != NULL)
        {
            if (p->priority < current->priority)
            {
                if (prev != NULL)
                {
                    prev->next = p;
                }
                else
                {
                    queue->head = p;
                }
                p->next = current;
                break;
            }
            else if (current->priority == p->priority && current->pid < p->pid)
            {
                if (prev != NULL)
                {
                    prev->next = p;
                }
                else
                {
                    queue->head = p;
                }
                p->next = current;
                break;
            }
            if (current->next == NULL)
            {
                current->next = p;
                break;
            }

            prev = current;
            current = current->next;
        }
    }
    else
    {
        Process *current = queue->head;
        Process *prev = NULL;

        while (current != NULL)
        {
            prev = current;
            current = current->next;
        }
        prev->next = p;
    }
    current = queue->head;
    printf("COLA %d:\n", queue->type);
    while (current != NULL)
    {
        printf("%s\n", current->name);
        current = current->next;
    }
    printf("\n");
}

Process *dequeue(Queue *queue)
{
    if (queue->head == NULL)
    {
        return NULL;
    }
    else
    {
        Process *temp = queue->head;
        queue->head = temp->next;
        // printf("head is: %s ; next is: %s\n", temp->name, temp->next->name);
        return temp;
    }
}

void removeProcessFromQueue(int pid, Queue *queue)
{
    char* name[16];
    if (queue->head != NULL){
        if (queue->head->pid == pid)
        {
            *name = queue->head->name;
            // printf("%s removido de %d\n", name[0], queue->type);
            queue->head = queue->head->next;
            return;
        }
    }

    Process *prev = NULL;
    Process *current = queue->head;
    
    while (current != NULL)
    {
        Process *next = current->next;
        if (current->pid == pid)
        {
            *name = current->name;
            // printf("%s removido de %d\n", name[0], queue->type);
            if (prev != NULL) {
                prev->next = next;
            }
            current->next = NULL;
            break;
            // return;
        }
        current = next;
    }
}

/* Convierte una línea (array de tokens) en un objeto Process */
Process createProcessFromTokens(char **tokens)
{
    Process p;
    strncpy(p.name, tokens[0], sizeof(p.name) - 1);
    p.name[sizeof(p.name) - 1] = '\0';
    p.pid = (unsigned int)atoi(tokens[1]);
    p.t_inicio = (unsigned int)atoi(tokens[2]);
    p.burst = (unsigned int)atoi(tokens[3]);
    p.n_bursts = (unsigned int)atoi(tokens[4]);
    p.io_wait = (unsigned int)atoi(tokens[5]);
    p.priority = (unsigned int)atoi(tokens[6]);

    // Inicialización de variables para la simulación
    p.state = READY;
    p.remaining_burst = p.burst;
    p.quantum = 0; // Se asignará según la cola en la que ingrese
    p.turnaround = 0;
    p.response = 0;
    p.waiting_time = 0;
    p.num_cambios = 0;
    p.started = 0;
    p.total_runtime = 0;
    p.next = NULL;
    p.queue_type = LOW;
    return p;
}

void insert_process_in_queues(Process *p)
{
    // printf("Insert\n");
    // En función de la prioridad, se determina la cola de ingreso:
    if (p->priority >= 1 && p->priority <= 10)
    {
        p->queue_type = HIGH;
        enqueue(highQueue, p, false);
    }
    else if (p->priority >= 11 && p->priority <= 20)
    {
        p->queue_type = MEDIUM;
        enqueue(mediumQueue, p, false);
    }
    else if (p->priority >= 21 && p->priority <= 30)
    {
        p->queue_type = LOW;
        enqueue(lowQueue, p, false);
    }
    else {
        printf("not queued\n");
    }
    // printf("%s insertado en %d\n", p->name, p->queue_type);
}

void reinsertProcess(Process *p)
{
    // printf("Reinsert\n");
    p->next = NULL;
    Queue *queue;
    if (p->queue_type == HIGH)
    {
        queue = highQueue;
    }
    else if (p->queue_type == MEDIUM)
    {
        queue = mediumQueue;
    }
    else
    {
        queue = lowQueue;
    }
    removeProcessFromQueue(p->pid, queue);
    enqueue(queue, p, false);
}

void move_processes(Queue *current_queue, Queue *new_queue)
{
    // printf("Move\n");
    while (current_queue->head != NULL)
    {
        printf("current: %d, new: %d\n", current_queue->type, new_queue->type);
        enqueue(new_queue, dequeue(current_queue), true);
    }
}

/* Esqueleto de la simulación del scheduler */
void simulateScheduler(Process *processes, int num_processes, int q, int n, const char *output_filename)
{
    int current_tick = 0;
    int processes_finished = 0;
    bool process_picked = false;

    /* Creación de las tres colas: High, Medium y Low */
    highQueue = createQueue(2);
    mediumQueue = createQueue(1);
    lowQueue = createQueue(0);

    while (processes_finished < num_processes)
    {
        // sleep(1);
        printf("\nTick: %d\n", current_tick);

        // 1. Actualizar los procesos que hayan terminado su tiempo de espera de I/O de WAITING a READY.
        for (int i = 0; i < num_processes; i++)
        {
            if (processes[i].state == WAITING && processes[i].io_finish <= current_tick)
            {
                processes[i].state = READY;
            }
        }

        // 2. Si hay un proceso en estado RUNNING, actualizar su estado segun corresponda.
        if (running_process)
        {
            if (running_process->remaining_burst == 0 && running_process->state == RUNNING) // Fin rafaga de ejecucion
            {
                running_process->n_bursts--;

                if (running_process->quantum == 0 && running_process->n_bursts > 0) {    // Quantum expirado
                    running_process->queue_type = DecrementQueueType(running_process->queue_type);
                    running_process->num_cambios++;
                    reinsertProcess(running_process);
                }

                if (running_process->n_bursts <= 0) // Fin ejecucion
                {
                    printf("Termino proceso %s\n", running_process->name);
                    running_process->state = FINISHED;
                    running_process->turnaround = current_tick - running_process->t_inicio;
                    running_process->waiting_time = running_process->turnaround - running_process->total_runtime;
                    processes_finished++;
                }
                else
                {
                    running_process->state = WAITING;
                    running_process->remaining_burst = running_process->burst;
                    running_process->io_finish = current_tick + running_process->io_wait;
                    reinsertProcess(running_process); // 3.1. Si un proceso salio de la CPU, ingresarlo a la cola que corresponda.
                }
            }
            else if (running_process->quantum == 0) // Quantum expirado
            {
                running_process->num_cambios++;
                running_process->state = READY;
                running_process->queue_type = DecrementQueueType(running_process->queue_type);
                reinsertProcess(running_process); // 3.1. Si un proceso salio de la CPU, ingresarlo a la cola que corresponda.
            }
            else // Caso normal
            {
                if (running_process->started == 0) {
                    running_process->started = 1;
                    running_process->response = current_tick - running_process->t_inicio;
                }
                running_process->remaining_burst--;
                running_process->quantum--;
                running_process->total_runtime++;
                printf("%s: n_bursts: %d ; remaining_burst: %d; quantum: %d; queue: %d, cambios: %d\n", running_process->name, running_process->n_bursts, 
                                                                            running_process->remaining_burst, running_process->quantum, running_process->queue_type, running_process->num_cambios);
            }
        }

        // 3. Ingresar los procesos a las colas segun corresponda (3.1 mas arriba)

        // 3.2 Si el tiempo de inicio de un proceso se cumple, ingresarlo a la cola que corresponda.
        for (int i = 0; i < num_processes; i++)
        {
            if (processes[i].t_inicio == current_tick && processes[i].state == READY)
            {
                printf("iniciar proceso: %s\n", processes[i].name);
                insert_process_in_queues(&processes[i]);
            }
        }

        // 3.3 Si han pasado n ticks, subir la prioridad de todos los procesos, ingresandolos a la cola siguiente correspondiente.
        if (current_tick > 0 && (current_tick % n) == 0 && !process_picked)
        {
            printf("moviendo queues\n");
            move_processes(mediumQueue, highQueue);
            move_processes(lowQueue, mediumQueue);
        }

        // 4. Si no hay un proceso en estado RUNNING, ingresar el proceso de mayor prioridad en estado READY a la CPU
        if (running_process) {
            if (running_process->state != RUNNING) {
                printf("desactivando: %s\n", running_process->name);
                running_process = NULL;
            }
        }
        process_picked = false;
        if (!running_process)
        {
            // printf("not running\n");
            for (int i = 0; i < 3; i++)
            {
                Queue *queue = NULL;
                int quantum;

                // determina a qué cola accedemos
                if (i == 0)
                {
                    queue = highQueue;
                    quantum = 2 * q;
                }
                else if (i == 1)
                {
                    queue = mediumQueue;
                    quantum = q;
                }
                else if (i == 2)
                {
                    queue = lowQueue;
                    quantum = -1;
                }

                // saltar colas vacías
                if (queue == NULL || queue->head == NULL)
                    continue;

                Process *current = queue->head;

                while (current != NULL)
                {
                    if (current->state == FINISHED) {
                        removeProcessFromQueue(current->pid, queue);
                    }
                    else if (current->state == READY)
                    {
                        running_process = current;
                        removeProcessFromQueue(current->pid, queue);
                        running_process->state = RUNNING;
                        printf("process: %s\n", current->name);
                        if (running_process->started == 0){
                            running_process->quantum = quantum;
                            running_process->remaining_burst = running_process->burst;
                        }
                        current_tick--;
                        process_picked = true;
                        break;
                    }

                    current = current->next;
                }
                if (process_picked){
                    break;
                }
            }
        }

        current_tick++; // Incrementar el contador de ticks
    }

    /* Al finalizar la simulación se imprime el archivo de salida CSV con las estadísticas */
    printStatistics(processes, num_processes, output_filename);

    /* Liberar recursos de las colas */
    free(highQueue);
    free(mediumQueue);
    free(lowQueue);
}

/* Imprime las estadísticas de cada proceso en formato CSV, ordenado por el tiempo de término.
   Formato: nombre_proceso,pid,turnaround,response,waiting,n_cambios_de_cola */
void printStatistics(Process *processes, int num_processes, const char *output_filename)
{
    FILE *fp = fopen(output_filename, "w");
    if (!fp)
    {
        perror("Error al abrir el archivo de salida");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_processes; i++)
    {
        fprintf(fp, "%s,%u,%u,%u,%u,%u\n",
                processes[i].name,
                processes[i].pid,
                processes[i].turnaround,
                processes[i].response,
                processes[i].waiting_time,
                processes[i].num_cambios);
    }
    fclose(fp);
}
