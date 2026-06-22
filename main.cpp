#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <random>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <algorithm>

using namespace std;

static string nowIso() {
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    stringstream ss;
    ss << put_time(&tmv, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

struct ConfiguracaoDupla {
    string estacao = "EB-27";
    int idDupla = 27;
    double nivelBaixo = 27.0;
    double nivelAlto = 84.0;
    double pressaoAlta = 6.8;
    string falhaObrigatoria = "sensor de nivel travado por 10 ciclos";
    string regraExtra = "bloqueio de partida quando o motor estiver em alerta";
};

class Sensor {
protected:
    string tag;
    string unidade;
    double valor;
    string status;
public:
    Sensor(string tag, string unidade, double inicial) : tag(tag), unidade(unidade), valor(inicial), status("OK") {}
    virtual ~Sensor() = default;
    virtual void atualizar(int ciclo) = 0;
    virtual string tipo() const = 0;
    string getTag() const { return tag; }
    string getUnidade() const { return unidade; }
    double getValor() const { return valor; }
    string getStatus() const { return status; }
    void setStatus(const string& s) { status = s; }
};

class SensorNivel : public Sensor {
    mt19937 gen;
    uniform_real_distribution<double> ruido;
public:
    SensorNivel() : Sensor("LT-101", "%", 52.0), gen(random_device{}()), ruido(-2.5, 3.0) {}
    void atualizar(int ciclo) override {
        if (ciclo >= 18 && ciclo < 28) { status = "TRAVADO"; return; }
        valor += ruido(gen);
        if (ciclo % 9 == 0) valor += 8.0;
        valor = max(5.0, min(96.0, valor));
        status = "OK";
    }
    string tipo() const override { return "nivel"; }
};

class SensorPressao : public Sensor {
    mt19937 gen;
    uniform_real_distribution<double> ruido;
public:
    SensorPressao() : Sensor("PT-201", "bar", 3.7), gen(random_device{}()), ruido(-0.18, 0.25) {}
    void atualizar(int ciclo) override {
        valor += ruido(gen);
        if (ciclo > 35 && ciclo < 45) valor += 0.28;
        valor = max(0.5, min(8.5, valor));
        status = valor > 6.8 ? "ALTO" : "OK";
    }
    string tipo() const override { return "pressao"; }
};

class SensorVazao : public Sensor {
    mt19937 gen;
    uniform_real_distribution<double> ruido;
public:
    SensorVazao() : Sensor("FT-301", "m3/h", 42.0), gen(random_device{}()), ruido(-3.0, 2.8) {}
    void atualizar(int ciclo) override {
        valor += ruido(gen);
        if (ciclo > 25 && ciclo < 34) valor -= 5.0;
        valor = max(0.0, min(75.0, valor));
        status = valor < 18.0 ? "BAIXO" : "OK";
    }
    string tipo() const override { return "vazao"; }
};

class SensorTemperaturaMotor : public Sensor {
    mt19937 gen;
    uniform_real_distribution<double> ruido;
public:
    SensorTemperaturaMotor() : Sensor("TT-401", "C", 54.0), gen(random_device{}()), ruido(-0.6, 1.0) {}
    void atualizar(int ciclo) override {
        valor += ruido(gen);
        if (ciclo > 30 && ciclo < 42) valor += 1.6;
        valor = max(25.0, min(98.0, valor));
        status = valor > 80.0 ? "ALERTA" : "OK";
    }
    string tipo() const override { return "temperatura_motor"; }
};

class Bomba {
private:
    string tag;
    bool ligada;
    bool bloqueada;
    int horasEquivalentes;
public:
    explicit Bomba(string tag) : tag(tag), ligada(false), bloqueada(false), horasEquivalentes(0) {}
    string getTag() const { return tag; }
    bool estaLigada() const { return ligada; }
    bool estaBloqueada() const { return bloqueada; }
    int getHorasEquivalentes() const { return horasEquivalentes; }
    void ligar() { if (bloqueada) throw runtime_error("Bomba bloqueada: " + tag); ligada = true; }
    void desligar() { ligada = false; }
    void bloquear() { bloqueada = true; ligada = false; }
    void desbloquear() { bloqueada = false; }
    void acumularHora() { if (ligada) horasEquivalentes++; }
};

class Alarme {
public:
    string codigo;
    string severidade;
    string mensagem;
    string timestamp;
    Alarme(string c, string s, string m) : codigo(c), severidade(s), mensagem(m), timestamp(nowIso()) {}
};

class RegraControle {
public:
    virtual ~RegraControle() = default;
    virtual void aplicar(class EstacaoBombeamento& estacao) = 0;
    virtual string nome() const = 0;
};

class Comando {
public:
    virtual ~Comando() = default;
    virtual string executar(class EstacaoBombeamento& estacao) = 0;
    virtual string nome() const = 0;
};

class EstacaoBombeamento {
private:
    ConfiguracaoDupla config;
    vector<unique_ptr<Sensor>> sensores;
    vector<Bomba> bombas;
    vector<Alarme> alarmes;
    int ciclo = 0;
public:
    EstacaoBombeamento() {
        sensores.push_back(make_unique<SensorNivel>());
        sensores.push_back(make_unique<SensorPressao>());
        sensores.push_back(make_unique<SensorVazao>());
        sensores.push_back(make_unique<SensorTemperaturaMotor>());
        bombas.emplace_back("P-101A");
        bombas.emplace_back("P-101B");
    }
    ConfiguracaoDupla getConfig() const { return config; }
    int getCiclo() const { return ciclo; }
    vector<unique_ptr<Sensor>>& getSensores() { return sensores; }
    vector<Bomba>& getBombas() { return bombas; }
    vector<Alarme>& getAlarmes() { return alarmes; }
    double valorSensor(const string& tipo) const {
        for (const auto& s : sensores) if (s->tipo() == tipo) return s->getValor();
        throw runtime_error("Sensor nao encontrado: " + tipo);
    }
    string statusSensor(const string& tipo) const {
        for (const auto& s : sensores) if (s->tipo() == tipo) return s->getStatus();
        return "INEXISTENTE";
    }
    void registrarAlarme(const string& codigo, const string& severidade, const string& msg) {
        bool repetido = any_of(alarmes.begin(), alarmes.end(), [&](const Alarme& a){ return a.codigo == codigo && a.mensagem == msg; });
        if (!repetido) alarmes.emplace_back(codigo, severidade, msg);
    }
    void limparAlarmes() { alarmes.clear(); }
    void atualizarSensores() {
        ciclo++;
        limparAlarmes();
        for (auto& s : sensores) s->atualizar(ciclo);
        for (auto& b : bombas) b.acumularHora();
    }
    string estadoBombasJson() const {
        stringstream ss;
        ss << "[";
        for (size_t i=0; i<bombas.size(); ++i) {
            if (i) ss << ",";
            ss << "{\"tag\":\"" << bombas[i].getTag() << "\",\"ligada\":" << (bombas[i].estaLigada()?"true":"false")
               << ",\"bloqueada\":" << (bombas[i].estaBloqueada()?"true":"false")
               << ",\"horas\":" << bombas[i].getHorasEquivalentes() << "}";
        }
        ss << "]";
        return ss.str();
    }
    string alarmesJson() const {
        stringstream ss;
        ss << "[";
        for (size_t i=0; i<alarmes.size(); ++i) {
            if (i) ss << ",";
            ss << "{\"codigo\":\"" << alarmes[i].codigo << "\",\"severidade\":\"" << alarmes[i].severidade
               << "\",\"mensagem\":\"" << alarmes[i].mensagem << "\",\"timestamp\":\"" << alarmes[i].timestamp << "\"}";
        }
        ss << "]";
        return ss.str();
    }
    string pacoteJson() const {
        stringstream ss;
        ss << fixed << setprecision(2);
        ss << "{\"estacao\":\"" << config.estacao << "\",\"ciclo\":" << ciclo << ",\"timestamp\":\"" << nowIso() << "\",";
        ss << "\"assinatura\":{\"id_dupla\":" << config.idDupla << ",\"nivel_baixo\":" << config.nivelBaixo
           << ",\"nivel_alto\":" << config.nivelAlto << ",\"pressao_alta\":" << config.pressaoAlta << "},";
        ss << "\"leituras\":[";
        for (size_t i=0; i<sensores.size(); ++i) {
            if (i) ss << ",";
            const auto& s = sensores[i];
            ss << "{\"tag\":\"" << s->getTag() << "\",\"tipo\":\"" << s->tipo() << "\",\"valor\":" << s->getValor()
               << ",\"unidade\":\"" << s->getUnidade() << "\",\"timestamp\":\"" << nowIso() << "\",\"status\":\"" << s->getStatus() << "\"}";
        }
        ss << "],\"bombas\":" << estadoBombasJson() << ",\"alarmes\":" << alarmesJson() << "}";
        return ss.str();
    }
};

class RegraNivel : public RegraControle {
public:
    void aplicar(EstacaoBombeamento& e) override {
        double nivel = e.valorSensor("nivel");
        auto& bombas = e.getBombas();
        auto cfg = e.getConfig();
        if (nivel >= cfg.nivelAlto) {
            if (!bombas[0].estaBloqueada()) bombas[0].ligar();
            if (!bombas[1].estaBloqueada()) bombas[1].ligar();
            e.registrarAlarme("ALM_NIVEL_ALTO", "ALTA", "Nivel acima do limite operacional");
        } else if (nivel <= cfg.nivelBaixo) {
            for (auto& b : bombas) b.desligar();
            e.registrarAlarme("ALM_NIVEL_BAIXO", "MEDIA", "Nivel baixo: bombas desligadas para protecao");
        }
    }
    string nome() const override { return "Regra de nivel"; }
};

class RegraPressao : public RegraControle {
public:
    void aplicar(EstacaoBombeamento& e) override {
        if (e.valorSensor("pressao") > e.getConfig().pressaoAlta) {
            for (auto& b : e.getBombas()) b.desligar();
            e.registrarAlarme("ALM_PRESSAO_ALTA", "CRITICA", "Pressao alta: desligamento preventivo das bombas");
        }
    }
    string nome() const override { return "Regra de pressao"; }
};

class RegraVazaoBaixa : public RegraControle {
public:
    void aplicar(EstacaoBombeamento& e) override {
        bool algumaLigada = false;
        for (auto& b : e.getBombas()) algumaLigada = algumaLigada || b.estaLigada();
        if (algumaLigada && e.valorSensor("vazao") < 18.0) {
            e.registrarAlarme("ALM_VAZAO_BAIXA", "MEDIA", "Vazao baixa com bomba ligada: possivel obstrucao ou cavitacao");
        }
    }
    string nome() const override { return "Regra de vazao baixa"; }
};

class RegraSensorTravado : public RegraControle {
public:
    void aplicar(EstacaoBombeamento& e) override {
        if (e.statusSensor("nivel") == "TRAVADO") {
            e.registrarAlarme("ALM_SENSOR_TRAVADO", "ALTA", "Falha simulada da dupla: sensor de nivel travado");
        }
    }
    string nome() const override { return "Regra de sensor travado"; }
};

class RegraBloqueioPorMotor : public RegraControle {
public:
    void aplicar(EstacaoBombeamento& e) override {
        if (e.statusSensor("temperatura_motor") == "ALERTA") {
            for (auto& b : e.getBombas()) b.bloquear();
            e.registrarAlarme("ALM_MOTOR_ALERTA", "CRITICA", "Regra extra da dupla: partida bloqueada por motor em alerta");
        }
    }
    string nome() const override { return "Regra extra: bloqueio por motor"; }
};

class LigarBombaA : public Comando {
public:
    string executar(EstacaoBombeamento& e) override { e.getBombas()[0].ligar(); return "P-101A ligada"; }
    string nome() const override { return "LIGAR_BOMBA_A"; }
};
class DesligarBombaA : public Comando {
public:
    string executar(EstacaoBombeamento& e) override { e.getBombas()[0].desligar(); return "P-101A desligada"; }
    string nome() const override { return "DESLIGAR_BOMBA_A"; }
};
class LigarBombaB : public Comando {
public:
    string executar(EstacaoBombeamento& e) override { e.getBombas()[1].ligar(); return "P-101B ligada"; }
    string nome() const override { return "LIGAR_BOMBA_B"; }
};
class ResetarBloqueios : public Comando {
public:
    string executar(EstacaoBombeamento& e) override { for (auto& b : e.getBombas()) b.desbloquear(); return "Bloqueios resetados"; }
    string nome() const override { return "RESETAR_BLOQUEIOS"; }
};

class RepositorioJsonLines {
private:
    string caminho;
public:
    explicit RepositorioJsonLines(string caminho) : caminho(caminho) {}
    void salvar(const string& linha) {
        ofstream arq(caminho, ios::app);
        if (!arq.is_open()) throw runtime_error("Nao foi possivel abrir arquivo JSONL: " + caminho);
        arq << linha << "\n";
    }
};

class FabricaSensores {
public:
    static unique_ptr<Sensor> criar(const string& tipo) {
        if (tipo == "nivel") return make_unique<SensorNivel>();
        if (tipo == "pressao") return make_unique<SensorPressao>();
        if (tipo == "vazao") return make_unique<SensorVazao>();
        if (tipo == "temperatura_motor") return make_unique<SensorTemperaturaMotor>();
        throw invalid_argument("Tipo de sensor desconhecido: " + tipo);
    }
};

int main() {
    try {
        EstacaoBombeamento estacao;
        RepositorioJsonLines repo("../../data/leituras.jsonl");
        vector<unique_ptr<RegraControle>> regras;
        regras.push_back(make_unique<RegraNivel>());
        regras.push_back(make_unique<RegraPressao>());
        regras.push_back(make_unique<RegraVazaoBaixa>());
        regras.push_back(make_unique<RegraSensorTravado>());
        regras.push_back(make_unique<RegraBloqueioPorMotor>());

        cout << "Mini-SCADA dispositivo C++ iniciado. Gravando em data/leituras.jsonl\n";
        for (int i = 0; i < 80; ++i) {
            estacao.atualizarSensores();
            for (auto& r : regras) r->aplicar(estacao);
            string pacote = estacao.pacoteJson();
            repo.salvar(pacote);
            cout << pacote << endl;
            this_thread::sleep_for(chrono::milliseconds(350));
        }
        return 0;
    } catch (const exception& ex) {
        cerr << "Erro no dispositivo: " << ex.what() << endl;
        return 1;
    }
}
