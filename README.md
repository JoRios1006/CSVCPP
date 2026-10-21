# csvcpp

Control de presupuesto contra gasto real, escrito en C++17. Lee dos archivos CSV
(`presupuesto.csv` y `movimientos.csv`), acumula los gastos por categoría y muestra
en consola un reporte con barra de progreso y estado (OK, ALERTA o EXCEDIDO).

El proyecto está pensado para que lo pueda seguir alguien que viene de Python:
el archivo principal contiene solo la lógica del práctico y la lectura de CSV
(la parte de bajo nivel) está aislada en otro archivo.

---

## Requisitos

* Sistema operativo: Linux u otro entorno POSIX (usa `open64`, `fstat64` y `mmap64`).
* Compilador: GCC o Clang con soporte para C++17.

## Compilación y ejecución

```bash
g++ -std=c++17 -Wall -Wextra -o tp main2.cpp csv_reader.cpp
./tp
```

Los archivos `presupuesto.csv` y `movimientos.csv` se buscan en el directorio desde
el que se ejecuta el programa.

Salida esperada con los datos de ejemplo:

```
CATEGORIA                   PRESUPUESTO       GASTO REAL       DIFERENCIA     BARRA CONSUMO        ESTADO
--------------------------------------------------------------------------------------------------------
Alquiler y Servicios     $  150000.00   $  120000.00   $   30000.00   [████████░░] 80.0%   ALERTA
Insumos y Oficina        $   45000.00   $   50000.00   $   -5000.00   [██████████] 111.1%  EXCEDIDO
...
```

---

## Estructura del proyecto

| Archivo          | Contenido                                                          | Hay que leerlo si...                    |
|------------------|--------------------------------------------------------------------|-----------------------------------------|
| `main2.cpp`      | Los incisos 1 a 5, las estructuras de datos, el reporte y `main`.  | Querés entender el práctico.            |
| `csv_reader.hpp` | Declara `recorrer_csv` y `a_double`, con comentarios de uso.       | Querés saber cómo se lee un CSV.        |
| `csv_reader.cpp` | Implementación de bajo nivel: `mmap`, recorte de texto y parseo.   | Te interesa el rendimiento o el sistema.|

Para resolver o modificar el práctico alcanza con `main2.cpp` y `csv_reader.hpp`.
`csv_reader.cpp` se puede tratar como una caja negra.

---

## Cómo leer `main2.cpp` si venís de Python

El archivo está dividido en tres partes comentadas.

### Parte A: tipos de datos

```cpp
struct Movimiento {
    std::string fecha;
    std::string categoria;
    std::string descripcion;
    double      monto_real = 0.0;
};

using Diccionario = std::map<std::string, double>;
```

* `struct` es como una clase de Python que solo guarda datos (similar a un `dataclass`).
  La consigna pide "una lista de diccionarios" para los movimientos; acá cada
  diccionario es un `Movimiento`, con campos de tipo fijo.
* `Diccionario` es un alias para `std::map<std::string, double>`: un `dict[str, float]`.
  Se mantiene ordenado por clave, por eso el reporte sale en orden alfabético.
* `std::vector<Movimiento>` es el equivalente a `list[Movimiento]`.

### Parte B: los incisos

| Inciso | Función                            | Equivalente en Python                              |
|--------|------------------------------------|----------------------------------------------------|
| 1      | `cargar_presupuesto(archivo)`      | Devuelve `dict[str, float]`                        |
| 2      | `cargar_movimientos(archivo)`      | Devuelve `list[Movimiento]`                        |
| 3      | `calcular_totales_reales(lista)`   | `defaultdict(float)` acumulando por categoría      |
| 4      | `generar_barra_progreso(pct)`      | Devuelve un `str` como `"[████████░░] 80.0%"`     |
| 5      | `evaluar_estado(pct)`              | Devuelve `"OK"`, `"ALERTA"` o `"EXCEDIDO"`         |

**Incisos 1 y 2: lectura con `recorrer_csv`**

```cpp
recorrer_csv(nombre_archivo, {"categoria", "monto_presupuestado"},
    [&](const Fila& celdas) {
        double monto = 0.0;
        if (a_double(celdas[1], monto))
            presupuesto[std::string(celdas[0])] = monto;
    });
```

Es el equivalente de:

```python
for fila in csv.DictReader(open(nombre_archivo)):
    presupuesto[fila["categoria"]] = float(fila["monto_presupuestado"])
```

* El segundo argumento es la lista de columnas que querés recibir. En cada vuelta,
  `celdas[0]` es la primera columna pedida y `celdas[1]` la segunda.
* `[&](const Fila& celdas) { ... }` es una **lambda**: una función sin nombre, igual
  que `lambda` en Python pero con cuerpo de varias líneas. El `[&]` le permite usar
  las variables de afuera (acá, `presupuesto`).
* `a_double(texto, resultado)` intenta convertir el texto a número. Devuelve
  `false` si no se puede, en lugar de lanzar una excepción como `float()`.

