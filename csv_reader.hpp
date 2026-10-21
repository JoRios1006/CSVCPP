// ============================================================
//  csv_reader.hpp
//  Lectura de CSV (mmap + parseo). Todo lo "de bajo nivel" vive
//  en csv_reader.cpp; acá solo se muestra cómo se USA.
// ============================================================
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Una fila = lista de celdas (texto). Es como una lista de str en Python.
// OJO: las celdas son vistas temporales. Si necesitás guardarlas,
// copialas: std::string(celdas[0]).
using Fila = std::vector<std::string_view>;

// Recorre un CSV fila por fila (parecido a: for fila in csv.reader(...)).
//   ruta     : archivo a leer
//   columnas : nombres de las columnas que querés recibir, en ese orden
//   por_fila : función que se llama una vez por cada fila de datos;
//              recibe las celdas pedidas, en el orden de 'columnas'
// Devuelve false si no se pudo abrir el archivo o falta alguna columna.
bool recorrer_csv(const std::string& ruta,
                  const std::vector<std::string_view>& columnas,
                  const std::function<void(const Fila&)>& por_fila);

// Texto -> número. Devuelve false si no es un número válido.
// Ignora espacios internos: "2 8000.00" se lee como 28000.00.
bool a_double(std::string_view texto, double& resultado);
