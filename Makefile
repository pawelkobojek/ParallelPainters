FILE=main

flags=-Wall

ifeq (run,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(RUN_ARGS):;@:)
endif

#ifeq(${#RUN_ARGS})

compile: ${FILE}.c
	gcc ${flags} -o ${FILE}.o -c ${FILE}.c 
	gcc ${flags} -lpthread -pthread -o ${FILE} ${FILE}.o
run: ${FILE} 
	./${FILE} ${RUN_ARGS}
clean:
	-rm -f ${FILE}.o ${FILE}
