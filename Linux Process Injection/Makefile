processInjectionProject: target.c processInjection.c ld-processInjection.c
	gcc -o target target.c -I.
	gcc -o processInjection processInjection.c -I.
	gcc -shared -o ld-processInjection.so -fPIC ld-processInjection.c
clean:
	@rm -f *.o processInjection target ld-processInjection.so
