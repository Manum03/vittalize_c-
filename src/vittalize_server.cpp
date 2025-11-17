#include <bits/stdc++.h>
#include <sstream> // Necesario para ostringstream
#include <nlohmann/json.hpp>
#include <crow.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <ctime>       // Para obtener la marca de tiempo
#include <iomanip>     // Para el formato de tiempo
#include <filesystem>  // Para crear el directorio de la base de datos

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

// Se setea en main() usando argv[0] para resolver rutas relativas al ejecutable
static fs::path g_exe_dir;

// Añade cabeceras CORS a cualquier respuesta enviada al frontend
void addCorsHeaders(crow::response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
}

enum class Nivel { NORMAL, VIGILANCIA, MODERADO, ALTO, CRITICO };

struct ResultadoItem {
    string nombre;
    Nivel nivel;
    string explicacion;
};

struct SignosVitales {
    double fc_lpm;     // Frecuencia cardiaca
    double fr_rpm;     // Frecuencia respiratoria
    double temp_c;     // Temperatura
    double spo2_pct;   // Saturación de oxígeno
    double pas_mmhg;   // Presión arterial sistólica
    double peso_kg;
    double talla_m;
};

struct Sintomas {
    bool fiebre_escalofrios = false;
    bool dolor_toracico = false;
    int disnea = 0;             // 0: ninguna, 1: leve, 2: severa
    int dolor_abdominal = 0;    // 0: ninguno, 1: moderado, 2: intenso
    bool cefalea_súbita = false;
    bool palidez_sudoracion = false;
};

// Funciones auxiliares
string nivelToStr(Nivel n) {
    switch (n) {
        case Nivel::NORMAL: return "NORMAL";
        case Nivel::VIGILANCIA: return "VIGILANCIA";
        case Nivel::MODERADO: return "MODERADO";
        case Nivel::ALTO: return "ALTO";
        case Nivel::CRITICO: return "CRITICO";
    }
    return "NORMAL";
}


/*template<typename T>
T leerNumero(const string& prompt, T minv, T maxv) {
    while (true) {
        cout << prompt << " ";
        T v;
        if (cin >> v) {
            if (v >= minv && v <= maxv) return v;
            cout << "   ❌ Valor fuera de rango (" << minv << " - " << maxv << ")\n";
        } else {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "   ❌ Entrada inválida, intenta de nuevo.\n";
        }
    }
}*/

/*bool leerSiNo(const string& prompt) {
    while (true) {
        cout << prompt << " (s/n): ";
        string s; cin >> s;
        for (auto &c : s) c = tolower(c);
        if (s == "s" || s == "si" || s == "sí") return true;
        if (s == "n" || s == "no") return false;
        cout << "   ❌ Responde con 's' o 'n'.\n";
    }
}*/

/*int leerOpcion(const string& prompt, const vector<string>& opciones) {
    cout << prompt << "\n";
    for (size_t i = 0; i < opciones.size(); ++i)
        cout << "  " << i + 1 << ") " << opciones[i] << "\n";
    while (true) {
        cout << "Elige [1-" << opciones.size() << "]: ";
        int op;
        if (cin >> op && op >= 1 && (size_t)op <= opciones.size())
            return op;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "   ❌ Opción inválida.\n";
    }
}*/

// ==== Evaluaciones ====
ResultadoItem evalFC(double fc) {
    if (fc < 60) return {"Frecuencia cardiaca", Nivel::VIGILANCIA, "Bradicardia (<60 lpm)"};
    if (fc > 100) return {"Frecuencia cardiaca", Nivel::ALTO, "Taquicardia (>100 lpm)"};
    return {"Frecuencia cardiaca", Nivel::NORMAL, "Dentro de 60–100 lpm"};
}

ResultadoItem evalFR(double fr) {
    if (fr < 12) return {"Frecuencia respiratoria", Nivel::CRITICO, "FR < 12 rpm (depresión respiratoria)"};
    if (fr > 20) return {"Frecuencia respiratoria", Nivel::MODERADO, "FR > 20 rpm (taquipnea)"};
    return {"Frecuencia respiratoria", Nivel::NORMAL, "12–20 rpm"};
}

