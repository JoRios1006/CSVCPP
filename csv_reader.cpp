// ============================================================
//  csv_reader.cpp
//  Implementación de la lectura de CSV con mmap.
// ============================================================

#include "csv_reader.hpp"

#include <charconv>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Todo lo de este bloque es privado de este archivo.
namespace {

// ---------- Archivo mapeado ----------
// mmap hace que el contenido del archivo aparezca como un array de chars
// en memoria, sin read() ni copias.
struct ArchivoMapeado {
    const char* datos = nullptr;
    size_t      tamano = 0;
    int         fd = -1;

    bool valido() const { return datos != nullptr; }
};

ArchivoMapeado map_file_to_memory(const std::string& ruta) {
    int fd = open64(ruta.c_str(), O_RDONLY);          // 1. descriptor
    if (fd < 0) return {};

    struct stat64 st;                                  // 2. tamaño
    if (fstat64(fd, &st) < 0 || st.st_size == 0) {
        close(fd);
        return {};
    }
    size_t tamano = static_cast<size_t>(st.st_size);

    void* ptr = mmap64(nullptr, tamano, PROT_READ, MAP_PRIVATE, fd, 0);  // 3. mapear
    if (ptr == MAP_FAILED) {
        close(fd);
        return {};
    }

    madvise(ptr, tamano, MADV_SEQUENTIAL);             // 4. optimizar paginación
    madvise(ptr, tamano, MADV_WILLNEED);
#ifdef MADV_POPULATE_READ
    madvise(ptr, tamano, MADV_POPULATE_READ);
#endif

    return {static_cast<const char*>(ptr), tamano, fd};
}

void unmap_file(ArchivoMapeado& archivo) {
    if (archivo.datos) munmap(const_cast<char*>(archivo.datos), archivo.tamano);
    if (archivo.fd >= 0) close(archivo.fd);
    archivo = {};
}

// ---------- Texto ----------
// Quita espacios, tabs y '\r' al inicio y al final.
std::string_view recortar(std::string_view texto) {
    const char* blancos = " \t\r";
    size_t ini = texto.find_first_not_of(blancos);
    if (ini == std::string_view::npos) return {};
    size_t fin = texto.find_last_not_of(blancos);
    return texto.substr(ini, fin - ini + 1);
}

// Corta una línea en celdas separadas por coma.
Fila dividir_linea(std::string_view linea) {
    Fila celdas;
    size_t inicio = 0;
    while (true) {
        size_t coma = linea.find(',', inicio);
        if (coma == std::string_view::npos) {
            celdas.push_back(recortar(linea.substr(inicio)));
            break;
        }
        celdas.push_back(recortar(linea.substr(inicio, coma - inicio)));
        inicio = coma + 1;
    }
    return celdas;
}

// Posición de cada columna pedida dentro del encabezado, o -1 si falta.
std::vector<int> buscar_indices(const Fila& encabezados,
                                const std::vector<std::string_view>& pedidas) {
    std::vector<int> indices;
    for (std::string_view nombre : pedidas) {
        int indice = -1;
        for (size_t i = 0; i < encabezados.size(); ++i)
            if (encabezados[i] == nombre) indice = static_cast<int>(i);
        indices.push_back(indice);
    }
    return indices;
}

}  // namespace

// ============================================================
// Funciones públicas (declaradas en csv_reader.hpp)
// ============================================================

bool recorrer_csv(const std::string& ruta,
                  const std::vector<std::string_view>& columnas,
                  const std::function<void(const Fila&)>& por_fila) {
    ArchivoMapeado archivo = map_file_to_memory(ruta);
    if (!archivo.valido()) {
        std::cerr << "No se pudo abrir " << ruta << "\n";
        return false;
    }

    std::string_view contenido(archivo.datos, archivo.tamano);
    std::vector<int> indices;
    bool es_encabezado = true;
    bool ok = true;
    size_t pos = 0;

    while (pos < contenido.size() && ok) {
        // Buscar el fin de la línea actual.
        size_t salto = contenido.find('\n', pos);
        if (salto == std::string_view::npos) salto = contenido.size();

        std::string_view linea = recortar(contenido.substr(pos, salto - pos));
        pos = salto + 1;
        if (linea.empty()) continue;

        Fila celdas = dividir_linea(linea);

        if (es_encabezado) {
            indices = buscar_indices(celdas, columnas);
            for (size_t i = 0; i < indices.size(); ++i) {
                if (indices[i] < 0) {
                    std::cerr << "Falta la columna '" << columnas[i] << "' en " << ruta << "\n";
                    ok = false;
                }
            }
            es_encabezado = false;
            continue;
        }

        // Armar solo las celdas pedidas, en el orden pedido.
        Fila seleccion;
        bool fila_completa = true;
        for (int idx : indices) {
            if (static_cast<size_t>(idx) >= celdas.size()) { fila_completa = false; break; }
            seleccion.push_back(celdas[idx]);
        }
        if (fila_completa) por_fila(seleccion);
    }

    unmap_file(archivo);  // por_fila ya terminó: ninguna vista queda en uso
    return ok;
}

bool a_double(std::string_view texto, double& resultado) {
    std::string limpio;
    for (char c : texto)
        if (c != ' ') limpio += c;

    auto r = std::from_chars(limpio.data(), limpio.data() + limpio.size(), resultado);
    return r.ec == std::errc() && r.ptr == limpio.data() + limpio.size();
}
