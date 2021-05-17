encrypt352: encrypt352.c encrypt-module.c
	gcc encrypt352.c encrypt-module.c -o encrypt352 -lpthread

clean:
	rm encrypt352