ResultadoItem evalTemp(double t) {
    if (t >= 38.5) return {"Temperatura", Nivel::CRITICO, "Fiebre alta (≥38.5 °C)"};
    if (t <= 35.0) return {"Temperatura", Nivel::CRITICO, "Hipotermia (≤35 °C)"};
    if (t >= 37.5) return {"Temperatura", Nivel::VIGILANCIA, "Febrícula (37.5–38.4 °C)"};
    return {"Temperatura", Nivel::NORMAL, "36–37.4 °C"};
}

ResultadoItem evalSpO2(double s) {
    if (s < 90) return {"Saturación O₂", Nivel::CRITICO, "SpO₂ < 90% (hipoxia severa)"};
    if (s < 95) return {"Saturación O₂", Nivel::VIGILANCIA, "SpO₂ 90–94% (vigilar)"};
    return {"Saturación O₂", Nivel::NORMAL, "≥95%"};
}

ResultadoItem evalPAS(double pas) {
    if (pas < 90) return {"Presión sistólica", Nivel::CRITICO, "PAS < 90 mmHg (hipotensión)"};
    if (pas > 140) return {"Presión sistólica", Nivel::MODERADO, "PAS > 140 mmHg (hipertensión leve)"};
    return {"Presión sistólica", Nivel::NORMAL, "90–140 mmHg"};
}

ResultadoItem evalIMC(double peso, double talla) {
    double imc = peso / (talla * talla);
    ostringstream ss; ss << fixed << setprecision(1) << imc;
    if (imc < 18.5) return {"IMC", Nivel::VIGILANCIA, "IMC=" + ss.str() + " (bajo peso)"};
    if (imc > 30) return {"IMC", Nivel::VIGILANCIA, "IMC=" + ss.str() + " (obesidad)"};
    return {"IMC", Nivel::NORMAL, "IMC=" + ss.str()};
}

vector<ResultadoItem> evalSintomas(const Sintomas& s) {
    vector<ResultadoItem> r;
    if (s.fiebre_escalofrios)
        r.push_back({"Fiebre o escalofríos", Nivel::VIGILANCIA, "Sugiere infección"});
    if (s.dolor_toracico && s.disnea >= 1)
        r.push_back({"Dolor torácico + disnea", s.disnea == 2 ? Nivel::CRITICO : Nivel::ALTO, "Posible evento cardiopulmonar"});
    else if (s.dolor_toracico)
        r.push_back({"Dolor torácico", Nivel::ALTO, "Debe evaluarse"});
    if (s.disnea == 1)
        r.push_back({"Disnea leve", Nivel::VIGILANCIA, "Vigilar"});
    if (s.disnea == 2)
        r.push_back({"Disnea severa", Nivel::CRITICO, "Urgencia respiratoria"});
    if (s.dolor_abdominal == 1)
        r.push_back({"Dolor abdominal moderado", Nivel::VIGILANCIA, "Requiere control"});
    if (s.dolor_abdominal == 2)
        r.push_back({"Dolor abdominal intenso", Nivel::CRITICO, "Sospecha abdomen agudo"});
    if (s.cefalea_súbita)
        r.push_back({"Cefalea súbita", Nivel::ALTO, "Sospecha neurológica"});
    if (s.palidez_sudoracion)
        r.push_back({"Palidez con sudoración", Nivel::CRITICO, "Posible shock o hipoglucemia"});
    return r;
}

Nivel nivelGlobal(const vector<ResultadoItem>& v) {
    Nivel n = Nivel::NORMAL;
    for (auto& i : v)
        if ((int)i.nivel > (int)n) n = i.nivel;
    return n;
}

json resultadoAJson(const vector<ResultadoItem>& v) {
    json j_items = json::array();
    Nivel nivel_global = nivelGlobal(v);

    // Convertir cada item de resultado a un objeto JSON
    for (const auto& item : v) {
        j_items.push_back({
            {"nombre", item.nombre},
            {"nivel", nivelToStr(item.nivel)},
            {"explicacion", item.explicacion}
        });
    }

    // Estructura final que el frontend espera
    return {
        {"nivel_global", nivelToStr(nivel_global)},
        {"items", j_items}
    };
}

