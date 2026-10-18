#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <numeric>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <charconv> 
int main(void){
const std::string csv_path = "./test_datasets/3MINDIA_minute.csv";
off64_t FD = open64(csv_path.c_str(), O_RDONLY);
struct stat64 st;
fstat64(FD, &st);
void* PTR = mmap64(NULL, st.st_size, PROT_READ, MAP_PRIVATE, FD, 0);
madvise(PTR, st.st_size, MADV_SEQUENTIAL);
madvise(PTR, st.st_size, MADV_WILLNEED);
madvise(PTR, st.st_size, MADV_POPULATE_READ);
char *file = static_cast<char*>(PTR);
std::vector<std::string_view> keys;
const char* start = file;
while (*file != '\0' && *file != '\n') {
    if (*file == ',') {
        keys.emplace_back(start, file - start);
        start = file + 1;
    }
    file++;
}
keys.emplace_back(start, file - start);
if (*file == '\n') {
    file++;
}

size_t num_cols = keys.size();

std::unordered_map<std::string_view, std::vector<float>> column_map;
std::vector<std::vector<float>*> col_ptrs(num_cols);

// Estimación matemática rápida para pre-reservar memoria y evitar realocaciones.
// Ajusta el '50' según el ancho promedio en bytes que tengan las filas de tu CSV.
size_t estimated_rows = st.st_size / 50;

for (size_t i = 0; i < num_cols; ++i) {
    // Inicializa el vector dentro del mapa
    column_map[keys[i]] = std::vector<float>();
    column_map[keys[i]].reserve(estimated_rows);
    
    // Almacena un puntero al vector para acceder sin usar la clave hash
    col_ptrs[i] = &column_map[keys[i]];
}

while (*file != '\0') {
    for (size_t i = 0; i < num_cols; ++i) {
        start = file;
        
        while (*file != '\0' && *file != ',' && *file != '\n') {
            file++;
        }
        
        float value = 0.0f;
        // Solo intentamos parsear si la celda no está vacía
        if (start != file) {
            std::from_chars(start, file, value);
        }
        
        // Empuja el valor flotante usando el puntero directo al vector de la columna
        col_ptrs[i]->push_back(value);
        
        if (*file == ',' || *file == '\n') {
            file++;
        }
    }
}// Depuración: Imprimir todas las claves registradas en el mapa
std::cout << "\n--- CLAVES EN EL MAPA ---\n";
for (const auto& [clave, valores] : column_map) {
    std::cout << "Nombre de columna: [" << clave << "] -> Tamaño: " << valores.size() << '\n';
}
std::cout << "-------------------------\n\n";


const char * COL = "volume";
const std::vector<float>& volume = column_map[COL];

// 2. Proteger la división por cero
if (volume.empty()) {
    std::cout << "Error: La columna [" << COL << "] está vacía o no existe en el mapa.\n";
} else {
    // 3. Usar 0.0f para acumular conservando los decimales (coma flotante)
    float suma = std::accumulate(volume.begin(), volume.end(), 0.0f);
    float promedio = suma / volume.size();
    
    std::cout << "Promedio: " << promedio << '\n';
}

// std::fwrite(file, 1, st.st_size, stdout);
munmap(PTR, st.st_size);
return 0;  
}
