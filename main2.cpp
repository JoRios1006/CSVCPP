// ============================================================
//  Control de presupuesto vs gasto real
//  Lectura de CSV con mmap + reporte en consola.
// ============================================================

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>        // printf
#include <fcntl.h>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// ============================================================
// PARTE A: LECTURA DE ARCHIVOS (mmap + tabla)
// ============================================================

// ---------- A1) Archivo mapeado ----------
struct ArchivoMapeado {
    const char* datos = nullptr;  // primer byte del archivo en memoria
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

// ---------- A2) Tabla CSV ----------
// OJO: los string_view apuntan DENTRO del archivo mapeado.
// Solo son válidos mientras no se llame unmap_file.
using Fila = std::vector<std::string_view>;

struct Tabla {
    Fila              encabezados;
    std::vector<Fila> filas;
};

std::string_view recortar(std::string_view texto) {
    const char* blancos = " \t\r";
    size_t ini = texto.find_first_not_of(blancos);
    if (ini == std::string_view::npos) return {};
    size_t fin = texto.find_last_not_of(blancos);
    return texto.substr(ini, fin - ini + 1);
}

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

Tabla parse_csv(const ArchivoMapeado& archivo) {
    Tabla tabla;
    if (!archivo.valido()) return tabla;

    std::string_view contenido(archivo.datos, archivo.tamano);
    bool es_primera_linea = true;
    size_t pos = 0;

    while (pos < contenido.size()) {
        size_t salto = contenido.find('\n', pos);
        if (salto == std::string_view::npos) salto = contenido.size();

        std::string_view linea = recortar(contenido.substr(pos, salto - pos));
        pos = salto + 1;
        if (linea.empty()) continue;

        if (es_primera_linea) {
            tabla.encabezados = dividir_linea(linea);
            es_primera_linea = false;
        } else {
            tabla.filas.push_back(dividir_linea(linea));
        }
    }
    return tabla;
}

// ---------- A3) Ayudantes ----------
int indice_columna(const Tabla& tabla, std::string_view nombre) {
    for (size_t i = 0; i < tabla.encabezados.size(); ++i)
        if (tabla.encabezados[i] == nombre) return static_cast<int>(i);
    return -1;
}

// Texto -> double. Ignora espacios internos: "2 8000.00" se lee como 28000.00
// (el movimientos.csv de la cátedra trae ese espacio de más).
bool a_double(std::string_view texto, double& resultado) {
    std::string limpio;
    for (char c : texto)
        if (c != ' ') limpio += c;

    auto r = std::from_chars(limpio.data(), limpio.data() + limpio.size(), resultado);
    return r.ec == std::errc() && r.ptr == limpio.data() + limpio.size();
}

// ============================================================
// PARTE B: TIPOS DE DATOS DEL PROBLEMA
// ============================================================

// A diferencia de la Tabla, estos datos son COPIAS (std::string):
// siguen válidos después de cerrar el archivo.
struct Movimiento {
    std::string fecha;
    std::string categoria;
    std::string descripcion;
    double      monto_real = 0.0;
};

// "Diccionario" categoría -> monto. std::map los mantiene ordenados por nombre.
using Diccionario = std::map<std::string, double>;

// ============================================================
// PARTE C: INCISOS
// ============================================================

// ---------- Inciso 1 ----------
Diccionario cargar_presupuesto(const std::string& nombre_archivo) {
    Diccionario presupuesto;

    ArchivoMapeado archivo = map_file_to_memory(nombre_archivo);
    if (!archivo.valido()) {
        std::cerr << "No se pudo abrir " << nombre_archivo << "\n";
        return presupuesto;
    }

    Tabla tabla = parse_csv(archivo);
    int col_cat   = indice_columna(tabla, "categoria");
    int col_monto = indice_columna(tabla, "monto_presupuestado");

    if (col_cat >= 0 && col_monto >= 0) {
        for (const Fila& fila : tabla.filas) {
            double monto = 0.0;
            if (a_double(fila[col_monto], monto))
                presupuesto[std::string(fila[col_cat])] = monto;  // string_view -> string (copia)
            else
                std::cerr << "Monto invalido en presupuesto: " << fila[col_cat] << "\n";
        }
    }

    unmap_file(archivo);  // las copias ya están guardadas, se puede cerrar
    return presupuesto;
}

// ---------- Inciso 2 ----------
std::vector<Movimiento> cargar_movimientos(const std::string& nombre_archivo) {
    std::vector<Movimiento> movimientos;

    ArchivoMapeado archivo = map_file_to_memory(nombre_archivo);
    if (!archivo.valido()) {
        std::cerr << "No se pudo abrir " << nombre_archivo << "\n";
        return movimientos;
    }

    Tabla tabla = parse_csv(archivo);
    int col_fecha = indice_columna(tabla, "fecha");
    int col_cat   = indice_columna(tabla, "categoria");
    int col_desc  = indice_columna(tabla, "descripcion");
    int col_monto = indice_columna(tabla, "monto_real");

    if (col_fecha >= 0 && col_cat >= 0 && col_desc >= 0 && col_monto >= 0) {
        for (const Fila& fila : tabla.filas) {
            Movimiento m;
            m.fecha       = std::string(fila[col_fecha]);
            m.categoria   = std::string(fila[col_cat]);
            m.descripcion = std::string(fila[col_desc]);

            if (a_double(fila[col_monto], m.monto_real))
                movimientos.push_back(m);
            else
                std::cerr << "Monto invalido, fila ignorada: " << fila[col_desc] << "\n";
        }
    }

    unmap_file(archivo);
    return movimientos;
}

// ---------- Inciso 3 ----------
Diccionario calcular_totales_reales(const std::vector<Movimiento>& movimientos) {
    Diccionario totales;
    for (const Movimiento& m : movimientos)
        totales[m.categoria] += m.monto_real;  // si no existe, arranca en 0.0
    return totales;
}

// ---------- Inciso 4 ----------
// Tabla de barras precalculadas: la posición i tiene i bloques llenos.
// Devuelve, por ejemplo: "[████████░░] 80.0%"
std::string generar_barra_progreso(double porcentaje) {
    static const char* barras[] = {
        "░░░░░░░░░░",
        "█░░░░░░░░░",
        "██░░░░░░░░",
        "███░░░░░░░",
        "████░░░░░░",
        "█████░░░░░",
        "██████░░░░",
        "███████░░░",
        "████████░░",
        "█████████░",
        "██████████"
    };

    int indice = static_cast<int>(porcentaje / 10.0);  // 80.0 -> 8, 79.9 -> 7
    if (indice < 0)  indice = 0;                       // negativo: barra vacía
    if (indice > 10) indice = 10;                      // exceso: barra completa

    char texto_pct[16];
    std::snprintf(texto_pct, sizeof(texto_pct), " %.1f%%", porcentaje);  // siempre el % real
    return std::string("[") + barras[indice] + "]" + texto_pct;
}

// ---------- Inciso 5 ----------
std::string evaluar_estado(double porcentaje) {
    if (porcentaje < 80.0)   return "OK";
    if (porcentaje <= 100.0) return "ALERTA";
    return "EXCEDIDO";
}

// ============================================================
// PARTE D: REPORTE
// ============================================================

// Cuenta caracteres visibles de un texto UTF-8 (█ y ░ ocupan 3 bytes cada uno,
// por eso printf("%-19s") no alinea bien con ellos).
size_t largo_visible(const std::string& texto) {
    size_t n = 0;
    for (unsigned char c : texto)
        if ((c & 0xC0) != 0x80) ++n;  // saltar bytes de continuación
    return n;
}

void imprimir_linea_presupuesto(const std::string& categoria, double gastado,
                                double presupuestado,
                                const std::string& barra, const std::string& estado) {
    double diferencia = presupuestado - gastado;
    const size_t ANCHO_BARRA = 19;
    std::string relleno(ANCHO_BARRA - largo_visible(barra), ' ');  // alinear a mano

    // c_str() convierte std::string a char* para usarlo con printf.
    std::printf("%-24s $ %10.2f   $ %10.2f   $ %10.2f   %s%s  %s\n",
                categoria.c_str(), presupuestado, gastado, diferencia,
                barra.c_str(), relleno.c_str(), estado.c_str());
}

int main() {
    Diccionario presupuesto = cargar_presupuesto("presupuesto.csv");
    std::vector<Movimiento> movimientos = cargar_movimientos("movimientos.csv");
    Diccionario gastos = calcular_totales_reales(movimientos);

    std::printf("%-24s   %12s     %12s     %12s     %-19s  %s\n",
                "CATEGORIA", "PRESUPUESTO", "GASTO REAL", "DIFERENCIA",
                "BARRA CONSUMO", "ESTADO");
    std::printf("-------------------------------------------------------------------"
                "-------------------------------------\n");

    double total_presup = 0.0, total_gasto = 0.0;

    for (const auto& [categoria, presupuestado] : presupuesto) {
        double gastado = gastos.count(categoria) ? gastos[categoria] : 0.0;
        double pct = presupuestado > 0.0 ? gastado / presupuestado * 100.0 : 0.0;

        imprimir_linea_presupuesto(categoria, gastado, presupuestado,
                                   generar_barra_progreso(pct), evaluar_estado(pct));

        total_presup += presupuestado;
        total_gasto  += gastado;
    }

    // Gastos en categorías que no tienen presupuesto.
    for (const auto& [categoria, gastado] : gastos)
        if (!presupuesto.count(categoria))
            std::cerr << "Aviso: gasto sin presupuesto en '" << categoria << "'\n";

    double pct_total = total_presup > 0.0 ? total_gasto / total_presup * 100.0 : 0.0;
    std::printf("-------------------------------------------------------------------"
                "-------------------------------------\n");
    imprimir_linea_presupuesto("TOTAL", total_gasto, total_presup,
                               generar_barra_progreso(pct_total), evaluar_estado(pct_total));
    return 0;
}