// NUEVA FUNCIÓN: Mapea la entrada JSON del frontend a los structs C++.
pair<SignosVitales, Sintomas> deserializarEntrada(const json& data) {
    SignosVitales s{};
    Sintomas sym{};

    // --- Mapeo de Signos Vitales ---
    const auto& j_signos = data.at("signos");
    s.fc_lpm = j_signos.at("fc_lpm").get<double>();
    s.fr_rpm = j_signos.at("fr_rpm").get<double>();
    s.temp_c = j_signos.at("temp_c").get<double>();
    s.spo2_pct = j_signos.at("spo2_pct").get<double>();
    s.pas_mmhg = j_signos.at("pas_mmhg").get<double>();
    s.peso_kg = j_signos.at("peso_kg").get<double>();
    s.talla_m = j_signos.at("talla_m").get<double>();

    // --- Mapeo de Síntomas ---
    const auto& j_sintomas = data.at("sintomas");
    sym.fiebre_escalofrios = j_sintomas.at("fiebre_escalofrios").get<bool>();
    sym.dolor_toracico = j_sintomas.at("dolor_toracico").get<bool>();
    // Los valores de disnea y dolor_abdominal ya son int (0, 1, 2)
    sym.disnea = j_sintomas.at("disnea").get<int>(); 
    sym.dolor_abdominal = j_sintomas.at("dolor_abdominal").get<int>();
    sym.cefalea_súbita = j_sintomas.at("cefalea_subita").get<bool>();
    sym.palidez_sudoracion = j_sintomas.at("palidez_sudoracion").get<bool>();

    return {s, sym};
}

/*void imprimirReporte(const vector<ResultadoItem>& v) {
    cout << "\n===== REPORTE DE EVALUACIÓN =====\n";
    for (auto& i : v)
        cout << "• " << i.nombre << ": " << nivelToStr(i.nivel)
             << "\n  " << i.explicacion << "\n";
    cout << "----------------------------------\n";
    cout << "🔎 Nivel Global: " << nivelToStr(nivelGlobal(v)) << "\n";
    cout << "==================================\n\n";
}*/

// === Entrada guiada ===
/*void ingresarSignos(SignosVitales& s) {
    cout << "\n👉 Ingreso de signos vitales:\n";
    s.fc_lpm = leerNumero<double>("Frecuencia cardiaca (lpm) [20–220]:", 20, 220);
    s.fr_rpm = leerNumero<double>("Frecuencia respiratoria (rpm) [4–60]:", 4, 60);
    s.temp_c = leerNumero<double>("Temperatura (°C) [30–43]:", 30, 43);
    s.spo2_pct = leerNumero<double>("Saturación O₂ (%) [50–100]:", 50, 100);
    s.pas_mmhg = leerNumero<double>("Presión sistólica (mmHg) [50–250]:", 50, 250);
    s.peso_kg = leerNumero<double>("Peso (kg) [10–400]:", 10, 400);
    s.talla_m = leerNumero<double>("Talla (m) [0.5–2.5]:", 0.5, 2.5);
}*/

/*void ingresarSintomas(Sintomas& s) {
    cout << "\n👉 Ingreso de síntomas:\n";
    s.fiebre_escalofrios = leerSiNo("¿Tiene fiebre o escalofríos?");
    s.dolor_toracico = leerSiNo("¿Dolor en el pecho?");
    s.disnea = leerOpcion("¿Presenta dificultad para respirar?", {"No", "Leve", "Severa"}) - 1;
    s.dolor_abdominal = leerOpcion("Dolor abdominal:", {"Ninguno", "Moderado", "Intenso"}) - 1;
    s.cefalea_súbita = leerSiNo("¿Dolor de cabeza intenso de inicio súbito?");
    s.palidez_sudoracion = leerSiNo("¿Palidez con sudoración?");
}*/

vector<ResultadoItem> evaluar(const SignosVitales& s, const Sintomas& sym) {
    vector<ResultadoItem> r = {
        evalFC(s.fc_lpm),
        evalFR(s.fr_rpm),
        evalTemp(s.temp_c),
        evalSpO2(s.spo2_pct),
        evalPAS(s.pas_mmhg),
        evalIMC(s.peso_kg, s.talla_m)
    };
    auto extra = evalSintomas(sym);
    r.insert(r.end(), extra.begin(), extra.end());
    return r;
}

