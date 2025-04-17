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
#include <string.h>
#include "../file_manager/manager.h"

/* Definición de estados del proceso */
typedef enum
{
    RUNNING,
    READY,
    WAITING,
    FINISHED
} ProcessState;

/* Estructura que modela un proceso */
typedef struct Process 
{
    char name[64];
    unsigned int pid;
    ProcessState state;
    unsigned int t_inicio; // Tick en el que el proceso ingresa al sistema
    unsigned int burst;    // Duración de la ráfaga de CPU
    unsigned int n_bursts; // Número total de ráfagas
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
    int started;                  // Flag para indicar si ya se ejecutó (para calcular response)
    struct Process *next;
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
void enqueue(Queue *queue, Process *p);
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

    printf("Cantidad de procesos: %d\n", input_file->len);
    num_processes = input_file->len - 1; // Se resta 1 para ignorar la cabecera
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

void enqueue(Queue *queue, Process *p)
{
    printf("entramos a enqueue\n");
    p->quantum = queue->type * q;

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
            if (current->priority > p->priority)
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
            current = prev->next;
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
}

Process *dequeue(Queue *queue)
{
    if (queue->head == NULL)
        return NULL;
    return NULL;
    // Process *current = queue->head;
    // return current;
}

void freeQueue(Queue *queue)
{
    Process *temp;
    if (queue)
    {
        Process *current = queue->head;
        while (current != NULL)
        {
            temp = current->next;
            free(current);
            current = temp;
        }
        free(queue);
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
    p.next = NULL;
    return p;
}

/* Esqueleto de la simulación del scheduler */
void simulateScheduler(Process *processes, int num_processes, int q, int n, const char *output_filename)
{
    int current_tick = 0;
    int processes_finished = 0;

    /* Creación de las tres colas: High, Medium y Low */
    Queue *highQueue = createQueue(2);
    Queue *mediumQueue = createQueue(1);
    Queue *lowQueue = createQueue(0);

    /* La lógica de simulación debe:
         1. Insertar procesos a sus colas correspondientes al cumplir su T_INICIO.
         2. Ejecutar el proceso en CPU según las prioridades (High > Medium > Low).
         3. Actualizar estados (quantum, burst, I/O, aging, etc.) cada tick.
         4. Contabilizar estadísticas (turnaround, response, waiting_time, cambios de cola).
       Aquí se presenta un bucle de simulación simplificado que debes completar.
    */
    while (processes_finished < num_processes)
    {

        // 1. Insertar procesos al sistema en el tick actual
        for (int i = 0; i < num_processes; i++)
        {
            if (processes[i].state == READY)
            {
                // En función de la prioridad, se determina la cola de ingreso:
                if (processes[i].priority >= 1 && processes[i].priority <= 10)
                    enqueue(highQueue, &processes[i]);
                else if (processes[i].priority >= 11 && processes[i].priority <= 20)
                    enqueue(mediumQueue, &processes[i]);
                else if (processes[i].priority >= 21 && processes[i].priority <= 30)
                    enqueue(lowQueue, &processes[i]);
            }
        }

        for (int i = 0; i < 3; i++) {
            Queue* queue = NULL;

            // determina a qué cola accedemos
            if (i == 0) {
                queue = highQueue;
            } else if (i == 1) {
                queue = mediumQueue;
            } else if (i == 2) {
                queue = lowQueue;
            }

            // saltar colas vacías
            if (queue == NULL || queue->head == NULL) {
                printf("continuar\n");
                continue;
            }
            
            Process* current = queue->head;
            printf("numero de for: %d\n", i);
            printf("current: %s\n", current->name);
            printf("next: %s\n", current->next->name);
            printf("current_tick: %d\n", current_tick);

            while (current != NULL) {
                // sacar de WAITING a procesos que terminaron su I/O
                if (current->state == WAITING && current->io_finish == current_tick) {
                    current->state = READY;
                    current->waiting_time += current_tick - current->io_finish;
                }
                // caso de que el proceso no haya empezado
                if (current->started == 0 && current->t_inicio >= current_tick) {
                    current->started = 1;
                    current->state = RUNNING;
                    current->response = current_tick - current->t_inicio;
                }
                // verificar que quedan bursts y que no está en I/O
                if (current->n_bursts > 0 && ((current->state == READY) | (current->state == RUNNING))) {
                    
                    // se consume el quantum completo y cede el CPU
                    if (current->remaining_burst > current->quantum) {
                        current->remaining_burst-= current->quantum;
                        current_tick += current->quantum;
                        current->state = READY;
                    }

                    // se consume el quantum parcial y cede el CPU
                    else if (current->remaining_burst > 0 && current->remaining_burst <= current->quantum) {
                        current_tick += current->remaining_burst;
                        current->remaining_burst = 0;
                        current->n_bursts--;
                        current->remaining_burst = current->burst;

                        if (current->n_bursts > 0) {
                            // si quedan bursts, pasamos a WAITING
                            current->state = WAITING;
                            current->io_finish = current_tick + current->io_wait;
                        }
                        else {
                            // si no quedan bursts, pasamos a FINISHED
                            current->state = FINISHED;
                            current->turnaround = current_tick - current->t_inicio;
                            processes_finished++;
                    }
                }
                else {
                }
                current = current->next;
            }
            }
        }

        // 2. Actualización de procesos en la CPU (quantum, burst, etc.)
        //     Aquí se debe implementar la lógica para:
        //      - Decrementar burst y quantum del proceso RUNNING.
        //      - Cambiar estados a WAITING o FINISHED cuando corresponda.
        //      - Mover procesos a otra cola si se consumió el quantum.
        // (Implementación pendiente)

        // 3. Aplicar aging cada n ticks (si current_tick > 0)
        if (current_tick > 0 && (current_tick % n) == 0)
        {
            aging(highQueue);
            aging(mediumQueue);
            aging(lowQueue);
        }

        // 4. Seleccionar el siguiente proceso para ejecutar en la CPU
        //    (la lógica debe elegir de highQueue, si está vacía entonces mediumQueue, etc.)
        //    (Implementación pendiente)

        // 5. Actualizar estadísticas y verificar si algún proceso termina en este tick.
        //    Incrementar processes_finished cuando un proceso pase a FINISHED.
        //    (Implementación pendiente)

        current_tick++; // Incrementar el contador de ticks
    }

    /* Al finalizar la simulación se imprime el archivo de salida CSV con las estadísticas */
    printStatistics(processes, num_processes, output_filename);

    /* Liberar recursos de las colas */
    freeQueue(highQueue);
    freeQueue(mediumQueue);
    freeQueue(lowQueue);
}

/* Función de aging: se debe recorrer la cola y, según las reglas,
   mover un proceso a la siguiente cola si no está en RUNNING.
   (Implementación pendiente según las reglas de la tarea) */
void aging(Queue *queue)
{
    // Ejemplo: se podría recorrer la cola y actualizar la prioridad de cada proceso.
    // Esta función es un esqueleto para que completes la lógica.
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
