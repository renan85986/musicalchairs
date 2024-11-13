#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>
#include <memory>

constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(0);
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};

class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores) : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void iniciar_rodada() {
        std::cout << "\n------------------------------------------\n";
        std::cout << "Iniciando rodada com " << num_jogadores << " jogadores e " << cadeiras << " cadeiras.\n";
        std::cout << "A música está tocando... 🎵\n\n";
    }

    void parar_musica() {
        cadeira_sem.release(cadeiras);
        std::lock_guard<std::mutex> lock(music_mutex);
        musica_parada = true;
        music_cv.notify_all();
        std::cout << "> A música parou! Os jogadores estão tentando se sentar...\n\n";
    }

    void eliminar_jogador(int jogador_id) {
        std::cout << "Jogador " << jogador_id << " não conseguiu uma cadeira e foi eliminado!\n";
        num_jogadores--;
    }

    bool jogo_acabou() const {
        return num_jogadores <= 1;
    }

    void exibir_estado_rodada(const std::vector<int>& ocupantes) {
        std::cout << "------------------------------------------\n";
        for (size_t i = 0; i < ocupantes.size(); ++i) {
            std::cout << "[Cadeira " << i + 1 << "]: Ocupada por P" << ocupantes[i] << "\n";
        }
        std::cout << "------------------------------------------\n\n";
    }

    void anunciar_vencedor(int jogador_id) {
        std::cout << "🏆 Vencedor: Jogador " << jogador_id << "! Parabéns! 🏆\n";
    }

    void reduzir_cadeiras() {
        if (cadeiras > 1) {
            cadeiras--;
        }
    }

private:
    int num_jogadores;
    int cadeiras;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo) : id(id), jogo(jogo), eliminado(false) {}

    bool tentar_ocupar_cadeira() {
        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock, [] { return musica_parada.load(); });

        if (!eliminado && cadeira_sem.try_acquire()) {
            std::cout << "Jogador " << id << " conseguiu uma cadeira!\n";
            return true;
        } else if (!eliminado) {
            jogo.eliminar_jogador(id);
            eliminado = true;
        }
        return false;
    }

    void joga(std::vector<int>& ocupantes) {
        if (jogo_ativo.load() && !eliminado) {
            if (tentar_ocupar_cadeira()) {
                ocupantes.push_back(id);
            }
        }
    }

    bool is_eliminado() const {
        return eliminado;
    }

    int get_id() const {
        return id;
    }

private:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo) : jogo(jogo) {}

    void iniciar_jogo(std::vector<std::unique_ptr<Jogador>>& jogadores) {
        while (jogo_ativo.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(rand() % 3 + 1));
            jogo.iniciar_rodada();

            std::shuffle(jogadores.begin(), jogadores.end(), std::default_random_engine(std::random_device{}()));

            jogo.parar_musica();

            std::vector<int> ocupantes;
            for (auto& jogador : jogadores) {
                if (!jogador->is_eliminado()) {
                    jogador->joga(ocupantes);
                }
            }
            jogo.exibir_estado_rodada(ocupantes);

            if (jogo.jogo_acabou()) {
                for (auto& jogador : jogadores) {
                    if (!jogador->is_eliminado()) {
                        jogo.anunciar_vencedor(jogador->get_id());
                    }
                }
                jogo_ativo = false;
            } else {
                jogo.reduzir_cadeiras();
            }

            musica_parada = false;
        }
    }

private:
    JogoDasCadeiras& jogo;
};

int main() {
    std::srand(static_cast<unsigned int>(std::time(0)));

    std::cout << "-----------------------------------------------\n";
    std::cout << "Bem-vindo ao Jogo das Cadeiras Concorrente!\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << "O jogo iniciou com " << NUM_JOGADORES << " jogadores e " << NUM_JOGADORES - 1 << " cadeiras\n";

    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::unique_ptr<Jogador>> jogadores;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores.emplace_back(std::make_unique<Jogador>(i, jogo));
    }

    coordenador.iniciar_jogo(jogadores);

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}
