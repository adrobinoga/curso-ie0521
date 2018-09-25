# Tarea Programada I #

**Autor**: Alexander Marin

## Instrucciones ##

Para compilar el programa se puede utilizar:

	$ make

Una vez generado el ejecutable **cache**, se puede ejecutar una simulación con el siguiente
comando:

	$ gunzip -c mcf.trace.gz | cache -t <cache size> -a <number of ways> -l <line size>

Donde:

- **cache size**: Es el tamaño del cache en KB.
- **number of ways**: Es la asociatividad del cache, es decir asociatividad.
- **line size**: Es el tamaño de la línea en bytes.

### Pruebas ###

Para correr una prueba ya establecida se puede utilizar el comando:

	$ make run
