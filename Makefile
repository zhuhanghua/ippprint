objects=ipp.o job.o util.o thread.o  worker.o print.o client_main.o 
     
client:$(objects)
	gcc -o $@ $^ -lpthread
..c.o:
	gcc -g -c $<
clean :
	-rm client $(objects)