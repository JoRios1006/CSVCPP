// ============================================================
//  Lectura rápida de CSV con mmap
//  Aquí solo está la parte de "archivo -> memoria -> tabla".
//  Los incisos 1 a 5 se construyen encima de esto.
// ============================================================

#include <charconv>      // std::from_chars (texto -> número, muy rápido)
#include <cstddef>       // size_t
#include <fcntl.h>       // open64, O_RDONLY
#include <iostream>
#include <string>
#include <string_view>   // vista de texto SIN copiar
#include <sys/mman.h>    // mmap64, munmap, madvise
#include <sys/stat.h>    // fstat64 (tamaño del archivo)
#include <unistd.h>      // close
#include <vector>

// ------------------------------------------------------------
// 1) ARCHIVO MAPEADO
// ------------------------------------------------------------
// mmap hace que el contenido del archivo aparezca como si fuera
// un array de chars en memoria. No hay que hacer read() ni copiar.
struct ArchivoMapeado {
    const char* datos = nullptr;  // puntero al primer byte del archivo
    size_t      tamano = 0;       // cantidad de bytes
    int         fd = -1;          // descriptor de archivo (-1 = no abierto)

    bool valido() const { return datos != nullptr; }
};

// Abre el archivo y lo mapea. Si algo falla devuelve un ArchivoMapeado vacío.
ArchivoMapeado map_file_to_memory(const std::string& ruta) {
    // Paso 1: pedir el descriptor de archivo al sistema operativo.
    int fd = open64(ruta.c_str(), O_RDONLY);
    if (fd < 0) return {};

    // Paso 2: preguntar cuántos bytes pesa (fstat64 llena la struct st).
    struct stat64 st;
    if (fstat64(fd, &st) < 0 || st.st_size == 0) {  // mmap falla con tamaño 0
        close(fd);
        return {};
    }
    size_t tamano = static_cast<size_t>(st.st_size);

    // Paso 3: reservar el espacio virtual de solo lectura (PROT_READ).
    // MAP_PRIVATE = cambios no vuelven al archivo (igual no escribimos).
    void* ptr = mmap64(nullptr, tamano, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        return {};
    }

    // Paso 4: avisarle al kernel cómo vamos a leer, para optimizar la paginación.
    madvise(ptr, tamano, MADV_SEQUENTIAL);  // leeremos de principio a fin
    madvise(ptr, tamano, MADV_WILLNEED);    // vamos a necesitarlo pronto
#ifdef MADV_POPULATE_READ                   // solo existe en kernels Linux >= 5.14
    madvise(ptr, tamano, MADV_POPULATE_READ);  // cargarlo ya, evita fallos de página
#endif

    return {static_cast<const char*>(ptr), tamano, fd};
}

// Libera lo que pidió map_file_to_memory. Siempre llamarla al terminar.
void unmap_file(ArchivoMapeado& archivo) {
    if (archivo.datos) munmap(const_cast<char*>(archivo.datos), archivo.tamano);
    if (archivo.fd >= 0) close(archivo.fd);
    archivo = {};  // dejarlo en estado "vacío"
}

// ------------------------------------------------------------
// 2) TABLA CSV
// ------------------------------------------------------------
// Importante: los string_view NO copian texto, apuntan dentro del
// archivo mapeado. Por eso el archivo debe seguir mapeado mientras
// se use la tabla.
using Fila = std::vector<std::string_view>;

struct Tabla {
    Fila              encabezados;  // nombres de columnas
    std::vector<Fila> filas;        // cada fila = una lista de celdas
};

// Quita espacios, tabs y '\r' al inicio y al final.
// (Tus CSV tienen espacios después de las comas: "fecha, categoria")
std::string_view recortar(std::string_view texto) {
    const char* blancos = " \t\r";
    size_t ini = texto.find_first_not_of(blancos);
    if (ini == std::string_view::npos) return {};  // todo eran blancos
    size_t fin = texto.find_last_not_of(blancos);
    return texto.substr(ini, fin - ini + 1);
}

// Corta una línea en celdas separadas por coma.
Fila dividir_linea(std::string_view linea) {
    Fila celdas;
    size_t inicio = 0;
    while (true) {
        size_t coma = linea.find(',', inicio);
        if (coma == std::string_view::npos) {      // última celda
            celdas.push_back(recortar(linea.substr(inicio)));
            break;
        }
        celdas.push_back(recortar(linea.substr(inicio, coma - inicio)));
        inicio = coma + 1;
    }
    return celdas;
}

// Convierte todo el archivo mapeado en una Tabla.
Tabla parse_csv(const ArchivoMapeado& archivo) {
    Tabla tabla;
    if (!archivo.valido()) return tabla;

    std::string_view contenido(archivo.datos, archivo.tamano);
    bool es_primera_linea = true;
    size_t pos = 0;

    while (pos < contenido.size()) {
        // Buscar el fin de la línea actual.
        size_t salto = contenido.find('\n', pos);
        if (salto == std::string_view::npos) salto = contenido.size();

        std::string_view linea = recortar(contenido.substr(pos, salto - pos));
        pos = salto + 1;  // saltar el '\n'

        if (linea.empty()) continue;  // ignorar líneas vacías

        if (es_primera_linea) {
            tabla.encabezados = dividir_linea(linea);
            es_primera_linea = false;
        } else {
            tabla.filas.push_back(dividir_linea(linea));
        }
    }
    return tabla;
}

// ------------------------------------------------------------
// 3) AYUDANTES DE CONVERSIÓN
// ------------------------------------------------------------
// Texto -> float. Devuelve false si el texto no es un número válido.
bool a_float(std::string_view texto, float& resultado) {
    auto r = std::from_chars(texto.data(), texto.data() + texto.size(), resultado);
    // Válido solo si no hubo error Y se consumió todo el texto.
    return r.ec == std::errc() && r.ptr == texto.data() + texto.size();
}

// Índice de una columna por nombre, o -1 si no existe.
int indice_columna(const Tabla& tabla, std::string_view nombre) {
    for (size_t i = 0; i < tabla.encabezados.size(); ++i)
        if (tabla.encabezados[i] == nombre) return static_cast<int>(i);
    return -1;
}

// ------------------------------------------------------------
// 4) PROGRAMA DE PRUEBA
// ------------------------------------------------------------
void mostrar_tabla(const Tabla& tabla) {
    for (auto h : tabla.encabezados) std::cout << "[" << h << "] ";
    std::cout << "\n";
    for (const Fila& fila : tabla.filas) {
        for (auto celda : fila) std::cout << "[" << celda << "] ";
        std::cout << "\n";
    }
}

int main() {
    // Siempre: mapear -> usar -> desmapear.
    ArchivoMapeado archivo = map_file_to_memory("presupuesto.csv");
    if (!archivo.valido()) {
        std::cerr << "No se pudo abrir el archivo\n";
        return 1;
    }

    Tabla tabla = parse_csv(archivo);
    mostrar_tabla(tabla);

    // Ejemplo: leer la columna numérica.
    int col = indice_columna(tabla, "monto_presupuestado");
    for (const Fila& fila : tabla.filas) {
        float monto = 0.0f;
        if (col >= 0 && a_float(fila[col], monto))
            std::cout << fila[0] << " -> " << monto << "\n";
        else
            std::cout << fila[0] << " -> monto INVALIDO\n";
    }

    unmap_file(archivo);
    return 0;
}
