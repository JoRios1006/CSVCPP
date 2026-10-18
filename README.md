# csvcpp

Lector y analizador de archivos CSV de alto rendimiento en C++ utilizando mapeo de memoria (`mmap`) y parsing sin copias innecesarias (`std::string_view`).

---

## 🛠 Requisitos y Compatibilidad

* **Sistema Operativo:** Linux / Entornos POSIX (utiliza las llamadas de sistema `mmap64`, `open64`, `fstat64`).
* **Compilador:** GCC o Clang con soporte para **C++17** o superior (necesario por `<charconv>` y `std::string_view`).

---

## 🚀 Compilación y Ejecución

Para compilar el proyecto utilizando `g++`:

```bash
g++ -std=c++17 -O3 main2.cpp -o csvcpp
```

Para ejecutar el binario generado:

```bash
./csvcpp
```

*Nota: Asegúrate de tener un archivo CSV válido en la ruta especificada dentro de `main2.cpp` antes de ejecutar.*

---

## 📌 Estado Actual del Proyecto

Actualmente, la infraestructura principal de lectura y parseo está **completa**:

1. **Lectura por Kernel (`mmap64`):** El archivo se carga directamente en espacio de dirección virtual con sugerencias de acceso secuencial (`madvise`).
2. **Parseo de Cabeceras:** Extracción dinámica de los nombres de las columnas en `std::string_view`.
3. **Optimización de Memoria:** Estimación matemática del número de filas para pre-reservar espacio (`reserve`) en los `std::vector<float>` de cada columna y evitar realocaciones.
4. **Parsing Directo (`std::from_chars`):** Conversión directa de los caracteres del buffer mapeado a tipos `float` sin crear `std::string` intermedias.
5. **Estructura de Datos:** Un `std::unordered_map<std::string_view, std::vector<float>>` permite acceder a cualquier columna por su nombre en tiempo $O(1)$.
6. **Ejemplo Funcional:** Ya está implementado el cálculo del promedio para la columna `"volume"` usando `std::accumulate`.

---

## 📋 Tareas Pendientes

El trabajo restante consiste en procesar los datos extraídos según los requerimientos del proyecto:

1. **Sumas y Promedios Adicionales:** Implementar el cálculo para el resto de las columnas del CSV (de forma idéntica o similar a la columna `"volume"`).
2. **Presentación de Resultados:** Dar formato a la salida por pantalla (consola) para mostrar las métricas calculadas por columna.
3. *(Opcional)* Cambiar la ruta fija (`csv_path`) por un argumento pasado por línea de comandos (`argv[1]`).

---

## 🧠 Explicación del Código (`main2.cpp`)

Para trabajar sobre el archivo, ten en cuenta las siguientes secciones principales:

* **Acceso a Columnas:**
  ```cpp
  const std::vector<float>& datos = column_map["nombre_de_columna"];
  ```
* **Cálculo de Suma y Promedio:**
  ```cpp
  float suma = std::accumulate(datos.begin(), datos.end(), 0.0f);
  float promedio = suma / datos.size();
  ```
* **Imprimir columnas disponibles:** El mapa de columnas (`column_map`) permite iterar sobre todas las claves cargadas.

---

## 🐙 Guía Rápida de Git

Si no has usado Git previamente, sigue estos pasos para sincronizar tus cambios:

### 1. Obtener los últimos cambios antes de empezar a programar
Cada vez que retomes el trabajo, ejecuta en la terminal dentro de la carpeta del proyecto:
```bash
git pull
```

### 2. Guardar y subir tus cambios
Una vez que hayas modificado y probado el código:

1. Registra las modificaciones realizadas:
   ```bash
   git add main2.cpp
   ```
2. Crea un punto de guardado (commit) describiendo lo que hiciste:
   ```bash
   git commit -m "Calculo de promedios finalizado y formato de salida agregado"
   ```
3. Subes los cambios al repositorio remoto:
   ```bash
   git push
   ```