// Constantes para la base de datos (si no existe, se crea en build/data)
const fs::path DB_FILE_PATH = fs::path("data") / "vittalize.sqlite";
const string DB_FILE = DB_FILE_PATH.string();

// Devuelve la ruta absoluta de la carpeta web (soporta ejecutar desde raíz o desde build/)
fs::path webRoot() {
    static const fs::path cached = []{
        vector<fs::path> candidates = {
            fs::current_path() / "web",
            fs::current_path().parent_path() / "web",
            g_exe_dir / "web",
            g_exe_dir.parent_path() / "web"
        };
        for (auto& p : candidates) {
            if (fs::exists(p / "index.html"))
                return fs::weakly_canonical(p);
        }
        // Fallback: asume ./web aunque no exista (para no devolver ruta vacía)
        return fs::current_path() / "web";
    }();
    return cached;
}

// Devuelve un MIME simple según extensión
string guessMime(const fs::path& p) {
    auto ext = p.extension().string();
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".js") return "application/javascript";
    if (ext == ".css") return "text/css";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

// Carga un archivo estático y devuelve el contenido y el código HTTP.
struct StaticPayload {
    int code;
    string body;
    string mime;
};

StaticPayload loadStatic(const fs::path& file_path) {
    std::cout << "[STATIC] solicitando " << file_path << std::endl;
    if (!fs::exists(file_path)) {
        std::cout << "[STATIC] 404 no encontrado\n";
        return {404, "Archivo no encontrado", "text/plain"};
    }
    try {
        std::ifstream f(file_path, std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        return {200, ss.str(), guessMime(file_path)};
    } catch (...) {
        return {500, "No se pudo leer el archivo", "text/plain"};
    }
}

// Sirve un archivo estático devolviendo 404 si no existe (para rutas con response&).
void serveStatic(crow::response& res, const fs::path& file_path) {
    auto payload = loadStatic(file_path);
    res.code = payload.code;
    res.body = std::move(payload.body);
    if (!payload.mime.empty())
        res.add_header("Content-Type", payload.mime);
    res.end();
}

void asegurarDirectorioDB() {
    if (!DB_FILE_PATH.parent_path().empty()) {
        fs::create_directories(DB_FILE_PATH.parent_path());
    }
}

/**
 * @brief Inicializa la base de datos: crea la tabla EVALUACIONES si no existe.
 */
void inicializarDB() {
    try {
        asegurarDirectorioDB();
        // Se crea un objeto Database. Si el archivo no existe, se crea.
        SQLite::Database db(DB_FILE, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        // Recupera versión de SQLite desde la propia base de datos
        string sqlite_version = db.execAndGet("SELECT sqlite_version()").getString();
        cout << "Base de datos abierta exitosamente. Versión SQLite: " << sqlite_version << endl;

        // Comando SQL para crear la tabla
        string sql = R"(
            CREATE TABLE IF NOT EXISTS EVALUACIONES (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                fecha TEXT NOT NULL,
                nivel_global TEXT NOT NULL,
                fc_lpm REAL,
                fr_rpm REAL,
                temp_c REAL,
                spo2_pct REAL,
                pas_mmhg REAL,
                peso_kg REAL,
                talla_m REAL,
                sintomas_fiebre INTEGER,
                sintomas_toracico INTEGER,
                sintomas_disnea INTEGER,
                sintomas_abdominal INTEGER,
                sintomas_cefalea INTEGER,
                sintomas_palidez INTEGER,
                reporte_json TEXT
            );
        )";
        
        db.exec(sql);
        cout << "Tabla EVALUACIONES verificada/creada." << endl;

    } catch (exception& e) {
        cerr << "❌ Error al inicializar la base de datos: " << e.what() << endl;
        // Relanza la excepción para detener el servidor si la BD no puede iniciar
        throw; 
    }
}

/**
 * @brief Guarda una evaluación en la base de datos.
 */
void guardarEvaluacion(const SignosVitales& s, const Sintomas& sym, 
                       const string& nivel_global, const json& reporte_json) 
{
    try {
        // Asegurarse de que la carpeta 'data' exista antes de abrir la BD.
        // En un entorno de producción, esto debería manejarse con una verificación de sistema de archivos.
        asegurarDirectorioDB();
        SQLite::Database db(DB_FILE, SQLite::OPEN_READWRITE);

        // Obtener la marca de tiempo actual
        time_t t = time(nullptr);
        tm* now = localtime(&t);
        stringstream date_ss;
        // Formato de tiempo ISO 8601
        date_ss << put_time(now, "%Y-%m-%d %H:%M:%S"); 
        string current_time = date_ss.str();

        // Usamos una sentencia preparada para seguridad y rendimiento
        SQLite::Statement query(db, "INSERT INTO EVALUACIONES VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        // Bindear los parámetros (la posición del índice comienza en 1 en SQLiteCpp)
        query.bind(1, current_time);
        query.bind(2, nivel_global);
        
        // --- Signos Vitales (3 - 9) ---
        query.bind(3, s.fc_lpm);
        query.bind(4, s.fr_rpm);
        query.bind(5, s.temp_c);
        query.bind(6, s.spo2_pct);
        query.bind(7, s.pas_mmhg);
        query.bind(8, s.peso_kg);
        query.bind(9, s.talla_m);

        // --- Síntomas (10 - 15) ---
        query.bind(10, (int)sym.fiebre_escalofrios);
        query.bind(11, (int)sym.dolor_toracico);
        query.bind(12, sym.disnea);
        query.bind(13, sym.dolor_abdominal);
        query.bind(14, (int)sym.cefalea_súbita);
        query.bind(15, (int)sym.palidez_sudoracion);

        // 16. reporte_json (JSON completo del resultado)
        query.bind(16, reporte_json.dump());

        query.exec();
        cout << "✅ Evaluación guardada exitosamente en la BD." << endl;

    } catch (exception& e) {
        cerr << "❌ Error al guardar la evaluación en la BD: " << e.what() << endl;
        // El servidor no se detiene, solo registra la falla de persistencia.
    }
}

// Recupera las evaluaciones más recientes (limit default 20)
json obtenerEvaluaciones(size_t limit = 20) {
    json rows = json::array();
    try {
        asegurarDirectorioDB();
        SQLite::Database db(DB_FILE, SQLite::OPEN_READWRITE);

        SQLite::Statement query(db, "SELECT id, fecha, nivel_global, reporte_json FROM EVALUACIONES ORDER BY id DESC LIMIT ?");
        query.bind(1, static_cast<int>(limit));

        while (query.executeStep()) {
            json row;
            row["id"] = query.getColumn(0).getInt();
            row["fecha"] = query.getColumn(1).getString();
            row["nivel_global"] = query.getColumn(2).getString();

            try {
                auto rep = json::parse(query.getColumn(3).getString());
                row["reporte"] = rep;
            } catch (...) {
                row["reporte"] = nullptr;
            }
            rows.push_back(row);
        }
    } catch (exception& e) {
        cerr << "❌ Error al leer evaluaciones: " << e.what() << endl;
    }
    return rows;
}


// === Programa principal - Modo Servidor ===
int main(int argc, char* argv[]) {
    // Guardamos la carpeta del ejecutable para resolver rutas estáticas
    if (argc > 0) {
        try {
            g_exe_dir = fs::weakly_canonical(fs::path(argv[0])).parent_path();
        } catch (...) {
            g_exe_dir = fs::current_path();
        }
    } else {
        g_exe_dir = fs::current_path();
    }

    // Es buena práctica inicializar la BD antes de iniciar el servidor web.
    inicializarDB(); 

    crow::SimpleApp app; 

    cout << "=============================================\n";
    cout << "   🩺 VITTALIZE C++ SERVER - API REST\n";
    cout << "=============================================\n";

    // Ruta raíz: entrega el index.html desde ../web
    CROW_ROUTE(app, "/")
    ([&](){
        auto root = webRoot();
        auto payload = loadStatic(root / "index.html");
        crow::response res(payload.code);
        if (!payload.mime.empty())
            res.add_header("Content-Type", payload.mime);
        res.body = std::move(payload.body);
        return res;
    });

    // Archivos estáticos explícitos
    CROW_ROUTE(app, "/script.js")
    ([&](){
        auto root = webRoot();
        auto payload = loadStatic(root / "script.js");
        crow::response res(payload.code);
        if (!payload.mime.empty())
            res.add_header("Content-Type", payload.mime);
        res.body = std::move(payload.body);
        return res;
    });
    CROW_ROUTE(app, "/style.css")
    ([&](){
        auto root = webRoot();
        auto payload = loadStatic(root / "style.css");
        crow::response res(payload.code);
        if (!payload.mime.empty())
            res.add_header("Content-Type", payload.mime);
        res.body = std::move(payload.body);
        return res;
    });

    // [CORRECCIÓN DE CONFLICTO FINAL]: Consolidación de todos los métodos en una única ruta.
    // **Se elimina .methods(...) para evitar el error de runtime 'handler already exists'.**
    CROW_ROUTE(app, "/api/evaluar")
    .methods(crow::HTTPMethod::GET, crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)
    ([&](const crow::request& req) -> crow::response { // ÚNICO handler para la ruta
        
        // 1. Lógica para OPTIONS (Preflight CORS)
        if (req.method == crow::HTTPMethod::OPTIONS) {
            crow::response res(204);
            addCorsHeaders(res);
            return std::move(res);
        }
        
        // 2. Lógica para GET (Respuesta informativa)
        if (req.method == crow::HTTPMethod::GET) {
            json info = {
                {"message", "Usa POST en /api/evaluar para enviar los datos en JSON."}
            };
            crow::response res(200, info.dump());
            addCorsHeaders(res);
            return std::move(res);
        }

        // 3. Lógica para POST (Evaluación principal)
        if (req.method == crow::HTTPMethod::POST) {
            try {
                // 3. Parsear y Deserializar
                json input_json = json::parse(req.body); 
                auto [s, sym] = deserializarEntrada(input_json); 

                // 4. Ejecutar la lógica de evaluación (El núcleo C++)
                vector<ResultadoItem> resultados = evaluar(s, sym); 
                string nivel_global_str = nivelToStr(nivelGlobal(resultados));
                
                // 5. Convertir resultados a JSON para la respuesta y la BD
                json response_json = resultadoAJson(resultados);

                // 6. GUARDAR en la Base de Datos (Persistencia)
                guardarEvaluacion(s, sym, nivel_global_str, response_json);

                // 7. Devolver la respuesta JSON al cliente (Frontend)
                crow::response res(response_json.dump());
                addCorsHeaders(res);
                return std::move(res); // Usar std::move

            } catch (const json::exception& e) {
                // Error específico de Crow/JSON (JSON malformado)
                json error_response = {
                    {"error", "JSON de entrada inválido o malformado."}, 
                    {"details", e.what()}
                };
                crow::response res(400, error_response.dump());
                addCorsHeaders(res);
                return std::move(res);

            } catch (const exception& e) {
                // Error de lógica o de Base de Datos
                json error_response = {
                    {"error", "Falla en el procesamiento de la evaluación."}, 
                    {"details", e.what()}
                };
                crow::response res(400, error_response.dump());
                addCorsHeaders(res);
                return std::move(res);
            }
        }
        
        // Fallback: Método no permitido (ej. PUT, DELETE)
        return crow::response(405); 
    });

    // Endpoint para listar evaluaciones previas
    CROW_ROUTE(app, "/api/evaluaciones")
    .methods(crow::HTTPMethod::GET)
    ([&](const crow::request& req) {
        size_t limit = 20;
        if (req.url_params.get("limit")) {
            try {
                limit = std::stoul(req.url_params.get("limit"));
            } catch (...) {}
        }
        auto data = obtenerEvaluaciones(limit);
        crow::response res(data.dump());
        addCorsHeaders(res);
        res.add_header("Content-Type", "application/json");
        return res;
    });


    // 8. Iniciar el servidor
    // Usamos el puerto 18080 
    cout << "✅ Servidor Vittalize iniciado en el puerto 18080. Escuchando peticiones POST en /api/evaluar" << endl;
    
    app.port(18080)
       .multithreaded()
       .run();

    return 0;
}
