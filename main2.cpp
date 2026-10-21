#include <cstdio>        // printf
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "csv_reader.hpp"  // recorrer_csv() y a_double(): la lectura del CSV vive allá

// TIPOS DE DATOS DEL PROBLEMA
struct Movimiento {
    std::string fecha;
    std::string categoria;
    std::string descripcion;
    double      monto_real = 0.0;
};

// "Diccionario" categoría -> monto. std::map los mantiene ordenados por nombre.
using Diccionario = std::map<std::string, double>;

// INCISOS
// ---------- Inciso 1 ----------
Diccionario cargar_presupuesto(const std::string& nombre_archivo) {
    Diccionario presupuesto;

    // Por cada fila del CSV se ejecuta el bloque [&](...) { ... }
    // (es el cuerpo de un "for fila in csv" de Python).
    recorrer_csv(nombre_archivo, {"categoria", "monto_presupuestado"},
        [&](const Fila& celdas) {
            double monto = 0.0;
            if (a_double(celdas[1], monto))
                presupuesto[std::string(celdas[0])] = monto;
            else
                std::cerr << "Monto invalido en presupuesto: " << celdas[0] << "\n";
        });

    return presupuesto;
}

// ---------- Inciso 2 ----------
std::vector<Movimiento> cargar_movimientos(const std::string& nombre_archivo) {
    std::vector<Movimiento> movimientos;

    recorrer_csv(nombre_archivo, {"fecha", "categoria", "descripcion", "monto_real"},
        [&](const Fila& celdas) {
            Movimiento m;
            m.fecha       = std::string(celdas[0]);
            m.categoria   = std::string(celdas[1]);
            m.descripcion = std::string(celdas[2]);

            if (a_double(celdas[3], m.monto_real))
                movimientos.push_back(m);
            else
                std::cerr << "Monto invalido, fila ignorada: " << m.descripcion << "\n";
        });

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

    char texto_pct[16]; // padding de 16 caracteres para poner al final el texto de alerta
    std::snprintf(texto_pct, sizeof(texto_pct), " %.1f%%", porcentaje);  // siempre el % real
    return std::string("[") + barras[indice] + "]" + texto_pct;
    //                    [████████░░] 80.0%   ALERTA
}

// ---------- Inciso 5 ----------
std::string evaluar_estado(double porcentaje) {
    if (porcentaje < 80.0)   return "OK";
    if (porcentaje <= 100.0) return "ALERTA";
    return "EXCEDIDO";
}

// REPORTE
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

    double pct_total = total_presup > 0.0 ? total_gasto / total_presup * 100.0 : 0.0;
    std::printf("-------------------------------------------------------------------"
                "-------------------------------------\n");
    imprimir_linea_presupuesto("TOTAL", total_gasto, total_presup,
                               generar_barra_progreso(pct_total), evaluar_estado(pct_total));
    return 0;
}
