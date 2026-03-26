CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2
CFLAGS_DBG = -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS = -lm

SRC  = src
TEST = tests

SRCS = $(SRC)/atmosphere.c \
       $(SRC)/sensors.c    \
       $(SRC)/airdata.c    \
       $(SRC)/cadc.c       \
       $(SRC)/logger.c

cadc_f14: $(SRCS) main.c
	$(CC) $(CFLAGS) -I. \
	    $(SRCS) main.c \
	    -o cadc_f14 $(LDFLAGS)
	@echo "✓ Compilado: cadc_f14"

# Target por defecto
all: cadc_f14 test_atmosphere test_sensors test_airdata test_cadc test_logger
	@echo ""
	@echo "Proyecto completo compilado."

run: cadc_f14
	./cadc_f14 $(ARGS)

test_atmosphere: $(SRC)/atmosphere.c $(TEST)/test_atmosphere.c
	$(CC) $(CFLAGS_DBG) -I$(SRC) \
	    $(SRC)/atmosphere.c \
	    $(TEST)/test_atmosphere.c \
	    -o $(TEST)/test_atmosphere $(LDFLAGS)
	@echo "Compilado: test_atmosphere"

test_sensors: $(SRC)/atmosphere.c $(SRC)/sensors.c $(TEST)/test_sensors.c
	$(CC) $(CFLAGS_DBG) -I$(SRC) \
	    $(SRC)/atmosphere.c \
	    $(SRC)/sensors.c \
	    $(TEST)/test_sensors.c \
	    -o $(TEST)/test_sensors $(LDFLAGS)
	@echo "Compilado: test_sensors"

test_airdata: $(SRCS) $(TEST)/test_airdata.c
	$(CC) $(CFLAGS_DBG) -I$(SRC) \
	    $(SRC)/atmosphere.c \
	    $(SRC)/sensors.c \
	    $(SRC)/airdata.c \
	    $(TEST)/test_airdata.c \
	    -o $(TEST)/test_airdata $(LDFLAGS)
	@echo "Compilado: test_airdata"

test_cadc: $(SRCS) $(TEST)/test_cadc.c
	$(CC) $(CFLAGS_DBG) -I$(SRC) \
	    $(SRCS) \
	    $(TEST)/test_cadc.c \
	    -o $(TEST)/test_cadc $(LDFLAGS)
	@echo "Compilado: test_cadc"

test_logger: $(SRCS) $(TEST)/test_logger.c
	$(CC) $(CFLAGS_DBG) -I$(SRC) \
	    $(SRCS) \
	    $(TEST)/test_logger.c \
	    -o $(TEST)/test_logger $(LDFLAGS)
	@echo "Compilado: test_logger"

tests: test_atmosphere test_sensors test_airdata test_cadc test_logger
	@echo ""
	@echo "Ejecutando test_atmosphere"
	./$(TEST)/test_atmosphere
	@echo ""
	@echo "Ejecutando test_sensors"
	./$(TEST)/test_sensors
	@echo ""
	@echo "Ejecutando test_airdata"
	./$(TEST)/test_airdata
	@echo ""
	@echo "Ejecutando test_cadc"
	./$(TEST)/test_cadc
	@echo ""
	@echo "Ejecutando test_logger"
	./$(TEST)/test_logger


clean:
	rm -f cadc_f14
	rm -f $(TEST)/test_atmosphere $(TEST)/test_sensors \
	      $(TEST)/test_airdata $(TEST)/test_cadc \
	      $(TEST)/test_logger
	rm -f output/*.csv
	@echo "Limpieza completada"

.PHONY: all run tests clean
