#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include "../file_manager/manager.h"

int main(int argc, char const *argv[])
{
	/*Lectura del input*/
	char *file_name = (char *)argv[1];
	InputFile *input_file = read_file(file_name);

	/*Mostramos el archivo de input en consola*/
	printf("Cantidad de procesos: %d\n", input_file->len);
	printf("Procesos:\n");
	for (int i = 0; i < input_file->len; ++i)
	{
		printf("  NOMBRE_PROCESO: %s\n", input_file->lines[i][0]);
		printf("  PID: %s\n", input_file->lines[i][1]);
		printf("  T_INICIO: %s\n", input_file->lines[i][2]);
		printf("  T_CPU_BURST: %s\n", input_file->lines[i][3]);
		printf("  N_BURSTS: %s\n", input_file->lines[i][4]);
		printf("  IO_WAIT: %s\n", input_file->lines[i][5]);
		printf("  PRIORITY: %s\n", input_file->lines[i][6]);
		printf("\n");
	}

	input_file_destroy(input_file);
}