**Inciso 3: acumulación**

```cpp
for (const Movimiento& m : movimientos)
    totales[m.categoria] += m.monto_real;
```

En un `std::map`, acceder a una clave que no existe la crea con valor `0.0`. Por eso no
hace falta preguntar antes si la categoría ya está, como harías con un `dict` común.

**Inciso 4: barra de progreso**

Usa una tabla con las 11 barras posibles (0 a 10 bloques llenos) y elige una por
índice. El índice sale de dividir el porcentaje por 10, truncado, y se limita al
rango 0 a 10. Si el consumo supera el 100%, la barra queda completa pero el texto
muestra el porcentaje real.

**Inciso 5: estado**

Menor a 80: `OK`. De 80 a 100 inclusive: `ALERTA`. Mayor a 100: `EXCEDIDO`.

### Parte C: reporte

`main` carga los datos, calcula los totales y recorre el presupuesto imprimiendo
una línea por categoría con `imprimir_linea_presupuesto`. Al final agrega la fila
`TOTAL`.

```cpp
for (const auto& [categoria, presupuestado] : presupuesto)
```

Es el equivalente de `for categoria, presupuestado in presupuesto.items():`.

---

## Diferencias de C++ que conviene tener presentes

| Concepto                  | En Python                    | En este proyecto                                            |
|---------------------------|------------------------------|-------------------------------------------------------------|
| Tipos                     | Dinámicos                    | Declarados: `double`, `std::string`, `int`.                 |
| `str`                     | Un solo tipo                 | `std::string` (copia el texto) y `std::string_view` (solo lo mira). |
| `list`                    | `list`                       | `std::vector<T>`                                            |
| `dict`                    | `dict`                       | `std::map<K, V>`                                            |
| Errores de conversión     | Excepción (`ValueError`)     | Se devuelve `false` y se avisa por `std::cerr`.             |
| Memoria                   | Automática                   | Automática para `string` y `vector`; el archivo mapeado se libera con `unmap_file`. |
| Compilación               | Se ejecuta directo           | Hay que compilar antes de cada ejecución.                   |
| `print`                   | `print(f"...")`              | `std::printf("%-24s $ %10.2f\n", ...)`                      |

Dos cosas que suelen confundir:

* **`std::string_view`**: es una "vista" del texto, sin copiarlo. Las celdas que recibe
  la lambda apuntan directamente al archivo en memoria y solo valen mientras dura esa
  vuelta. Para guardarlas hay que copiarlas: `std::string(celdas[0])`.
* **`c_str()`**: `printf` es una función de C y no entiende `std::string`. Con
  `texto.c_str()` se obtiene la versión que sí entiende.

---

## Cómo funciona la lectura de CSV (`csv_reader.cpp`)

No hace falta para resolver el práctico, pero explica por qué es rápido.

1. **`map_file_to_memory`**: abre el archivo (`open64`), pregunta su tamaño (`fstat64`)
   y lo mapea con `mmap64`. El archivo aparece como un arreglo de caracteres en
   memoria, sin lecturas ni copias. Luego `madvise` le avisa al kernel que se leerá
   en orden (`MADV_SEQUENTIAL`), que se necesitará pronto (`MADV_WILLNEED`) y que
   conviene cargarlo de una vez (`MADV_POPULATE_READ`, disponible desde Linux 5.14).
2. **Parseo por línea**: se recorre el texto buscando `\n`, se parte cada línea por
   comas y se recortan espacios, tabs y `\r`. Todo con `std::string_view`, sin copiar.
3. **Selección de columnas**: la primera línea es el encabezado. Se ubican las columnas
   pedidas por nombre y, para cada fila, se entregan a la lambda solo esas celdas, en
   el orden pedido.
4. **`unmap_file`**: al terminar se libera el mapeo y se cierra el archivo.
5. **`a_double`**: convierte con `std::from_chars`, sin crear objetos temporales.
   Ignora espacios internos, y por eso lee `2 8000.00` como `28000.00`.

Si falta una columna pedida, `recorrer_csv` avisa por pantalla y devuelve `false`.
Las filas con menos celdas de las esperadas se ignoran.

---

## Limitaciones conocidas

* El parser no soporta campos entre comillas ni comas dentro de un campo.
* Las rutas de los CSV están fijas en `main`.
* Una categoría con gastos pero sin presupuesto no aparece en el reporte; se avisa por `stderr`.
* Linux y POSIX únicamente.

---

## Guía rápida de Git

### Antes de empezar a programar

```bash
git pull
```

### Guardar y subir cambios

1. Registrar los archivos modificados:
   ```bash
   git add main2.cpp csv_reader.cpp csv_reader.hpp README.md
   ```
2. Crear el commit con una descripción de lo hecho:
   ```bash
   git commit -m "Descripcion breve del cambio"
   ```
3. Subir al repositorio remoto:
   ```bash
   git push
   ```
